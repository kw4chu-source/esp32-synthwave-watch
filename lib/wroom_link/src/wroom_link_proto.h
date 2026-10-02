#pragma once
// Protokol ESP-NOW Cardputer <-> projekty WROOM.
// Wspolny dla obu stron (Cardputer linkuje ta sama biblioteke).

#include <stdint.h>
#include <string.h>
#include <mbedtls/md.h>

namespace wroom_link {

constexpr uint8_t MAGIC0 = 'W';
constexpr uint8_t MAGIC1 = 'L';
constexpr uint8_t PROTO_VERSION = 1;

enum class MsgType : uint8_t {
  Discover      = 0x01,  // Cardputer -> broadcast
  DiscoverReply = 0x02,  // WROOM -> Cardputer: nazwa + wersja
  CmdLoader     = 0x10,  // Cardputer -> WROOM: przejdz do loadera (HMAC)
  Ack           = 0x11,
};

enum AckCode : uint8_t { ACK_OK = 0, ACK_BAD_HMAC = 1, ACK_REPLAY = 2 };

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

// hmac = HMAC-SHA256(klucz, header || nonce || MAC odbiorcy)
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

inline Header makeHeader(MsgType type, uint32_t seq) {
  return Header{{MAGIC0, MAGIC1}, PROTO_VERSION, type, seq};
}

inline bool headerValid(const Header& h) {
  return h.magic[0] == MAGIC0 && h.magic[1] == MAGIC1 && h.proto == PROTO_VERSION;
}

inline void cmdLoaderHmac(const char* key, const CmdLoaderMsg& m, const uint8_t target[6],
                          uint8_t out[32]) {
  uint8_t buf[sizeof(Header) + sizeof(uint32_t) + 6];
  memcpy(buf, &m.h, sizeof(Header));
  memcpy(buf + sizeof(Header), &m.nonce, sizeof(uint32_t));
  memcpy(buf + sizeof(Header) + sizeof(uint32_t), target, 6);
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  reinterpret_cast<const uint8_t*>(key), strlen(key), buf, sizeof(buf), out);
}

// Porownanie w stalym czasie
inline bool hmacEqual(const uint8_t a[32], const uint8_t b[32]) {
  uint8_t d = 0;
  for (int i = 0; i < 32; i++) d |= a[i] ^ b[i];
  return d == 0;
}

}  // namespace wroom_link
