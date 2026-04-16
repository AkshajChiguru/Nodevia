#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <ctime>
#include "core/blockchain.h"
#include "wallet/wallet.h"
#include "network/seed.h"
#include "network/network.h"
#include "utils/serializer.h"
#include "utils/config.h"

std::vector<Wallet> wallets;
std::unique_ptr<NetworkNode> netNode;

static std::string shortAddr(const std::string& a) {
    if (a.size() <= 12) return a;
    return a.substr(0,8) + "..." + a.substr(a.size()-4);
}

static std::string fmtTime(long ts) {
    char buf[32]; time_t t = ts;
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buf;
}

// ── Wallet persistence ────────────────────────────────────────────────────────

static void saveWallets() {
    std::ofstream out("wallets.dat");
    if (!out.is_open()) { std::cerr << "[ERROR] Cannot open wallets.dat\n"; return; }
    for (auto& w : wallets) {
        std::string ph = w.getPasswordHash().empty() ? "-" : w.getPasswordHash();
        out << w.getSeed() << " " << w.getAddress() << " "
            << w.getPrivateKeyRaw() << " " << ph << "\n";
    }
    std::cout << "[INFO] Wallets saved (" << wallets.size() << ")\n";
}

static void loadWallets() {
    std::ifstream in("wallets.dat");
    if (!in.is_open()) return;
    wallets.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string seed,addr,priv,pass="-";
        if (!(ss >> seed >> addr >> priv)) continue;
        ss >> pass;
        if (pass == "-") pass = "";
        wallets.emplace_back(seed, addr, priv, pass);
    }
    std::cout << "[INFO] Wallets loaded: " << wallets.size() << "\n";
}

static Wallet* findWallet(const std::string& addr) {
    for (auto& w : wallets) if (w.getAddress()==addr) return &w;
    return nullptr;
}

static void printHelp() {
    std::cout << "\n+================================================+\n";
    std::cout << "|          Nodevia V1 (NDV) - Commands           |\n";
    std::cout << "+================================================+\n";
    std::cout << "|  WALLET                                        |\n";
    std::cout << "|  create              Create a new wallet       |\n";
    std::cout << "|  list                List wallets + balances   |\n";
    std::cout << "|  balance <addr>      Check NDV balance         |\n";
    std::cout << "|  export <addr>       Show keys + seed          |\n";
    std::cout << "|  passwd <addr>       Change wallet password    |\n";
    std::cout << "|  TRANSACTIONS                                  |\n";
    std::cout << "|  send <f> <t> <amt>  Send NDV (auto-signed)   |\n";
    std::cout << "|  history <addr>      Transaction history       |\n";
    std::cout << "|  mempool             Show pending TXs          |\n";
    std::cout << "|  BLOCKCHAIN                                    |\n";
    std::cout << "|  mine <addr>         Mine + broadcast block    |\n";
    std::cout << "|  chain               Show blockchain explorer  |\n";
    std::cout << "|  status              Node + chain status       |\n";
    std::cout << "|  NETWORK                                       |\n";
    std::cout << "|  node <port>         Start this node           |\n";
    std::cout << "|  connect <host> <p>  Connect to a peer         |\n";
    std::cout << "|  peers               List connected peers      |\n";
    std::cout << "|  sync                Request chain sync        |\n";
    std::cout << "|  discover            Discover more peers       |\n";
    std::cout << "|  syncmem             Sync mempool from peers   |\n";
    std::cout << "|  help                Show this menu            |\n";
    std::cout << "|  exit                Save & quit               |\n";
    std::cout << "+================================================+\n\n";
}

int main() {
    Blockchain nodevia;
    loadWallets();

    std::cout << "\n";
    std::cout << "  ███╗   ██╗ ██████╗ ██████╗ ███████╗██╗   ██╗██╗ █████╗\n";
    std::cout << "  ████╗  ██║██╔═══██╗██╔══██╗██╔════╝██║   ██║██║██╔══██╗\n";
    std::cout << "  ██╔██╗ ██║██║   ██║██║  ██║█████╗  ██║   ██║██║███████║\n";
    std::cout << "  ██║╚██╗██║██║   ██║██║  ██║██╔══╝  ╚██╗ ██╔╝██║██╔══██║\n";
    std::cout << "  ██║ ╚████║╚██████╔╝██████╔╝███████╗ ╚████╔╝ ██║██║  ██║\n";
    std::cout << "  ╚═╝  ╚═══╝ ╚═════╝ ╚═════╝ ╚══════╝  ╚═══╝  ╚═╝╚═╝  ╚═╝\n";
    std::cout << "                     V1 — Decentralized Currency\n\n";
    std::cout << "  Type 'help' to see all commands.\n\n";

    std::string cmd;
    while (true) {
        std::cout << "ndv> ";
        std::cin >> cmd;

        // ── CREATE ───────────────────────────────────────────────
        if (cmd == "create") {
            std::string pass;
            std::cout << "Set password (- to skip): ";
            std::cin >> pass;
            if (pass == "-") pass = "";
            Wallet w;
            if (!pass.empty()) w.setPassword("", pass);
            std::cout << "\n  Wallet Created\n";
            std::cout << "  Address    : " << w.getAddress()   << "\n";
            std::cout << "  Public Key : " << w.getPublicKey() << "\n";
            std::cout << "  Seed       : " << w.getSeed()      << "  <- BACKUP THIS!\n\n";
            wallets.push_back(w);
            saveWallets();
        }

        // ── LIST ─────────────────────────────────────────────────
        else if (cmd == "list") {
            if (wallets.empty()) { std::cout << "  No wallets.\n"; continue; }
            std::cout << "\n+-----+------------------+----------+--------------+\n";
            std::cout << "|  #  | Address          | Password | Balance      |\n";
            std::cout << "+-----+------------------+----------+--------------+\n";
            int i=1;
            for (auto& w : wallets) {
                std::string lock = w.getPasswordHash().empty() ? "No" : "Yes";
                std::cout << "|  "  << std::left  << std::setw(3) << i++
                          << "| "  << std::setw(17) << shortAddr(w.getAddress())
                          << "| "  << std::setw(9)  << lock
                          << "| "  << std::right << std::fixed << std::setprecision(4)
                          << std::setw(9) << nodevia.getBalance(w.getAddress())
                          << " NDV |\n";
            }
            std::cout << "+-----+------------------+----------+--------------+\n\n";
        }

        // ── BALANCE ──────────────────────────────────────────────
        else if (cmd == "balance") {
            std::string addr; std::cin >> addr;
            std::cout << "Balance [" << addr << "]: "
                      << std::fixed << std::setprecision(4)
                      << nodevia.getBalance(addr) << " NDV\n";
        }

        // ── MINE ─────────────────────────────────────────────────
        else if (cmd == "mine") {
            std::string addr; std::cin >> addr;
            if (!findWallet(addr))
                std::cout << "  [WARN] Address not in local wallets.\n";
            std::cout << "  Mining to " << addr << " ...\n";

            if (netNode) {
                nodevia.setOnBlockMined([&](const Block& b) {
                    Message msg{MsgType::BLOCK, netNode->nodeId(), Serializer::blockToStr(b)};
                    netNode->broadcast(msg);
                    std::cout << "  [NET] Block #" << b.index
                              << " broadcast to " << netNode->peerCount() << " peers\n";
                });
            }
            nodevia.minePendingTransactions(addr);
        }

        // ── SEND ─────────────────────────────────────────────────
        else if (cmd == "send") {
            std::string from, to; double amt;
            std::cin >> from >> to >> amt;
            Wallet* w = findWallet(from);
            if (!w) { std::cout << "  [ERROR] Sender wallet not found.\n"; continue; }
            std::string pass = "";
            if (!w->getPasswordHash().empty()) {
                std::cout << "  Password for " << from << ": ";
                std::cin >> pass;
                if (!w->checkPassword(pass)) {
                    std::cout << "  [ERROR] Wrong password.\n"; continue;
                }
            }
            Transaction tx(from, to, amt, TRANSACTION_FEE);
            tx.publicKey = w->getPublicKey();
            tx.signature = w->sign(tx.signingData());
            if (nodevia.addTransaction(tx)) {
                std::cout << "  TX queued: " << amt << " NDV -> " << to << "\n";
                if (netNode) {
                    Message msg{MsgType::TX, netNode->nodeId(), Serializer::txToStr(tx)};
                    netNode->broadcast(msg);
                    std::cout << "  [NET] TX broadcast to " << netNode->peerCount() << " peers\n";
                }
            }
        }

        // ── HISTORY ──────────────────────────────────────────────
        else if (cmd == "history") {
            std::string addr; std::cin >> addr;
            auto txns = nodevia.getAllTransactions();
            int found = 0;
            std::cout << "\nHistory: " << addr << "\n";
            std::cout << "+--------+-------------+----------------------------+\n";
            std::cout << "| Type   | Amount (NDV)| Counterparty               |\n";
            std::cout << "+--------+-------------+----------------------------+\n";
            for (auto& tx : txns) {
                if (tx.sender != addr && tx.receiver != addr) continue;
                found++;
                bool sent = (tx.sender == addr);
                std::string other = sent ? tx.receiver : tx.sender;
                std::cout << "| " << std::left  << std::setw(7) << (sent?"[SENT]":"[RECV]")
                          << "| " << std::right << std::fixed << std::setprecision(4)
                          << std::setw(11) << tx.amount
                          << " | " << std::left << std::setw(26) << shortAddr(other) << "|\n";
            }
            std::cout << "+--------+-------------+----------------------------+\n";
            if (!found) std::cout << "  No transactions.\n";
            std::cout << "\n";
        }

        // ── MEMPOOL ──────────────────────────────────────────────
        else if (cmd == "mempool") {
            auto pool = nodevia.getMempoolSnapshot();
            std::cout << "\n  Mempool: " << pool.size() << " pending TXs\n";
            if (pool.empty()) { std::cout << "  (empty)\n\n"; continue; }
            std::cout << "+------------------+------------------+----------+-------+\n";
            std::cout << "| From             | To               | Amount   | Fee   |\n";
            std::cout << "+------------------+------------------+----------+-------+\n";
            for (auto& tx : pool) {
                std::cout << "| " << std::left << std::setw(17) << shortAddr(tx.sender)
                          << "| " << std::setw(17) << shortAddr(tx.receiver)
                          << "| " << std::right << std::fixed << std::setprecision(2)
                          << std::setw(8) << tx.amount
                          << " | " << std::setw(5) << tx.fee << " |\n";
            }
            std::cout << "+------------------+------------------+----------+-------+\n\n";
        }

        // ── CHAIN (explorer) ─────────────────────────────────────
        else if (cmd == "chain") {
            auto& blocks = nodevia.getChain();
            std::cout << "\n  Blockchain Explorer — " << blocks.size() << " blocks\n\n";
            for (auto& b : blocks) {
                std::cout << "  Block #" << b.index << "\n";
                std::cout << "    Time  : " << fmtTime(b.timestamp) << "\n";
                std::cout << "    Hash  : " << b.hash.substr(0,32) << "...\n";
                std::cout << "    Prev  : " << b.prevHash.substr(0,20) << "...\n";
                std::cout << "    Nonce : " << b.nonce << "\n";
                std::cout << "    TXs   : " << b.transactions.size() << "\n";
                for (auto& tx : b.transactions) {
                    if (tx.sender == "network")
                        std::cout << "      [REWARD] " << tx.amount << " NDV -> "
                                  << shortAddr(tx.receiver) << "\n";
                    else
                        std::cout << "      [TX] " << shortAddr(tx.sender) << " -> "
                                  << shortAddr(tx.receiver) << "  " << tx.amount << " NDV\n";
                }
                std::cout << "\n";
            }
        }

        // ── STATUS ───────────────────────────────────────────────
        else if (cmd == "status") {
            std::cout << "\n  Nodevia V1 Node Status\n";
            std::cout << "  ----------------------\n";
            std::cout << "  Version    : " << NDV_VERSION << "\n";
            std::cout << "  Ticker     : " << NDV_TICKER << "\n";
            std::cout << "  Blocks     : " << nodevia.chainLength() << "\n";
            std::cout << "  Difficulty : " << nodevia.getDifficulty() << "\n";
            std::cout << "  Mempool    : " << nodevia.mempoolSize() << " TXs\n";
            std::cout << "  Wallets    : " << wallets.size() << "\n";
            if (netNode) {
                std::cout << "  Node ID    : " << netNode->nodeId() << "\n";
                std::cout << "  Peers      : " << netNode->peerCount() << "\n";
            } else {
                std::cout << "  Network    : offline (use 'node <port>')\n";
            }
            std::cout << "\n";
        }

        // ── EXPORT ───────────────────────────────────────────────
        else if (cmd == "export") {
            std::string addr; std::cin >> addr;
            Wallet* w = findWallet(addr);
            if (!w) { std::cout << "  [ERROR] Wallet not found.\n"; continue; }
            std::string pass = "";
            if (!w->getPasswordHash().empty()) {
                std::cout << "  Password: "; std::cin >> pass;
                if (!w->checkPassword(pass)) {
                    std::cout << "  [ERROR] Wrong password.\n"; continue;
                }
            }
            std::cout << "\n  SENSITIVE — Do not share!\n";
            std::cout << "  Address    : " << w->getAddress()        << "\n";
            std::cout << "  Public Key : " << w->getPublicKey()      << "\n";
            std::cout << "  Seed       : " << w->getSeed()           << "\n";
            std::cout << "  Private Key: " << w->getPrivateKey(pass) << "\n\n";
        }

        // ── PASSWD ───────────────────────────────────────────────
        else if (cmd == "passwd") {
            std::string addr; std::cin >> addr;
            Wallet* w = findWallet(addr);
            if (!w) { std::cout << "  [ERROR] Wallet not found.\n"; continue; }
            std::string oldPass="", newPass="";
            if (!w->getPasswordHash().empty()) {
                std::cout << "  Current password: "; std::cin >> oldPass;
            }
            std::cout << "  New password (- to remove): "; std::cin >> newPass;
            if (newPass == "-") newPass = "";
            if (w->setPassword(oldPass, newPass)) {
                std::cout << "  Password updated.\n"; saveWallets();
            } else {
                std::cout << "  [ERROR] Current password incorrect.\n";
            }
        }

        // ── NODE ─────────────────────────────────────────────────
        else if (cmd == "node") {
            int port; std::cin >> port;
            if (netNode) { std::cout << "  [NET] Already running.\n"; continue; }
            netNode = std::make_unique<NetworkNode>(nodevia, "0.0.0.0", port);
            netNode->start();
        }

        // ── CONNECT ──────────────────────────────────────────────
        else if (cmd == "connect") {
            std::string host; int port;
            std::cin >> host >> port;
            if (!netNode) { std::cout << "  Start node first: node <port>\n"; continue; }
            netNode->connectToPeer(host, port);
        }

        // ── PEERS ────────────────────────────────────────────────
        else if (cmd == "peers") {
            if (!netNode) { std::cout << "  No node running.\n"; continue; }
            auto ids = netNode->getPeerIds();
            std::cout << "  Connected peers (" << ids.size() << "):\n";
            for (auto& id : ids) std::cout << "    " << id << "\n";
            if (ids.empty()) std::cout << "    (none)\n";
        }

        // ── SYNC ─────────────────────────────────────────────────
        else if (cmd == "sync") {
            if (!netNode) { std::cout << "  No node running.\n"; continue; }
            Message req{MsgType::CHAIN_REQUEST, netNode->nodeId(), ""};
            netNode->broadcast(req);
            std::cout << "  [NET] Chain sync requested\n";
        }

        // ── DISCOVER ─────────────────────────────────────────────
        else if (cmd == "discover") {
            if (!netNode) { std::cout << "  No node running.\n"; continue; }
            netNode->discoverPeers();
        }

        // ── SYNCMEM ──────────────────────────────────────────────
        else if (cmd == "syncmem") {
            if (!netNode) { std::cout << "  No node running.\n"; continue; }
            netNode->syncMempool();
        }

        // ── HELP ─────────────────────────────────────────────────
        else if (cmd == "help") { printHelp(); }

        // ── EXIT ─────────────────────────────────────────────────
        else if (cmd == "exit" || cmd == "quit") {
            saveWallets();
            std::cout << "  Goodbye from Nodevia V1\n";
            break;
        }

        else {
            std::cout << "  Unknown command: '" << cmd << "'. Type 'help'.\n";
        }
    }
    return 0;
}