#include "bbts/encode.h"

#include <stdint.h>

#include <string>
#include <iostream>
#include <sstream>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

using std::string;
using std::stringstream;
using std::ostream_iterator;

using boost::archive::iterators::transform_width;
using boost::archive::iterators::base64_from_binary;
using boost::archive::iterators::binary_from_base64;

typedef base64_from_binary<transform_width<string::const_iterator, 6, 8> > Base64EncodeIterator;
typedef transform_width<binary_from_base64<string::const_iterator>, 8, 6> Base64DecodeIterator;

namespace bbts {

bool Base64Encode(const string& input, string* output) {
  if (input.empty()) {
    output->clear();
    return true;
  }

  stringstream result;
  std::copy(Base64EncodeIterator(input.begin()),
            Base64EncodeIterator(input.end()),
            ostream_iterator<char>(result));
  size_t equal_count = (3 - input.length() % 3) % 3;
  for (size_t i = 0; i < equal_count; i++) {
    result.put('=');
  }
  *output = result.str();
  if (output->empty()) {
    return false;
  }
  return true;
}

bool Base64Decode(const string& input, string* output) {
  unsigned int size = input.size();
  // Remove the padding characters, cf. https://svn.boost.org/trac/boost/ticket/5629
  if (size && input[size - 1] == '=') {
    --size;
    if (size && input[size - 1] == '=') --size;
  }
  if (size == 0) {
    output->clear();
    return true;
  }

  stringstream result;
  try {
    std::copy(Base64DecodeIterator(input.begin()),
              Base64DecodeIterator(input.begin() + size),
              ostream_iterator<char>(result));
  } catch(...) {
    return false;
  }
  *output = result.str();
  if (output->empty()) {
    return false;
  }
  return true;
}

inline static bool HexcharToDigit(char c, uint8_t *digit) {
  assert(digit);
  if (c >= '0' && c <= '9') {
    *digit = c - '0';
  } else if (c >= 'a' && c <= 'f') {
    *digit = c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    *digit = c - 'A' + 10;
  } else {
    return false;
  }
  return true;
}

inline static char DigitToHex(uint8_t digit) {
  static const char hexchars[] = { '0', '1', '2', '3', '4', '5'
      , '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  return hexchars[digit & 0x0f];
}

bool HexstrToBytes(const string &hex, string* bytes) {
  assert(bytes);
  string::size_type hex_len = hex.length();
  if (hex_len % 2 != 0) {
    return false;
  }

  bytes->clear();
  for (string::size_type i = 0; i < hex_len; i += 2) {
    uint8_t digit, byte;
    if (!HexcharToDigit(hex[i], &digit)) {
      return false;
    }
    byte = digit << 4;
    if (!HexcharToDigit(hex[i + 1], &digit)) {
      return false;
    }
    byte += digit;
    bytes->append(1, byte);
  }
  return true;
}

bool BytesToHex(const string &bytes, string* hex) {
  assert(hex);
  hex->clear();
  string::size_type bytes_len = bytes.length();
  for (string::size_type i = 0; i < bytes_len; ++i) {
    uint8_t byte = bytes[i];
    char c = DigitToHex((byte & 0xf0) >> 4);
    hex->append(1, c);
    c = DigitToHex(byte & 0x0f);
    hex->append(1, c);
  }
  return true;
}

bool HexToBase64(const string &hex, string* base64str) {
  assert(base64str);
  string bytes;
  if (!HexstrToBytes(hex, &bytes)) {
    return false;
  }
  return Base64Encode(bytes, base64str);
}

bool Base64ToHex(const string &base64str, string* hex) {
  assert(hex);
  string bytes;
  if (!Base64Decode(base64str, &bytes)) {
    return false;
  }
  return BytesToHex(bytes, hex);
}

} // namespace bbts
