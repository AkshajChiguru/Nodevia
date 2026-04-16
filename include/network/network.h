#pragma once
#include "network/message.h"
#include "core/blockchain.h"
#include <string>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>

struct Peer {
    std::string host;
    int         port;
    int         sockfd;
    long        lastSeen;  // timestamp of last message
    std::string id() const { return host + ":" + std::to_string(port); }
};

class NetworkNode {
public:
    NetworkNode(Blockchain& chain, const std::string& host, int port);
    ~NetworkNode();

    void start();
    bool connectToPeer(const std::string& host, int port);
    void broadcast(const Message& msg);
    void sendToPeer(const std::string& peerId, const Message& msg);

    std::vector<std::string> getPeerIds() const;
    std::string nodeId() const { return host_ + ":" + std::to_string(port_); }
    int  peerCount() const;

    // Request peer lists from all connected peers (peer discovery)
    void discoverPeers();

    // Sync mempool from all peers
    void syncMempool();

private:
    Blockchain&  chain_;
    std::string  host_;
    int          port_;
    int          serverSock_;
    std::atomic<bool> running_;

    mutable std::mutex  peerMutex_;
    std::vector<Peer>   peers_;

    // Seen message hashes — prevents rebroadcasting duplicates
    mutable std::mutex  seenMutex_;
    std::set<std::string> seenMessages_;

    std::thread serverThread_;
    std::thread pingThread_;

    void acceptLoop();
    void pingLoop();           // keepalive + timeout cleanup
    void handlePeer(int sockfd, std::string remoteId);
    void handleMessage(const Message& msg, int replySock);
    void doHandshake(int sockfd);

    // Returns false if this message was already seen (duplicate)
    bool markSeen(const std::string& msgHash);

    static bool sendLine(int sockfd, const std::string& line);
    static bool recvLine(int sockfd, std::string& out);
};