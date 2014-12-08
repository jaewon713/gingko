#ifndef OP_OPED_NOAH_BBTS_TRACKER_REMOTE_PEERS_SYNCRONIZER_
#define OP_OPED_NOAH_BBTS_TRACKER_REMOTE_PEERS_SYNCRONIZER_

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

struct redisReply;

namespace bbts {
namespace tracker {

struct InfoHashQueue;
struct SingleInfoHashInfo;
typedef boost::unordered_map<std::string, boost::shared_ptr<SingleInfoHashInfo> > InfoHashMap;

class RemotePeersSyncronizer : private boost::noncopyable {
 public:
  static void ThreadFunc(boost::shared_ptr<InfoHashQueue> queue_to_syncronize,
                         boost::shared_ptr<InfoHashMap> remote_map);

  static void UpdateInfoHashMapByReply(const std::string &key, redisReply *reply,
                                       const boost::shared_ptr<InfoHashMap> &info_hash_map);

 private:
  RemotePeersSyncronizer();
  ~RemotePeersSyncronizer();

  const static uint32_t MAX_HANDLE_INFO_HASHS = 1;
};

} // namespace tracker
} // namespace bbts


#endif // OP_OPED_NOAH_BBTS_TRACKER_REMOTE_PEERS_SYNCRONIZER_
