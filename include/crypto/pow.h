#ifndef POW_H
#define POW_H
#include "core/block.h"
class ProofOfWork {
public:
    static void mine(Block& block, int difficulty);
};
#endif