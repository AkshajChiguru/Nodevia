#ifndef BLOCK_H
#define BLOCK_H
#include <string>
#include <vector>
#include "core/transaction.h"

class Block {
public:
    int index;
    std::vector<Transaction> transactions;
    std::string prevHash;
    std::string hash;
    long timestamp;
    int  nonce;

    // Normal constructor: used when mining new blocks
    Block(int idx, std::vector<Transaction> txs, std::string prevHash);

    // Restore constructor: used by loadFromFile — no recalculation
    Block(int idx, long ts, int nonce,
          std::string prev, std::string h,
          std::vector<Transaction> txs);

    std::string calculateHash();
};
#endif