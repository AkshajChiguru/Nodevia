#pragma once
#include "core/block.h"
#include "core/transaction.h"
#include <string>
#include <vector>

// Converts blockchain objects to/from wire-safe strings.
// Uses ';' as record separator and ':' as field separator.
// These characters must NOT appear in any field value (hashes are hex-safe).

namespace Serializer {
    // Transaction: txid:sender:receiver:amount:fee:sig:pubkey
    std::string   txToStr (const Transaction& tx);
    Transaction   strToTx (const std::string& s);

    // Block: idx:ts:nonce:prev:hash:tx_count;tx1;tx2;...
    std::string   blockToStr (const Block& b);
    Block         strToBlock (const std::string& s);

    // Chain: block1;;block2;;...
    std::string             chainToStr (const std::vector<Block>& chain);
    std::vector<Block>      strToChain (const std::string& s);
}