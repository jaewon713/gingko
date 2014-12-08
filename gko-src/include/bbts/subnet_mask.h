/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   subnet_mask.h
 *
 * @author liuming03
 * @date   2014-4-10
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_SUBNET_MASK_H_
#define OP_OPED_NOAH_TOOLS_BBTS_SUBNET_MASK_H_

#include <string>

namespace bbts {

bool ParseMaskToIPRange(const std::string &ip, const std::string &mask, std::string *range);

} // namespace bbts

#endif // OP_OPED_NOAH_TOOLS_BBTS_SUBNET_MASK_H_
