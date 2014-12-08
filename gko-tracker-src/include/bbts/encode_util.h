/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   encode_util.h
 *
 * @author liuming03
 * @date   2014年6月5日
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_ENCODE_UTIL_H_
#define OP_OPED_NOAH_BBTS_ENCODE_UTIL_H_

#include <string>

namespace bbts {

bool BytesToHex(const std::string &bytes, std::string* hex);

bool HexToBase64(const std::string &hex, std::string* base64str);

bool Base64ToHex(const std::string &base64str, std::string* hex);

} // namespace bbts

#endif // OP_OPED_NOAH_BBTS_ENCODE_UTIL_H_
