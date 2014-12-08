/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/*
 * @file   path.h
 *
 * @author liuming03
 * @date   2013-4-9
 * @brief  关于文件路径的一些操作集
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_PATH_H_
#define OP_OPED_NOAH_TOOLS_BBTS_PATH_H_

#include <assert.h>
#include <string>
#include <sys/stat.h>

namespace bbts {
/* 路径最大长度 */
static const int MAX_PATH_LEN = 4096;

/**
 * @brief  获取当前程序的绝对路径
 *
 * @param  self_path(out) 成功将本程序实际路径全名放入self_path中
 * @return 成功返回true，失败false
 */
bool get_self_path(std::string *self_path);

/**
 * @brief  获取当前工作目录
 *
 * @param  cwd(out) 获取当前工作目录
 * @return 成功返回true，失败false
 */
bool get_current_working_dir(std::string *cwd);

/**
 * @brief  去掉path末尾多余的斜杠'/'
 *
 * @param  path(in) 待过滤的路径
 * @return 返回过滤末尾'/'后的path
 */
std::string trim_back_slash(const std::string &path);

/**
 * @brief  去掉path头部多余的斜杠'/'
 *
 * @param  path(in) 待过滤的路径
 * @return 返回过滤头部'/'后的path
 */
std::string trim_front_slash(const std::string &path);

/**
 * @brief 分离给定path中父路径和最后一级路径名
 *
 * @param  path(in)         给定的路径
 * @param  parent_path(out) path的父路径
 * @param  short_name (out) path的最后一级路径
 * @return 成功true，失败false
 */
bool path_slipt(const std::string &path, std::string *parent_path, std::string *short_name);

/**
 * @brief  取得指定目录的父目录
 *
 * @param  path(in)  给定目录
 * @return 存在父目录则返回父目录，否则返回空串
 */
std::string parent_path(const std::string &path);
/**
 * @brief  取得某个路径的最后一级路径名
 *
 * @param  path(in) 源文件路径
 * @return 返回最后一级路径名
 */
std::string file_name(const std::string &path);

/**
 * @brief  取得某个路径的子路径，即subpath("abc/def") == "def"
 *
 * @param  path(in) 源文件路径
 * @return 返回子路径名
 */
std::string subpath(const std::string &path);

/**
 * @brief  获取当前进程用户的$HOME目录
 *
 * @param  home(out) 成功则返回$HOME目录
 * @return 成功返回true，失败返回false
 */
bool get_home_path(std::string *home_path);

/**
 * @brief  取得当前工作根目录
 *  假定的目录结构为$root/bin/program，根据program找到$root
 *
 * @param  root_path 成功返回根目录
 * @return 成功返回true，失败返回false
 */
bool get_root_path(std::string *root_path);

/**
 * @brief  补全为绝对路径
 *
 * @param  path(in) 某个给定路径
 * @return 转换为绝对路径，即如果是相对目录，则用当前工作目录补全
 */
std::string complete_full_path(const std::string &path);

/**
 * @brief  给路径中插入反斜杠,以适应正则表达式, 如/a/b/c转换成\/a\/b\/c
 *
 * C字符串反斜杠会被转移，所以实际上是代码中加了两个反斜杠
 * @param  path(in) 某个路径
 * @return 转换后的path
 */

std::string insert_back_slash(const std::string &path);

/**
 * @brief  判断给定路径是否为绝对路径
 *
 * @param  path 某个路径
 * @return true: 是绝对路径; false: 不是绝对路径
 */
bool is_absolute_path(const std::string &path);

/**
 * @brief  整理用户输入的路径，去掉多余的/ . ..
 *
 * @param  path(in) 源路径
 * @return 如果是正确的路径则返回过滤后的，否则返回空
 */
std::string trim_path(const std::string &path);

/**
 * @brief  检查给定目录是否存在，不存在则创建（非递归）
 *
 * @param  path(in)  目录
 * @param  mode(in)  创建目录的权限
 * @return 成功返回true，失败返回false
 */
bool check_and_mkdir(const std::string &path, mode_t mode);

/**
 * @brief  检查给定目录是否存在，不存在则递归创建
 *
 * @param  path(in)  目录
 * @param  mode(in)  创建目录的权限
 * @return 成功返回true，失败返回false
 */
bool check_and_mkdir_r(const std::string &path, mode_t mode);

} /* namespace bbts */

#endif // OP_OPED_NOAH_TOOLS_BBTS_PATH_H_
