/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   file_is_locked.cpp
 *
 * @author liuming03
 * @date   2013-11-13
 * @brief 
 */

#include <fcntl.h>
#include <stdio.h>

int main(int argc, char* argv[])
         {
  if (argc < 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
    return 1;
  }

  int fd = open(argv[1], O_WRONLY, 0600);
  if (fd < 0) {
    fprintf(stderr, "open file %s fail.\n", argv[1]);
    return 2;
  }

  struct flock flockbuf = { F_WRLCK, 0, SEEK_SET, 0, 0 };
  if (fcntl(fd, F_SETLK, &flockbuf) < 0) {
    fprintf(stderr, "flock file %s fail, is locked.\n", argv[1]);
    // 锁失败，则文件已加锁
    return 0;
  }

  fprintf(stdout, "flock file %s success, is unlocked.\n", argv[1]);
  return 3;
}
