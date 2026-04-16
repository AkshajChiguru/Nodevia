#include "core/blockchain.h"
#include "crypto/pow.h"
#include "utils/config.h"
#include "wallet/wallet.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>

Blockchain::Blockchain() : difficulty_(INITIAL_DIFFICULTY), lastBlockTime_((long)time(0)) {
    loadFromFile();
    if (chain_.empty()) {
        chain_.push_back(createGenesisBlock());
    } else if (!isChainValid()) {
        std::cout << "[ERROR] Blockchain corrupted!\n";
    }
    std::cout << "Nodevia V1 started | Blocks: " << chain_.size()
              << " | Difficulty: " << difficulty_ << "\n";
}

Block Blockchain::createGenesisBlock() {
    return Block(0, {Transaction("network","genesis",0)}, "0");
}

Block Blockchain::getLatestBlock() { return chain_.back(); }

std::string Blockchain::getLatestHash() const {
    return chain_.empty() ? "0" : chain_.back().hash;
}

// ── Difficulty adjustment (every DIFFICULTY_ADJUST_INTERVAL blocks) ───────────

int Blockchain::adjustDifficulty() {
    int len = (int)chain_.size();
    if (len % DIFFICULTY_ADJUST_INTERVAL != 0 || len == 0) return difficulty_;

    long now = (long)time(0);
    long elapsed = now - lastBlockTime_;
    long expected = (long)DIFFICULTY_ADJUST_INTERVAL * TARGET_BLOCK_TIME;

    if (elapsed < expected / 2 && difficulty_ < 6) {
        difficulty_++;
        std::cout << "[CHAIN] Difficulty increased to " << difficulty_ << "\n";
    } else if (elapsed > expected * 2 && difficulty_ > 1) {
        difficulty_--;
        std::cout << "[CHAIN] Difficulty decreased to " << difficulty_ << "\n";
    }
    lastBlockTime_ = now;
    return difficulty_;
}

// ── Chain validation ──────────────────────────────────────────────────────────

bool Blockchain::isChainValidFor(const std::vector<Block>& c) const {
    for (size_t i = 0; i < c.size(); ++i) {
        if (const_cast<Block&>(c[i]).calculateHash() != c[i].hash) {
            std::cout << "[WARN] Block " << i << " hash mismatch\n"; return false;
        }
        if (i > 0) {
            std::string target(difficulty_, '0');
            if (c[i].hash.substr(0, difficulty_) != target) {
                std::cout << "[WARN] Block " << i << " PoW invalid\n"; return false;
            }
            if (c[i].prevHash != c[i-1].hash) {
                std::cout << "[WARN] Block " << i << " broken link\n"; return false;
            }
        }
    }
    return true;
}

bool Blockchain::isChainValid() { return isChainValidFor(chain_); }

bool Blockchain::replaceChainIfBetter(const std::vector<Block>& nc) {
    if (nc.size() <= chain_.size()) return false;
    if (!isChainValidFor(nc)) return false;
    chain_ = nc;
    saveToFile();
    return true;
}

// ── Accept block from network ─────────────────────────────────────────────────

bool Blockchain::addBlock(const Block& b) {
    if (b.prevHash != getLatestHash()) {
        std::cout << "[INVALID] Block prevHash mismatch\n"; return false;
    }
    std::string target(difficulty_, '0');
    if (b.hash.size() < (size_t)difficulty_ || b.hash.substr(0, difficulty_) != target) {
        std::cout << "[INVALID] Block PoW invalid\n"; return false;
    }
    if (const_cast<Block&>(b).calculateHash() != b.hash) {
        std::cout << "[INVALID] Block hash incorrect\n"; return false;
    }
    for (auto& tx : b.transactions) {
        if (tx.sender == "network") continue;
        if (!Wallet::verifySignature(tx.signingData(), tx.signature, tx.publicKey)) {
            std::cout << "[INVALID] TX sig invalid in block\n"; return false;
        }
    }
    chain_.push_back(b);
    // Remove confirmed txs from mempool
    for (auto& btx : b.transactions)
        mempool_.erase(std::remove_if(mempool_.begin(), mempool_.end(),
            [&](const Transaction& mt){ return mt.txid == btx.txid; }), mempool_.end());
    saveToFile();
    return true;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

bool Blockchain::txidExists(const std::string& txid) const {
    for (auto& tx : mempool_) if (tx.txid == txid) return true;
    for (auto& b  : chain_)   for (auto& tx : b.transactions) if (tx.txid == txid) return true;
    return false;
}

double Blockchain::getEffectiveBalance(const std::string& addr) const {
    double bal = 0;
    for (auto& b : chain_) for (auto& tx : b.transactions) {
        if (tx.sender   == addr) bal -= (tx.amount + tx.fee);
        if (tx.receiver == addr) bal += tx.amount;
    }
    for (auto& tx : mempool_) if (tx.sender == addr) bal -= (tx.amount + tx.fee);
    return bal;
}

// ── Transaction ───────────────────────────────────────────────────────────────

bool Blockchain::addTransaction(const Transaction& tx) {
    if ((int)mempool_.size() >= MAX_MEMPOOL_SIZE) {
        std::cout << "[MEMPOOL] Full — rejecting TX\n"; return false;
    }
    if (isTransactionValid(tx)) {
        mempool_.push_back(tx);
        std::cout << "TX added [" << tx.txid.substr(0,10) << "...] fee=" << tx.fee << "\n";
        if (onTxAdded_) onTxAdded_(tx);
        return true;
    }
    return false;
}

bool Blockchain::isTransactionValid(const Transaction& tx) {
    if (tx.amount <= 0) return false;
    if (tx.sender == "network") return true;
    if (tx.fee < TRANSACTION_FEE_MIN) {
        std::cout << "[INVALID] Fee below minimum\n"; return false;
    }
    if (txidExists(tx.txid)) {
        std::cout << "[INVALID] Duplicate TX\n"; return false;
    }
    if (tx.signature.empty() || tx.publicKey.empty()) {
        std::cout << "[INVALID] Missing sig/pubkey\n"; return false;
    }
    if (!Wallet::verifySignature(tx.signingData(), tx.signature, tx.publicKey)) {
        std::cout << "[INVALID] Sig verification failed\n"; return false;
    }
    if (getEffectiveBalance(tx.sender) < (tx.amount + tx.fee)) {
        std::cout << "[INVALID] Insufficient balance\n"; return false;
    }
    return true;
}

// ── Mining ────────────────────────────────────────────────────────────────────

void Blockchain::minePendingTransactions(std::string minerAddress) {
    // Sort mempool by fee descending — miners pick highest-fee txs first
    auto pool = mempool_;
    std::sort(pool.begin(), pool.end(),
        [](const Transaction& a, const Transaction& b){ return a.fee > b.fee; });

    // Cap at MAX_BLOCK_TXS
    if ((int)pool.size() > MAX_BLOCK_TXS)
        pool = std::vector<Transaction>(pool.begin(), pool.begin() + MAX_BLOCK_TXS);

    double totalFees = 0;
    for (auto& tx : pool) totalFees += tx.fee;

    // Calculate block reward (with halving)
    int halvings = (int)(chain_.size() / HALVING_INTERVAL);
    double reward = MINING_REWARD;
    for (int i = 0; i < halvings && reward > 0.001; ++i) reward /= 2.0;

    Transaction rewardTx("network", minerAddress, reward + totalFees);
    pool.push_back(rewardTx);

    int diff = adjustDifficulty();
    Block newBlock(chain_.size(), pool, getLatestBlock().hash);
    ProofOfWork::mine(newBlock, diff);

    // Remove mined txs from mempool
    for (auto& tx : pool)
        mempool_.erase(std::remove_if(mempool_.begin(), mempool_.end(),
            [&](const Transaction& mt){ return mt.txid == tx.txid; }), mempool_.end());

    chain_.push_back(newBlock);
    saveToFile();

    Block& b = chain_.back();
    std::cout << "\n+==============================+\n";
    std::cout << "  Block #"    << b.index       << " mined!\n";
    std::cout << "  Hash      : " << b.hash.substr(0,24) << "...\n";
    std::cout << "  TXs       : " << (int)b.transactions.size() << "\n";
    std::cout << "  Difficulty: " << diff         << "\n";
    std::cout << "  Reward    : " << std::fixed << std::setprecision(4)
              << (reward + totalFees)             << " NDV\n";
    std::cout << "  Balance   : " << std::fixed << std::setprecision(4)
              << getBalance(minerAddress)          << " NDV\n";
    std::cout << "+==============================+\n\n";

    if (onBlockMined_) onBlockMined_(b);
}

// ── Balance ───────────────────────────────────────────────────────────────────

double Blockchain::getBalance(std::string addr) {
    double bal = 0;
    for (auto& b : chain_) for (auto& tx : b.transactions) {
        if (tx.sender   == addr) bal -= (tx.amount + tx.fee);
        if (tx.receiver == addr) bal += tx.amount;
    }
    return bal;
}

std::vector<Transaction> Blockchain::getAllTransactions() {
    std::vector<Transaction> all;
    for (auto& b : chain_) for (auto& t : b.transactions) all.push_back(t);
    return all;
}

// ── Persistence ───────────────────────────────────────────────────────────────

void Blockchain::saveToFile() {
    std::ofstream out("blockchain.dat");
    if (!out.is_open()) { std::cerr << "[ERROR] Cannot write blockchain.dat\n"; return; }
    for (auto& b : chain_) {
        out << "BLOCK\n" << b.index << "\n" << b.timestamp << "\n" << b.nonce << "\n"
            << b.prevHash << "\n" << b.hash << "\n" << b.transactions.size() << "\n";
        for (auto& tx : b.transactions) {
            out << "TX "
                << (tx.txid.empty()      ? "-" : tx.txid)      << " "
                << tx.sender << " " << tx.receiver << " "
                << tx.amount << " " << tx.fee << " "
                << (tx.signature.empty() ? "-" : tx.signature)  << " "
                << (tx.publicKey.empty() ? "-" : tx.publicKey)  << "\n";
        }
        out << "END\n";
    }
    out.close();
}

void Blockchain::loadFromFile() {
    std::ifstream in("blockchain.dat");
    if (!in.is_open()) return;
    chain_.clear();
    std::string line;
    auto strip = [](std::string& s){ if (!s.empty()&&s.back()=='\r') s.pop_back(); };
    while (std::getline(in, line)) {
        strip(line);
        if (line != "BLOCK") continue;
        std::string si,st,sn,sp,sh,sc;
        if (!std::getline(in,si))  { break; } strip(si);
        if (!std::getline(in,st))  { break; } strip(st);
        if (!std::getline(in,sn))  { break; } strip(sn);
        if (!std::getline(in,sp))  { break; } strip(sp);
        if (!std::getline(in,sh))  { break; } strip(sh);
        if (!std::getline(in,sc))  { break; } strip(sc);
        int idx=std::stoi(si); long ts=std::stol(st);
        int nc=std::stoi(sn);  int cnt=std::stoi(sc);
        std::vector<Transaction> txs;
        for (int t=0; t<cnt; ++t) {
            if (!std::getline(in,line)) { break; } strip(line);
            if (line.size()<3||line.substr(0,3)!="TX ") { --t; continue; }
            std::istringstream ss(line.substr(3));
            std::string txid,sender,receiver,sig,pub;
            double amount=0,fee=0;
            ss >> txid >> sender >> receiver >> amount >> fee >> sig >> pub;
            Transaction tx(sender,receiver,amount,fee);
            tx.txid=(txid=="-")?tx.txid:txid;
            tx.signature=(sig=="-")?"":sig;
            tx.publicKey=(pub=="-")?"":pub;
            txs.push_back(tx);
        }
        while (std::getline(in,line)) { strip(line); if (line=="END") break; }
        chain_.emplace_back(idx,ts,nc,sp,sh,txs);
    }
    std::cout << "Loaded " << chain_.size() << " blocks\n";
}