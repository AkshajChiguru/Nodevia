# Nodevia (NDV) V1

A peer-to-peer electronic currency system implemented from first principles in C++17.

Nodevia provides a fully operational blockchain network with native wallet management, transaction signing, proof-of-work mining, a distributed mempool, peer-to-peer node communication, and chain synchronization. The implementation uses no external blockchain frameworks. All core components — SHA-256 hashing, the proof-of-work algorithm, the wallet derivation system, the transaction model, and the networking layer — are written entirely in standard C++17.

---

## Author

**Akshaj Chiguru**

---

## Overview

| Property | Value |
|---|---|
| Language | C++17 |
| Ticker | NDV |
| Consensus | Proof of Work (SHA-256) |
| Networking | Peer-to-peer (multi-node) |
| Storage | File-based persistence |
| Interface | Command-line (CLI) |
| External dependencies | None |

---

## Architecture

Nodevia is composed of six primary subsystems:

**Blockchain** — A sequential, cryptographically linked chain of blocks. Each block embeds the hash of its predecessor, ensuring that any modification to historical data invalidates all subsequent blocks. The full chain is viewable through the built-in explorer and persisted to `blockchain.dat`.

**Transaction System** — Transactions transfer NDV tokens between wallet addresses and are automatically signed at the point of creation. Each transaction carries a sender address, receiver address, transfer amount, fee, digital signature, and a unique transaction identifier. Full transaction history is queryable per address.

**Mempool** — Unconfirmed transactions are held in a distributed pending pool prior to block inclusion. The mempool is viewable at any time and synchronizes across connected peers. Only validated, non-duplicate transactions are accepted.

**Proof of Work** — Block creation requires finding a nonce such that the SHA-256 hash of the block header satisfies a configurable difficulty target. The miner broadcasts the completed block to the network upon success. The base block reward is 10 NDV plus the aggregate of all included transaction fees.

**Wallet System** — Wallets are derived from a randomly generated seed. A private key and address are produced deterministically from the seed. Transactions are signed automatically at send time. Optional password protection restricts access to key material. Wallet data is persisted to `wallets.dat`.

**Peer-to-Peer Network** — Nodes communicate directly with one another. Each node can initiate connections to peers, broadcast transactions and blocks, synchronize chain state, synchronize the mempool, and discover additional peers through connected nodes.

---

## Build

**Requirements:** A C++17-compliant compiler (GCC 7+ or Clang 5+) and a POSIX-compatible environment.

```bash
g++ -std=c++17 src/*.cpp -Iinclude -o nodevia
```

---

## Usage

```bash
./nodevia
```

All interaction is conducted through the command-line interface.

### Wallet

| Command | Description |
|---|---|
| `create` | Create a new wallet |
| `list` | List all wallets and balances |
| `balance <addr>` | Check the NDV balance of an address |
| `export <addr>` | Display keys and seed phrase for a wallet |
| `passwd <addr>` | Set or change a wallet password |

### Transactions

| Command | Description |
|---|---|
| `send <from> <to> <amount>` | Send NDV (transaction is auto-signed) |
| `history <addr>` | Display full transaction history for an address |
| `mempool` | Show all pending unconfirmed transactions |

### Blockchain

| Command | Description |
|---|---|
| `mine <addr>` | Mine a new block and broadcast it to peers |
| `chain` | Open the blockchain explorer |
| `status` | Display node and chain status |

### Network

| Command | Description |
|---|---|
| `node <port>` | Start this node on the specified port |
| `connect <host> <port>` | Connect to a peer node |
| `peers` | List all currently connected peers |
| `sync` | Request a full chain sync from peers |
| `syncmem` | Synchronize the mempool from peers |
| `discover` | Request peer lists from connected nodes |

### General

| Command | Description |
|---|---|
| `help` | Display the command reference |
| `exit` | Save state and quit |

---

## Data Files

| File | Contents |
|---|---|
| `blockchain.dat` | Full blockchain ledger |
| `wallets.dat` | Wallet key material and metadata |

Both files are created automatically on first run. The blockchain is fully validated on startup.

---

## Genesis Block

The genesis block carries the following inscription:

```
"The Times 2026 — Nodevia begins"
```

---

## Limitations

Nodevia V1 is an educational implementation. The following constraints apply:

- No production-grade cryptographic signature scheme. The security model is intentionally simplified.
- No script-based transaction conditions or smart contract support.
- Network resilience depends on a sufficient number of independently operated nodes.
- Cryptographic components are implemented internally and have not been audited to production standards.

---

## Planned Improvements

- Full cryptographic digital signature scheme
- Merkle tree integration for efficient transaction verification
- Simplified payment verification (SPV) for lightweight clients
- Difficulty adjustment based on observed block production rate
- Enhanced peer discovery and connection management
- Performance optimizations to the mining engine and storage subsystems

---

## Reference

Nodevia is inspired by the design principles described in:

> S. Nakamoto, "Bitcoin: A Peer-to-Peer Electronic Cash System," 2008.

---

## License

This project is released for educational purposes. Refer to the `LICENSE` file for full terms.
