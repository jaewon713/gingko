#include "bbts/encode_util.h"

#include <assert.h>
#include <stdint.h>

#include <string>

#include "bbts/encode.h"

using std::string;

namespace bbts {

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
  if (base64_encode(bytes, base64str) != 0) {
    return false;
  }
  return true;
}

bool Base64ToHex(const string &base64str, string* hex) {
  assert(hex);
  string bytes;
  if (base64_decode(base64str, &bytes) != 0) {
    return false;
  }
  if (!BytesToHex(bytes, hex)) {
    return false;
  }
  return true;
}

} // namespace bbts
