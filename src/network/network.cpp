#include "network/network.h"
#include "utils/config.h"
#include "utils/serializer.h"
#include "utils/utils.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

NetworkNode::NetworkNode(Blockchain& chain, const std::string& host, int port)
    : chain_(chain), host_(host), port_(port), serverSock_(-1), running_(false)
{}

NetworkNode::~NetworkNode() {
    running_ = false;
    if (serverSock_ >= 0) { shutdown(serverSock_, SHUT_RDWR); close(serverSock_); }
}

// ── Socket helpers ────────────────────────────────────────────────────────────

bool NetworkNode::sendLine(int sockfd, const std::string& line) {
    std::string msg = line + "\n";
    size_t total = 0;
    while (total < msg.size()) {
        ssize_t n = ::send(sockfd, msg.c_str() + total, msg.size() - total, MSG_NOSIGNAL);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool NetworkNode::recvLine(int sockfd, std::string& out) {
    out.clear();
    char c;
    while (true) {
        ssize_t n = ::recv(sockfd, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') return true;
        if (c != '\r') out += c;
        if (out.size() > 4 * 1024 * 1024) return false; // 4MB cap
    }
}

// ── Duplicate message prevention ─────────────────────────────────────────────

bool NetworkNode::markSeen(const std::string& msgHash) {
    std::lock_guard<std::mutex> lk(seenMutex_);
    if (seenMessages_.count(msgHash)) return false; // already seen
    seenMessages_.insert(msgHash);
    // Keep set bounded — evict oldest (just clear at 10k — simple approach)
    if (seenMessages_.size() > 10000) seenMessages_.clear();
    return true; // first time seen
}

// ── Server ────────────────────────────────────────────────────────────────────

void NetworkNode::start() {
    serverSock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock_ < 0) { std::cerr << "[NET] Cannot create socket\n"; return; }

    int opt = 1;
    setsockopt(serverSock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[NET] Bind failed: " << strerror(errno) << "\n";
        close(serverSock_); serverSock_ = -1; return;
    }
    listen(serverSock_, 20);
    running_ = true;

    serverThread_ = std::thread(&NetworkNode::acceptLoop, this);
    serverThread_.detach();

    pingThread_ = std::thread(&NetworkNode::pingLoop, this);
    pingThread_.detach();

    std::cout << "[NET] Listening on " << host_ << ":" << port_ << "\n";
}

void NetworkNode::acceptLoop() {
    while (running_) {
        sockaddr_in ca{}; socklen_t len = sizeof(ca);
        int csock = accept(serverSock_, (sockaddr*)&ca, &len);
        if (csock < 0) { if (running_) std::cerr << "[NET] Accept error\n"; break; }

        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ca.sin_addr, buf, sizeof(buf));
        std::string rid = std::string(buf) + ":" + std::to_string(ntohs(ca.sin_port));

        std::cout << "[NET] Incoming: " << rid << "\n";
        std::thread([this, csock, rid]() {
            doHandshake(csock);
            handlePeer(csock, rid);
        }).detach();
    }
}

void NetworkNode::pingLoop() {
    while (running_) {
        sleep(PEER_TIMEOUT_SEC / 2);
        if (!running_) break;

        std::lock_guard<std::mutex> lk(peerMutex_);
        long now = (long)time(0);
        for (auto& p : peers_) {
            if (p.sockfd < 0) continue;
            // Send ping
            Message ping{MsgType::PING, nodeId(), ""};
            sendLine(p.sockfd, ping.serialize());
            // Disconnect stale peers
            if (now - p.lastSeen > PEER_TIMEOUT_SEC * 2) {
                std::cout << "[NET] Peer timeout: " << p.id() << "\n";
                close(p.sockfd);
                p.sockfd = -1;
            }
        }
    }
}

// ── Outgoing connection ───────────────────────────────────────────────────────

bool NetworkNode::connectToPeer(const std::string& host, int port) {
    {
        std::lock_guard<std::mutex> lk(peerMutex_);
        for (auto& p : peers_)
            if (p.host == host && p.port == port && p.sockfd >= 0) {
                std::cout << "[NET] Already connected to " << host << ":" << port << "\n";
                return true;
            }
        if ((int)peers_.size() >= MAX_PEERS) {
            std::cerr << "[NET] Max peers reached\n"; return false;
        }
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    addrinfo hints{}, *res;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        std::cerr << "[NET] Cannot resolve " << host << "\n"; close(sock); return false;
    }
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        std::cerr << "[NET] Cannot connect to " << host << ":" << port
                  << " — " << strerror(errno) << "\n";
        freeaddrinfo(res); close(sock); return false;
    }
    freeaddrinfo(res);

    {
        std::lock_guard<std::mutex> lk(peerMutex_);
        peers_.push_back({host, port, sock, (long)time(0)});
    }
    std::cout << "[NET] Connected to " << host << ":" << port << "\n";

    std::thread([this, sock, host, port]() {
        doHandshake(sock);
        handlePeer(sock, host + ":" + std::to_string(port));
    }).detach();
    return true;
}

// ── Handshake ─────────────────────────────────────────────────────────────────

void NetworkNode::doHandshake(int sockfd) {
    std::string data = std::to_string(chain_.chainLength()) + ":" + chain_.getLatestHash();
    Message m{MsgType::HANDSHAKE, nodeId(), data};
    sendLine(sockfd, m.serialize());
}

// ── Peer receive loop ─────────────────────────────────────────────────────────

void NetworkNode::handlePeer(int sockfd, std::string remoteId) {
    std::string line;
    while (running_ && recvLine(sockfd, line)) {
        if (line.empty()) continue;
        // Update lastSeen
        {
            std::lock_guard<std::mutex> lk(peerMutex_);
            for (auto& p : peers_)
                if (p.sockfd == sockfd) { p.lastSeen = (long)time(0); break; }
        }
        Message msg = Message::parse(line);
        if (msg.type != MsgType::UNKNOWN)
            handleMessage(msg, sockfd);
    }
    std::cout << "[NET] Disconnected: " << remoteId << "\n";
    {
        std::lock_guard<std::mutex> lk(peerMutex_);
        for (auto& p : peers_) if (p.sockfd == sockfd) { p.sockfd = -1; break; }
    }
    close(sockfd);
}

// ── Message routing ───────────────────────────────────────────────────────────

void NetworkNode::handleMessage(const Message& msg, int replySock) {

    // Duplicate suppression for broadcast messages
    if (msg.type == MsgType::TX || msg.type == MsgType::BLOCK) {
        std::string msgHash = sha256(msg.serialize());
        if (!markSeen(msgHash)) return; // already processed — drop
    }

    switch (msg.type) {

    case MsgType::HANDSHAKE: {
        size_t sep = msg.data.find(':');
        if (sep == std::string::npos) break;
        int theirLen = std::stoi(msg.data.substr(0, sep));
        if (theirLen > chain_.chainLength()) {
            std::cout << "[NET] Peer has longer chain (" << theirLen
                      << " vs " << chain_.chainLength() << ") — syncing\n";
            sendLine(replySock, Message{MsgType::CHAIN_REQUEST, nodeId(), ""}.serialize());
        }
        // Also request peer list for peer discovery
        sendLine(replySock, Message{MsgType::PEERS_REQUEST, nodeId(), ""}.serialize());
        break;
    }

    case MsgType::PING:
        sendLine(replySock, Message{MsgType::PONG, nodeId(), ""}.serialize());
        break;

    case MsgType::PONG:
        break;

    case MsgType::TX: {
        try {
            Transaction tx = Serializer::strToTx(msg.data);
            if (chain_.addTransaction(tx)) {
                std::cout << "[NET] TX from " << msg.senderId << "\n";
                // Relay to other peers (flood propagation)
                broadcast(msg);
            }
        } catch (...) { std::cerr << "[NET] Bad TX\n"; }
        break;
    }

    case MsgType::BLOCK: {
        try {
            Block b = Serializer::strToBlock(msg.data);
            if (chain_.addBlock(b)) {
                std::cout << "[NET] Block #" << b.index << " from " << msg.senderId << "\n";
                // Relay block to other peers
                broadcast(msg);
            }
        } catch (...) { std::cerr << "[NET] Bad BLOCK\n"; }
        break;
    }

    case MsgType::CHAIN_REQUEST: {
        std::string chainData = Serializer::chainToStr(chain_.getChain());
        sendLine(replySock, Message{MsgType::CHAIN_RESPONSE, nodeId(), chainData}.serialize());
        std::cout << "[NET] Sent chain (" << chain_.chainLength()
                  << " blocks) to " << msg.senderId << "\n";
        break;
    }

    case MsgType::CHAIN_RESPONSE: {
        try {
            auto theirChain = Serializer::strToChain(msg.data);
            if (chain_.replaceChainIfBetter(theirChain))
                std::cout << "[NET] Chain replaced: " << theirChain.size() << " blocks\n";
        } catch (...) { std::cerr << "[NET] Bad CHAIN_RESPONSE\n"; }
        break;
    }

    case MsgType::PEERS_REQUEST: {
        // Respond with our known peer list
        std::string peerList;
        {
            std::lock_guard<std::mutex> lk(peerMutex_);
            for (auto& p : peers_) {
                if (p.sockfd >= 0) {
                    if (!peerList.empty()) peerList += ",";
                    peerList += p.host + ":" + std::to_string(p.port);
                }
            }
        }
        sendLine(replySock, Message{MsgType::PEERS_RESPONSE, nodeId(), peerList}.serialize());
        break;
    }

    case MsgType::PEERS_RESPONSE: {
        // Parse CSV peer list and connect to unknown peers
        if (msg.data.empty()) break;
        std::istringstream ss(msg.data);
        std::string entry;
        while (std::getline(ss, entry, ',')) {
            size_t colon = entry.rfind(':');
            if (colon == std::string::npos) continue;
            std::string h = entry.substr(0, colon);
            int p = std::stoi(entry.substr(colon + 1));
            // Don't connect to ourselves
            if (h == host_ && p == port_) continue;
            // Connect if not already connected
            connectToPeer(h, p);
        }
        break;
    }

    case MsgType::MEMPOOL_REQ: {
        auto txns = chain_.getMempoolSnapshot();
        std::string data;
        for (auto& tx : txns) {
            if (!data.empty()) data += "~";
            data += Serializer::txToStr(tx);
        }
        sendLine(replySock, Message{MsgType::MEMPOOL_RESP, nodeId(), data}.serialize());
        break;
    }

    case MsgType::MEMPOOL_RESP: {
        if (msg.data.empty()) break;
        std::istringstream ss(msg.data);
        std::string txStr;
        int added = 0;
        while (std::getline(ss, txStr, '~')) {
            try {
                Transaction tx = Serializer::strToTx(txStr);
                if (chain_.addTransaction(tx)) ++added;
            } catch (...) {}
        }
        if (added > 0)
            std::cout << "[NET] Synced " << added << " mempool TXs from "
                      << msg.senderId << "\n";
        break;
    }

    default: break;
    }
}

// ── Public interface ──────────────────────────────────────────────────────────

void NetworkNode::broadcast(const Message& msg) {
    std::lock_guard<std::mutex> lk(peerMutex_);
    std::string line = msg.serialize();
    for (auto& p : peers_)
        if (p.sockfd >= 0) sendLine(p.sockfd, line);
}

void NetworkNode::sendToPeer(const std::string& peerId, const Message& msg) {
    std::lock_guard<std::mutex> lk(peerMutex_);
    for (auto& p : peers_)
        if (p.id() == peerId && p.sockfd >= 0) { sendLine(p.sockfd, msg.serialize()); return; }
}

std::vector<std::string> NetworkNode::getPeerIds() const {
    std::lock_guard<std::mutex> lk(peerMutex_);
    std::vector<std::string> ids;
    for (auto& p : peers_) if (p.sockfd >= 0) ids.push_back(p.id());
    return ids;
}

int NetworkNode::peerCount() const {
    std::lock_guard<std::mutex> lk(peerMutex_);
    int n = 0;
    for (auto& p : peers_) if (p.sockfd >= 0) ++n;
    return n;
}

void NetworkNode::discoverPeers() {
    broadcast(Message{MsgType::PEERS_REQUEST, nodeId(), ""});
    std::cout << "[NET] Peer discovery broadcast sent\n";
}

void NetworkNode::syncMempool() {
    broadcast(Message{MsgType::MEMPOOL_REQ, nodeId(), ""});
    std::cout << "[NET] Mempool sync requested\n";
}