/*
 * redis_command_creator.h
 *
 *  Created on: 2013-8-20
 *      Author: huxiaoxiao
 */

#ifndef OP_OPED_NOAH_BBTS_REDIS_COMMAND_CREATOR_H_
#define OP_OPED_NOAH_BBTS_REDIS_COMMAND_CREATOR_H_

namespace bbts {

class RedisCommandCreator {
 public:
  static int SetHashOneItemInfo(const std::string &key,
                                const std::string &hash_key,
                                const std::string &value,
                                std::string *command) {
    assert(command);
    command->clear();
    std::stringstream command_stream;
    command_stream<<"hset "<<key.c_str()<<" "<<hash_key.c_str()<<" "<<value.c_str();
    command->append(command_stream.str());
    return 0;
  }

  static int DeleteOneHashItem(const std::string &key,
                               const std::string &hash_key,
                               std::string *command) {
    assert(command);
    command->clear();
    std::stringstream command_stream;
    command_stream<<"hdel "<<key.c_str()<<" "<<hash_key.c_str();
    command->append(command_stream.str());
    return 0;
  }

  static int SetExpire(const std::string &key,
                       const std::string expire_time,
                       std::string *command) {
    assert(command);
    command->clear();
    std::stringstream command_stream;
    command_stream<<"EXPIRE "<<key.c_str()<<" "<<expire_time.c_str();
    command->append(command_stream.str());
    return 0;
  }

  static int SetExpire(const std::string &key, uint32_t expire_time, std::string *command) {
    assert(command);
    command->clear();
    std::stringstream command_stream;
    command_stream<<"EXPIRE "<<key.c_str()<<" "<<expire_time;
    command->append(command_stream.str());
    return 0;
  }

  static int GetHashValueInfo(const std::string &key, std::string *command) {
    assert(command);
    command->clear();
    std::stringstream command_stream;
    command_stream<<"HVALS "<<key.c_str();
    command->append(command_stream.str());
    return 0;
  }
};

}

#endif /* OP_OPED_NOAH_BBTS_REDIS_COMMAND_CREATOR_H_ */
