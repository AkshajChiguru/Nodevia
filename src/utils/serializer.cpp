#include "utils/serializer.h"
#include <sstream>
#include <stdexcept>

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string esc(const std::string& s) {
    // Fields must not contain ':' or ';'.
    // Hashes / hex strings are safe. Addresses are safe ("NDV-..." = alphanum+dash).
    return s.empty() ? "-" : s;
}
static std::string unesc(const std::string& s) {
    return (s == "-") ? "" : s;
}

// Split string by first N occurrences of delim, put rest in last token
static std::vector<std::string> splitN(const std::string& s, char delim, int n) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (int i = 0; i < n && start < s.size(); ++i) {
        size_t pos = s.find(delim, start);
        if (pos == std::string::npos) { parts.push_back(s.substr(start)); return parts; }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    if (start <= s.size()) parts.push_back(s.substr(start));
    return parts;
}

// Split ALL occurrences
static std::vector<std::string> splitAll(const std::string& s, const std::string& delim) {
    std::vector<std::string> parts;
    size_t start = 0, pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    parts.push_back(s.substr(start));
    return parts;
}

// ── Transaction ───────────────────────────────────────────────────────────────

std::string Serializer::txToStr(const Transaction& tx) {
    return esc(tx.txid)      + ":" +
           esc(tx.sender)    + ":" +
           esc(tx.receiver)  + ":" +
           std::to_string(tx.amount) + ":" +
           std::to_string(tx.fee)    + ":" +
           esc(tx.signature) + ":" +
           esc(tx.publicKey);
}

Transaction Serializer::strToTx(const std::string& s) {
    auto p = splitN(s, ':', 6);
    if (p.size() < 7) throw std::runtime_error("Bad TX: " + s);

    std::string sender   = unesc(p[1]);
    std::string receiver = unesc(p[2]);
    double amount = std::stod(p[3]);
    double fee    = std::stod(p[4]);

    Transaction tx(sender, receiver, amount, fee);
    tx.txid      = unesc(p[0]);
    tx.signature = unesc(p[5]);
    tx.publicKey = unesc(p[6]);
    return tx;
}

// ── Block ─────────────────────────────────────────────────────────────────────
// Format: idx;ts;nonce;prev;hash;tx_count;txstr1;txstr2;...
// Separator between blocks in chain: "||"

std::string Serializer::blockToStr(const Block& b) {
    std::string s = std::to_string(b.index)     + ";" +
                    std::to_string(b.timestamp)  + ";" +
                    std::to_string(b.nonce)      + ";" +
                    esc(b.prevHash)              + ";" +
                    esc(b.hash)                  + ";" +
                    std::to_string(b.transactions.size());
    for (auto& tx : b.transactions)
        s += ";" + txToStr(tx);
    return s;
}

Block Serializer::strToBlock(const std::string& s) {
    auto p = splitAll(s, ";");
    if (p.size() < 6) throw std::runtime_error("Bad Block: " + s.substr(0,40));

    int  idx   = std::stoi(p[0]);
    long ts    = std::stol(p[1]);
    int  nc    = std::stoi(p[2]);
    auto prev  = unesc(p[3]);
    auto hash  = unesc(p[4]);
    int  cnt   = std::stoi(p[5]);

    std::vector<Transaction> txs;
    for (int i = 0; i < cnt && (6 + i) < (int)p.size(); ++i)
        txs.push_back(strToTx(p[6 + i]));

    return Block(idx, ts, nc, prev, hash, txs);
}

// ── Chain ─────────────────────────────────────────────────────────────────────

std::string Serializer::chainToStr(const std::vector<Block>& chain) {
    std::string result;
    for (size_t i = 0; i < chain.size(); ++i) {
        if (i > 0) result += "||";
        result += blockToStr(chain[i]);
    }
    return result;
}

std::vector<Block> Serializer::strToChain(const std::string& s) {
    std::vector<Block> chain;
    auto parts = splitAll(s, "||");
    for (auto& p : parts) {
        if (p.empty()) continue;
        chain.push_back(strToBlock(p));
    }
    return chain;
}