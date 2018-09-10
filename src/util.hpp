#pragma once

#include <string>

using byte_t = int8_t;
using ubyte_t = uint8_t;
using word_t = int16_t;
using uword_t = uint16_t;

/*
 * Swaps the endianess of a word-buffer
 */
void swap_word_endianess(std::vector<ubyte_t>& buf) {
  const auto size = buf.size() - buf.size()%2;
  for (std::size_t i = 0; i < size; i+=2) {
    std::swap(buf[i], buf[i+1]);
  }
}

void swap_word_endianess(std::vector<word_t>& buf) {
  std::for_each(buf.begin(), buf.end(), [](auto& w) {
    ubyte_t* b = reinterpret_cast<ubyte_t*>(&w);
    std::swap(b[0], b[1]);
  });
}

/*
 * Converts a byte-array to an unsigned integer value.
 * Example: [0xAA, 0xBB, 0xCC] -> 0x00AABBCC
 *
 * \returns The converted number.
 */
template <class Type, std::size_t Bytes>
Type nbytes_to_u(const ubyte_t (&raw)[Bytes]) {
  static_assert(sizeof(Type) >= Bytes, "Size of target integer must be greater-than or equal the source array size.");
  Type val{};
  for (std::size_t i = 0; i < Bytes; ++i) {
    val <<= 8;
    val  |= raw[i];
  }
  return val;
}

/*
 * Reads a fixed-size, non null-terminated string from the source
 * buffer `buf`. Trailing spaces (`fill_char`) will be removed.
 *
 * \returns The string read
 */
template <std::size_t Len>
std::string get_lstr(const char (&buf)[Len], char fill_char = ' ') {
  std::string str(Len, fill_char);
  std::copy_n(buf, Len, str.begin());
  auto end_loc = str.find_last_not_of(fill_char);
  if (end_loc != std::string::npos) {
    str.erase(end_loc+1);
  }
  return str;
}

/*
 * Writes the content of `str` into the fixed-size buffer `buf`.
 * The target buffer will be filled with spaces (`fill_char`) and not
 * null-terminated.
 * 
 * \returns True if the source strings size was less than or equal the
 *          target buffers size.
 */
template <std::size_t Len>
bool set_lstr(std::string str, char (&buf)[Len], char fill_char = ' ') {
  auto res = str.size() <= Len;
  if (res)
    str.resize(Len, fill_char);
  std::copy_n(str.begin(), Len, buf);
  return res;
}

/*
 * Casts the contents of the buffer `buffer` at offset `pos` to the struct requested.
 * Increments the offset `pos` by the size of the struct, on success.
 * Checks if neither the offset is out of bounds nor the struct is too large.
 * Throws if an error occurs.
 *
 * \returns Pointer to struct at buffer offset; casted to via reinterpret_cast.
 */
template <class Type>
Type* buffer_struct_next(std::vector<ubyte_t>& buffer, std::size_t& offset) {
  if (offset > buffer.size()) {
    throw std::runtime_error("Position out of bounds.");
  } else if (buffer.size() - offset < sizeof(Type)) {
    throw std::runtime_error("Buffer size to small.");
  }

  auto ptr = reinterpret_cast<Type*>(buffer.data()+offset);
  offset += sizeof(Type);
  return ptr;
}

template <class Type>
const Type* buffer_struct_next(const std::vector<ubyte_t>& buffer, std::size_t& offset) {
  return buffer_struct_next<Type>(const_cast<std::vector<ubyte_t>&>(buffer), offset);
}
