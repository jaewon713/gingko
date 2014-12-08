/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   common_def.h
 *
 * @author liuming03
 * @date   2013-4-13
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_AGENT_COMMON_DEF_H_
#define OP_OPED_NOAH_TOOLS_BBTS_AGENT_COMMON_DEF_H_

#ifndef GINGKO_VERSION
#define GINGKO_VERSION "unknow version"
#endif

#if defined(__DATE__) && defined(__TIME__)
static const char BUILD_DATE[] = __DATE__ " " __TIME__;
#else
static const char BUILD_DATE[] = "unknown"
#endif

#endif // OP_OPED_NOAH_TOOLS_BBTS_AGENT_COMMON_DEF_H_
