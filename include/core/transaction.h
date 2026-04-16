#ifndef TRANSACTION_H
#define TRANSACTION_H
#include <string>

class Transaction {
public:
    std::string txid;
    std::string sender;
    std::string receiver;
    double      amount;
    double      fee;
    std::string signature;
    std::string publicKey;

    Transaction(std::string s, std::string r, double a, double f = 0.0);

    std::string signingData() const;  // sender+receiver+amount+fee (for signing)
    std::string toString()    const;  // full string including txid (for hashing)
};
#endif