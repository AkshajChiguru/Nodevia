#include "network/message.h"

std::string Message::typeToStr(MsgType t) {
    switch (t) {
        case MsgType::HANDSHAKE:       return "HANDSHAKE";
        case MsgType::TX:              return "TX";
        case MsgType::BLOCK:           return "BLOCK";
        case MsgType::CHAIN_REQUEST:   return "CHAIN_REQUEST";
        case MsgType::CHAIN_RESPONSE:  return "CHAIN_RESPONSE";
        case MsgType::PEERS_REQUEST:   return "PEERS_REQUEST";
        case MsgType::PEERS_RESPONSE:  return "PEERS_RESPONSE";
        case MsgType::MEMPOOL_REQ:     return "MEMPOOL_REQ";
        case MsgType::MEMPOOL_RESP:    return "MEMPOOL_RESP";
        case MsgType::PING:            return "PING";
        case MsgType::PONG:            return "PONG";
        default:                       return "UNKNOWN";
    }
}

MsgType Message::strToType(const std::string& s) {
    if (s == "HANDSHAKE")       return MsgType::HANDSHAKE;
    if (s == "TX")              return MsgType::TX;
    if (s == "BLOCK")           return MsgType::BLOCK;
    if (s == "CHAIN_REQUEST")   return MsgType::CHAIN_REQUEST;
    if (s == "CHAIN_RESPONSE")  return MsgType::CHAIN_RESPONSE;
    if (s == "PEERS_REQUEST")   return MsgType::PEERS_REQUEST;
    if (s == "PEERS_RESPONSE")  return MsgType::PEERS_RESPONSE;
    if (s == "MEMPOOL_REQ")     return MsgType::MEMPOOL_REQ;
    if (s == "MEMPOOL_RESP")    return MsgType::MEMPOOL_RESP;
    if (s == "PING")            return MsgType::PING;
    if (s == "PONG")            return MsgType::PONG;
    return MsgType::UNKNOWN;
}

std::string Message::serialize() const {
    return typeToStr(type) + "|" + senderId + "|" + data;
}

Message Message::parse(const std::string& line) {
    Message m; m.type = MsgType::UNKNOWN;
    size_t p1 = line.find('|');
    if (p1 == std::string::npos) return m;
    size_t p2 = line.find('|', p1 + 1);
    if (p2 == std::string::npos) return m;
    m.type     = strToType(line.substr(0, p1));
    m.senderId = line.substr(p1 + 1, p2 - p1 - 1);
    m.data     = line.substr(p2 + 1);
    return m;
}