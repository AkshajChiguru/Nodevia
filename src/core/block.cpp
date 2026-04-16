#include "core/block.h"
#include "utils/utils.h"
#include <ctime>

Block::Block(int idx, std::vector<Transaction> txs, std::string prev) {
    index        = idx;
    transactions = txs;
    prevHash     = prev;
    timestamp    = time(0);
    nonce        = 0;
    hash         = calculateHash();
}

// Restore constructor: all fields set directly, hash NOT recalculated
Block::Block(int idx, long ts, int nc,
             std::string prev, std::string h,
             std::vector<Transaction> txs) {
    index        = idx;
    timestamp    = ts;
    nonce        = nc;
    prevHash     = prev;
    hash         = h;
    transactions = txs;
}

std::string Block::calculateHash() {
    std::string txData;
    for (const auto& tx : transactions) txData += tx.toString();
    return sha256(
        std::to_string(index)     +
        txData                    +
        prevHash                  +
        std::to_string(timestamp) +
        std::to_string(nonce)
    );
}