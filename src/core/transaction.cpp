#include "core/transaction.h"
#include "utils/utils.h"
#include <ctime>
#include <atomic>

// Monotonic counter — ensures txid is unique even for identical field values
static std::atomic<uint64_t> txCounter{0};

Transaction::Transaction(std::string s, std::string r, double a, double f)
    : sender(s), receiver(r), amount(a), fee(f)
{
    uint64_t seq = ++txCounter;
    txid = sha256(sender + receiver
                + std::to_string(amount) + std::to_string(fee)
                + std::to_string((long)time(0))
                + std::to_string(seq));
}

std::string Transaction::signingData() const {
    return sender + receiver + std::to_string(amount) + std::to_string(fee);
}

std::string Transaction::toString() const {
    return txid + signingData() + signature + publicKey;
}