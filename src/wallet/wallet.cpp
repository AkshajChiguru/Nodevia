#include "wallet/wallet.h"
#include "network/seed.h"
#include <functional>
#include <sstream>
#include <iomanip>

// Two-round deterministic hash → 32 hex chars
static std::string hashStr(const std::string& s) {
    size_t r1 = std::hash<std::string>{}(s);
    size_t r2 = std::hash<std::string>{}(std::to_string(r1) + s);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << r1
        << std::setw(16) << r2;
    return oss.str();
}

// publicKey = hash("sign:" + privateKey)  — one-way, used for verification
static std::string derivePublicKey(const std::string& priv) {
    return hashStr("sign:" + priv);
}

// Address = "NDV-" + first 12 chars of hash(seed)
static std::string deriveAddress(const std::string& s) {
    std::string h = hashStr(s);
    while (h.size() < 12) h = "0" + h;
    return "NDV-" + h.substr(0, 12);
}

Wallet::Wallet() {
    seed         = Seed::generateSeed();
    privateKey   = Seed::derive(seed, "priv");
    publicKey    = derivePublicKey(privateKey);
    address      = deriveAddress(seed);
    passwordHash = "";
}

Wallet::Wallet(std::string addr, std::string priv, std::string passHash) {
    seed         = "derived:" + addr;
    address      = addr;
    privateKey   = priv;
    publicKey    = derivePublicKey(priv);
    passwordHash = passHash;
}

Wallet::Wallet(std::string s, std::string addr, std::string priv, std::string passHash) {
    seed         = s;
    address      = addr;
    privateKey   = priv;
    publicKey    = derivePublicKey(priv);
    passwordHash = passHash;
}

std::string Wallet::getAddress()      const { return address;   }
std::string Wallet::getSeed()         const { return seed;      }
std::string Wallet::getPasswordHash() const { return passwordHash; }
std::string Wallet::getPublicKey()    const { return publicKey; }
std::string Wallet::getPrivateKeyRaw()const { return privateKey; }  // persistence use only

std::string Wallet::getPrivateKey(const std::string& password) const {
    if (passwordHash.empty()) return privateKey;
    if (hashStr(password) == passwordHash) return privateKey;
    return "WRONG_PASSWORD";
}

bool Wallet::checkPassword(const std::string& password) const {
    if (passwordHash.empty()) return true;
    return hashStr(password) == passwordHash;
}

bool Wallet::setPassword(const std::string& oldPassword, const std::string& newPassword) {
    if (!checkPassword(oldPassword)) return false;
    passwordHash = newPassword.empty() ? "" : hashStr(newPassword);
    return true;
}

// signature = hash(publicKey + data)
std::string Wallet::sign(const std::string& data) const {
    return hashStr(publicKey + data);
}

// verify: recompute hash(publicKey + data) == signature
bool Wallet::verifySignature(const std::string& data,
                              const std::string& signature,
                              const std::string& publicKey) {
    if (signature.empty() || publicKey.empty()) return false;
    return hashStr(publicKey + data) == signature;
}