/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   test_ObjectPool.cpp
 *
 * @author liuming03
 * @date   2014年6月30日
 * @brief 
 */

#include <stdio.h>

#include <string>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "ObjectPool.h"

struct Node {
  Node() : a("csuliuming"), b(2), c(3.0) {
    //printf("new node\n");
  }

  ~Node() {
    //printf("delete node\n");
  }

  std::string a;
  int b;
  double c;
};

typedef Node* NodePtr;
typedef boost::shared_ptr<Node> SharedNode;

int test0(int argc, char* argv[]) {
  bbts::ObjectPool<Node> pool(4);
  boost::shared_ptr<Node> nodes[10];
  nodes[0] = pool.GetOneObject();
  {
    nodes[1] = pool.GetOneObject();
    boost::shared_ptr<Node> node;
    node = pool.GetOneObject();
    nodes[2] = pool.GetOneObject();
  }
  nodes[3] = pool.GetOneObject();
  nodes[4] = pool.GetOneObject();
  {
    boost::shared_ptr<Node> node;
    node = pool.GetOneObject();
    boost::shared_ptr<Node> node2;
    node2 = pool.GetOneObject();
  }
  nodes[5] = pool.GetOneObject();
  nodes[6] = pool.GetOneObject();
  nodes[7] = pool.GetOneObject();
  {
    boost::shared_ptr<Node> node;
    node = pool.GetOneObject();
  }
  nodes[8] = pool.GetOneObject();
  {
    boost::shared_ptr<Node> node;
    node = pool.GetOneObject();
    boost::shared_ptr<Node> node2;
    node2 = pool.GetOneObject();
    boost::shared_ptr<Node> node3;
    node3 = pool.GetOneObject();
  }
  nodes[9] = pool.GetOneObject();
  return 0;
}

void time_consuming(const boost::function<void (void)> &func, const std::string &tag) {
  struct timeval tv_begin, tv_end;
  gettimeofday(&tv_begin, NULL);
  func();
  gettimeofday(&tv_end, NULL);
  printf("%s consuming time: %lds%ldus\n",
         tag.c_str(),
         tv_end.tv_sec - tv_begin.tv_sec,
         tv_end.tv_usec - tv_begin.tv_usec);
}

void alloc_n_objs(int n) {
  SharedNode *nodes = new SharedNode[n];
  for (int i = 0; i < n; ++i) {
    nodes[i].reset(new Node());
  }
  delete[] nodes;
}

void alloc_n_objs_by_pool(int n) {
  bbts::ObjectPool<Node> pool(10000);
  int count = 10;
  int k = n / count;
  for (int j = 0; j < count; ++j) {
    SharedNode *nodes = new SharedNode[k];
    for (int i = 0; i < k; ++i) {
      nodes[i] = pool.GetOneObject();
    }
    delete[] nodes;
  }
}

int test1(int argc, char* argv[]) {
  int n = atoi(argv[1]);
  time_consuming(boost::bind(alloc_n_objs, n), "alloc_n_objs");
  time_consuming(boost::bind(alloc_n_objs_by_pool, n), "alloc_n_objs_by_pool");
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("%s testn args...\n", argv[0]);
    return 1;
  }
  typedef int (*func_t)(int argc, char* argv[]);
  func_t funcs[] = {
    test0,
    test1,
  };
  return funcs[atoi(argv[1])](argc - 1, argv + 1);
}

