#ifndef OP_OPED_NOAH_BBTS_THRIFT_SERIALIZER_H_
#define OP_OPED_NOAH_BBTS_THRIFT_SERIALIZER_H_

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>

//serialize
namespace bbts {

template <typename T>
void ThriftSerializeToString(const T& object, std::string* str) {
  using apache::thrift::protocol::TProtocol;
  using apache::thrift::protocol::TBinaryProtocol;
  using apache::thrift::transport::TMemoryBuffer;
  boost::shared_ptr<TMemoryBuffer> membuffer(new TMemoryBuffer());
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(membuffer));
  object.write(protocol.get());
  str->clear();
  *str = membuffer->getBufferAsString();
}

template <typename T>
void ThriftParseFromString(const std::string& buffer, T* object) {
  using apache::thrift::protocol::TProtocol;
  using apache::thrift::protocol::TBinaryProtocol;
  using apache::thrift::transport::TMemoryBuffer;
  boost::shared_ptr<TMemoryBuffer> membuffer(new TMemoryBuffer());
  membuffer->write(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.length());
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(membuffer));
  object->read(protocol.get());
}

}
#endif // OP_OPED_NOAH_BBTS_THRIFT_SERIALIZER_H_
