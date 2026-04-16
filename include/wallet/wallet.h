#pragma once
#include <string>

class Wallet {
private:
    std::string seed;
    std::string address;
    std::string privateKey;
    std::string publicKey;
    std::string passwordHash;

public:
    Wallet();
    Wallet(std::string addr, std::string priv, std::string passHash);
    Wallet(std::string seed, std::string addr, std::string priv, std::string passHash);

    std::string getAddress()      const;
    std::string getSeed()         const;
    std::string getPasswordHash() const;
    std::string getPublicKey()    const;
    std::string getPrivateKeyRaw()const;  // for persistence only — no password check
    std::string getPrivateKey(const std::string& password) const;

    bool checkPassword(const std::string& password) const;
    bool setPassword(const std::string& oldPassword, const std::string& newPassword);

    std::string sign(const std::string& data) const;
    static bool verifySignature(const std::string& data,
                                const std::string& signature,
                                const std::string& publicKey);
};