#pragma once

#include <cstdlib>

namespace sy {
struct buffer_addr {
  uint8_t addr[3];
} __attribute__((packed));

struct wave_addr {
  uint8_t _0;
  buffer_addr addr;
} __attribute__((packed));

struct file_header {
  uint8_t _0[8];
  uint16_t card_id;
  uint8_t _1[3];
  uint8_t num_waves;
  char name[10];
  uint8_t _2[8];
} __attribute__((packed));

struct wave_header {
  uint8_t _0;
  uint8_t num_samples;
  char name[8];
  char _1[174];
  char _2[18];
  char _3[1280];
} __attribute__((packed));

struct sample_header {
  uint8_t volume;
  uint8_t loop_mode;
  uint8_t orig_key;
  int8_t pitch;
  char _0[10];
  buffer_addr sample_begin;
  buffer_addr loop_begin;
  uint8_t sample_no;
  buffer_addr loop_end;
  uint8_t _1;
  buffer_addr sample_end;
  uint8_t _2[82];
} __attribute__((packed));
}
