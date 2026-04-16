#pragma once
#include <string>

// ── Message types ─────────────────────────────────────────────────────────────
// Wire format (one line, terminated '\n'):
//   TYPE|SENDER_ID|DATA
//
// New in V1:
//   PEERS_REQUEST  — ask peer for their peer list
//   PEERS_RESPONSE — respond with known peer list (CSV of host:port)
//   MEMPOOL_REQ    — request peer's mempool
//   MEMPOOL_RESP   — respond with mempool transactions

enum class MsgType {
    HANDSHAKE,
    TX,
    BLOCK,
    CHAIN_REQUEST,
    CHAIN_RESPONSE,
    PEERS_REQUEST,
    PEERS_RESPONSE,
    MEMPOOL_REQ,
    MEMPOOL_RESP,
    PING,
    PONG,
    UNKNOWN
};

struct Message {
    MsgType     type;
    std::string senderId;
    std::string data;

    std::string serialize() const;
    static Message parse(const std::string& line);
    static std::string typeToStr(MsgType t);
    static MsgType     strToType(const std::string& s);
};