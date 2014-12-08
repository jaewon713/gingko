/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file    TaskDB.cpp
 *
 *  @author liuming03
 *  @date   2013-5-6
 */

#include "bbts/agent/TaskDB.h"

#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/thread.hpp>
#include <../sqlite/sqlite3.h>

#include <bbts/log.h>

namespace bbts {
namespace agent {

using std::string;
using boost::asio::io_service;
using boost::shared_ptr;
using boost::system::error_code;
using boost::posix_time::hours;
typedef boost::mutex::scoped_lock scoped_lock;

int TaskDB::DEL_DATA_TIMER_INTERVAL = 24;

TaskDB::TaskDB(const string &db_file_name, io_service &io_service, int delete_data_interval) :
    db_file_name_(db_file_name), delete_data_interval_(delete_data_interval),
    reconnecting_(false), del_data_timer_(io_service) {
  Reconnect();
  DeleteDataTimerCallback(error_code());
}

TaskDB::~TaskDB() {
}

bool TaskDB::Reconnect() {
  {
    scoped_lock lock(reconnecting_mutex_);
    if (reconnecting_) {
      while (reconnecting_) {
        reconnecting_cond_.wait(lock);
      }
      return true;
    }
    reconnecting_ = true;
  }

  sqlite3 *sqlite_db = NULL;
  if (sqlite3_open(db_file_name_.c_str(), &sqlite_db) != 0) {
    WARNING_LOG("Can't open db file %s: %s.", db_file_name_.c_str(), sqlite3_errmsg(sqlite_db));
    sqlite3_close(sqlite_db);
    return false;
  }
  NOTICE_LOG("open db file %s success.", db_file_name_.c_str());
  {
    scoped_lock lock(task_db_mutex_);
    task_db_.reset(sqlite_db, boost::bind(&sqlite3_close, _1));
  }

  {
    scoped_lock lock(reconnecting_mutex_);
    reconnecting_ = false;
  }
  reconnecting_cond_.notify_all();

  if (!InitTable()) {
    WARNING_LOG("init table failed.");
    return false;
  }
  return true;
}

bool TaskDB::Excute(const string &sql, callback_t callback, void *userdata) {
  shared_ptr<sqlite3> db;
  {
    scoped_lock lock(task_db_mutex_);
    db = task_db_;
  }

  char *err = NULL;
  if (sqlite3_exec(db.get(), sql.c_str(), callback, userdata, &err) != SQLITE_OK) {
    WARNING_LOG("SQL error: %s", err);
    sqlite3_free(err);
    return false;
  }
  return true;
}

bool TaskDB::InitTable() {
  string sql = "CREATE TABLE IF NOT EXISTS task ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT"
      ", infohash CHAR(40) DEFAULT ''"
      ", save_path VARCHAR(1024) DEFAULT ''"
      ", cmd VARCHAR(4096) DEFAULT ''"
      ", status VARCHAR(20) DEFAULT ''"
      ", uid INT(10) DEFAULT -1"
      ", gid INT(10) DEFAULT -1"
      ", start_time TIMESTAMP DEFAULT ''"
      ", end_time TIMESTAMP DEFAULT ''"
      ", upload INT(20) DEFAULT 0"
      ", download INT(20) DEFAULT 0"
      ", progress INT(5) DEFAULT 0"
      ", error varchar(1024) DEFAULT ''"
      ")";

  string sql_config = "CREATE TABLE IF NOT EXISTS agent_config ("
      "bind_port INT(10) DEFAULT 422"
      ", upload_limit INT(10) DEFAULT 1000"
      ", connection_limit INT(10) DEFAULT 50000"
      ")";
  if (!this->Excute(sql_config)) {
    WARNING_LOG("create table agent_config failed");
  }

  return this->Excute(sql);
}

void TaskDB::DeleteDataTimerCallback(const error_code &ec) {
  if (ec) {
    return;
  }
  std::stringstream sql;
  sql << "DELETE FROM task WHERE end_time != '' and end_time < datetime('now', 'localtime', '-"
      << delete_data_interval_ << " day')";
  this->Excute(sql.str());
  del_data_timer_.expires_from_now(hours(DEL_DATA_TIMER_INTERVAL));
  del_data_timer_.async_wait(boost::bind(&TaskDB::DeleteDataTimerCallback, this, boost::asio::placeholders::error()));
}

} // namespace agent
} // namespace bbts

