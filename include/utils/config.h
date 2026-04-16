#ifndef CONFIG_H
#define CONFIG_H

// ── Nodevia V1 Configuration ──────────────────────────────────────────────────

// Token identity
#define NDV_VERSION      "1.0.0"
#define NDV_TICKER       "NDV"
#define NDV_NAME         "Nodevia"

// Supply
const double MINING_REWARD        = 50.0;    // NDV per block (halves every HALVING_INTERVAL)
const double TRANSACTION_FEE_MIN  = 0.001;   // Minimum fee
const double TRANSACTION_FEE      = 1.0;     // Default fee used by CLI
const int    HALVING_INTERVAL     = 210000;  // Blocks between reward halvings
const double MAX_SUPPLY           = 21000000.0; // 21 million NDV

// Proof of Work
const int    INITIAL_DIFFICULTY   = 2;       // Leading zeros required
const int    DIFFICULTY_ADJUST_INTERVAL = 10;     // Recalculate every N blocks
const long   TARGET_BLOCK_TIME    = 10;      // Target seconds per block

// Mempool
const int    MAX_MEMPOOL_SIZE     = 5000;    // Max pending transactions
const int    MAX_BLOCK_TXS        = 500;     // Max TXs per block

// Network
const int    DEFAULT_PORT         = 8333;    // Default P2P port
const int    MAX_PEERS            = 50;      // Max simultaneous peers
const int    PEER_TIMEOUT_SEC     = 30;      // Seconds before peer timeout

#endif