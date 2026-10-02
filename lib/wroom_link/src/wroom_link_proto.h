#pragma once
// Protokol ESP-NOW Cardputer <-> projekty WROOM.
// Format ustalony od poczatku, zeby pelna wersja (etap 4) nie zmieniala ramek.

#include <stdint.h>

namespace wroom_link {

constexpr uint8_t MAGIC0 = 'W';
constexpr uint8_t MAGIC1 = 'L';
constexpr uint8_t PROTO_VERSION = 1;

enum class MsgType : uint8_t {
  Discover      = 0x01,  // Cardputer -> broadcast
  DiscoverReply = 0x02,  // WROOM -> Cardputer: nazwa + wersja
  CmdLoader     = 0x10,  // Cardputer -> WROOM: przejdz do loadera (HMAC)
  Ack           = 0x11,
  Nack          = 0x12,
};

struct __attribute__((packed)) Header {
  uint8_t magic[2];
  uint8_t proto;
  MsgType type;
  uint32_t seq;
};

struct __attribute__((packed)) DiscoverMsg {
  Header h;
};

struct __attribute__((packed)) DiscoverReplyMsg {
  Header h;
  char name[32];
  char version[32];
};

// HMAC-SHA256(klucz, header || nonce || mac_odbiorcy)
struct __attribute__((packed)) CmdLoaderMsg {
  Header h;
  uint32_t nonce;
  uint8_t hmac[32];
};

struct __attribute__((packed)) AckMsg {
  Header h;
  uint32_t ackSeq;
  uint8_t code;
};

inline bool headerValid(const Header& h) {
  return h.magic[0] == MAGIC0 && h.magic[1] == MAGIC1 && h.proto == PROTO_VERSION;
}

}  // namespace wroom_link
