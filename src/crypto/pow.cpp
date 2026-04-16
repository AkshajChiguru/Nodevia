#include "crypto/pow.h"
#include <iostream>
void ProofOfWork::mine(Block& block, int difficulty) {
    std::string target(difficulty, '0');
    while (block.hash.substr(0, difficulty) != target) {
        block.nonce++;
        block.hash = block.calculateHash();
    }
    std::cout << "Mined Block: " << block.hash << "\n";
}