/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   CUrl.cpp
 *
 * @author liuming03
 * @date   2013-1-24
 */

#include "bbts/CUrl.h"

#include <assert.h>

#include <string>

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/types.h>

namespace bbts {

using std::string;

CUrl::CUrl() : curl_(NULL), chunk_(NULL) {
}

CUrl::~CUrl() {
}

bool CUrl::Init() {
  curl_ = curl_easy_init();
  if (!curl_) {
    return false;
  }

  chunk_ = curl_slist_append(chunk_, "Accept-Encoding:gzip, deflate");
  chunk_ = curl_slist_append(chunk_, "User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:16.0) Gecko/20100101 Firefox/16.0");
  chunk_ = curl_slist_append(chunk_, "Connection: Keep-Alive");
  if (!chunk_) {
    return false;
  }

  curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, chunk_);
  curl_easy_setopt(curl_, CURLOPT_ENCODING, "gzip");
  curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 60);
  curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 3);
  return true;
}

void CUrl::Destroy() {
  if (chunk_) {
    curl_slist_free_all(chunk_);
  }
  chunk_ = NULL;
  if (curl_) {
    curl_easy_cleanup(curl_);
  }
  curl_ = NULL;
}

static int ReadData(void *buffer, size_t size, size_t nb, void* stream) {
  string &res = *(static_cast<string *>(stream));

  size_t len = size * nb;
  if (len == 0 || !buffer) {
    return len;
  }

  res.append(static_cast<char *>(buffer), len);
  return len;
}

int CUrl::Get(const string &url, string *response) {
  assert(response);
  if (!Init()) {
    return CURLE_FAILED_INIT;
  }

  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, ReadData);
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, response);
  CURLcode ret = curl_easy_perform(curl_);
  Destroy();
  return ret;
}

int CUrl::Post(const string &url, int port, const string &post_data, string *response) {
  assert(response);
  if (!Init()) {
    return CURLE_FAILED_INIT;
  }

  curl_easy_setopt(curl_, CURLOPT_POST, port);
  curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, post_data.c_str());
  curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, post_data.size());
  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, ReadData);
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, response);
  CURLcode ret = curl_easy_perform(curl_);
  Destroy();
  return ret;
}

const char *CUrl::StringError(int ec) {
    return curl_easy_strerror(static_cast<CURLcode>(ec));
}

} // namespace bbts
