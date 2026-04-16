#include "network/seed.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace Seed {

std::string generateSeed() {
    std::random_device rd;
    std::mt19937_64 rng(rd() ^ (uint64_t)
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t r1 = dist(rng), r2 = dist(rng), r3 = dist(rng);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << r1
        << std::setw(16) << r2
        << std::setw(16) << r3;
    return oss.str();
}

std::string derive(const std::string& seed, const std::string& salt) {
    std::hash<std::string> h;
    size_t r1 = h(seed + ":" + salt);
    size_t r2 = h(std::to_string(r1) + salt);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << r1
        << std::setw(16) << r2;
    return oss.str();
}

} // namespace Seed