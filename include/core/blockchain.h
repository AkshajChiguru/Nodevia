#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm>
#include "core/block.h"
#include "core/transaction.h"

class Blockchain {
public:
    using BlockCallback = std::function<void(const Block&)>;
    using TxCallback    = std::function<void(const Transaction&)>;

    void setOnBlockMined(BlockCallback cb) { onBlockMined_ = cb; }
    void setOnTxAdded(TxCallback cb)       { onTxAdded_   = cb; }

private:
    std::vector<Block>       chain_;
    std::vector<Transaction> mempool_;
    int                      difficulty_;
    long                     lastBlockTime_;  // for difficulty adjustment

    BlockCallback onBlockMined_;
    TxCallback    onTxAdded_;

    Block  createGenesisBlock();
    Block  getLatestBlock();
    bool   isTransactionValid(const Transaction& tx);
    bool   txidExists(const std::string& txid) const;
    double getEffectiveBalance(const std::string& address) const;
    int    adjustDifficulty();          // auto-adjust based on block time

public:
    Blockchain();

    // ── Transaction ───────────────────────────────────────────────
    bool   addTransaction(const Transaction& tx);

    // ── Mining ────────────────────────────────────────────────────
    void   minePendingTransactions(std::string minerAddress);

    // ── Block (network) ───────────────────────────────────────────
    bool   addBlock(const Block& b);
    bool   replaceChainIfBetter(const std::vector<Block>& newChain);

    // ── Mempool ───────────────────────────────────────────────────
    std::vector<Transaction> getMempoolSnapshot() const { return mempool_; }
    int  mempoolSize() const { return (int)mempool_.size(); }

    // ── Query ─────────────────────────────────────────────────────
    double getBalance(std::string address);
    std::vector<Transaction> getAllTransactions();
    bool   isChainValid();
    bool   isChainValidFor(const std::vector<Block>& c) const;
    int    chainLength()   const { return (int)chain_.size(); }
    int    getDifficulty() const { return difficulty_; }
    std::string getLatestHash() const;
    const std::vector<Block>& getChain() const { return chain_; }

    // ── Persistence ───────────────────────────────────────────────
    void   saveToFile();
    void   loadFromFile();
};
#endif