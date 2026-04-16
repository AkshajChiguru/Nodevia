#pragma once
#include <string>

namespace Seed {
    std::string generateSeed();
    std::string derive(const std::string& seed, const std::string& salt);
}