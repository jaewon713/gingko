#ifndef OP_OPED_NOAH_BBTS_ENCODE_H_
#define OP_OPED_NOAH_BBTS_ENCODE_H_

#include <string>

namespace bbts {

bool Base64Encode(const std::string &input, std::string *output);

bool Base64Decode(const std::string &input, std::string *output);

bool BytesToHex(const std::string &bytes, std::string* hex);

bool HexToBase64(const std::string &hex, std::string* base64str);

bool Base64ToHex(const std::string &base64str, std::string* hex);

} // namespace bbts

#endif /* OP_OPED_NOAH_BBTS_ENCODE_H_ */
