WORKROOT = ../../../../../../
INCLUDES = -I$(WORKROOT)/op/oped/noah/hermes/bbts-tracker/include \
					-I$(WORKROOT)/op/oped/noah/hermes/bbts-tracker/protocol \
					-I$(WORKROOT)/op/oped/noah/lib/hiredis/output/include \
					-I$(WORKROOT)/op/oped/noah/thirdparty/thrift/output/include/ \
					-I$(WORKROOT)/op/oped/noah/thirdparty/jemalloc/output/include/ \
					-I$(WORKROOT)/op/oped/noah/argus/lib/mini-httpd/output/include/ \
					-I$(WORKROOT)/third-64/glog/include/   \
					-I$(WORKROOT)/third-64/protobuf/include/   \
					-I$(WORKROOT)/third-64/libevent/include/  \
					-I$(WORKROOT)/third-64/gflags/include \
					-I$(WORKROOT)/third-64/boost/include/  \
					-I$(WORKROOT)/third-64/boost/include/boost/tr1 \
					-I./

LIBS	= -L$(WORKROOT)/op/oped/noah/hermes/bbts-tracker/output/lib -lbbts_protocol \
				-L$(WORKROOT)/op/oped/noah/thirdparty/thrift/output/lib  -lthriftnb -lthrift -lthriftz\
				-L$(WORKROOT)/op/oped/noah/thirdparty/jemalloc/output/lib -ljemalloc \
				-L$(WORKROOT)/op/oped/noah/argus/lib/mini-httpd/output/lib/ -lminihttpd \
				-L$(WORKROOT)/op/oped/noah/lib/hiredis/output/lib/ -lhiredis \
				-L$(WORKROOT)/third-64/glog/lib -lglog \
				-L$(WORKROOT)/third-64/gflags/lib -lgflags \
				-L$(WORKROOT)/third-64/protobuf/lib -lprotobuf \
				$(WORKROOT)/third-64/libevent/lib/libevent.a \
				$(WORKROOT)/third-64/boost/lib/libboost_system.a \
				$(WORKROOT)/third-64/boost/lib/libboost_thread.a \
				-L$(WORKROOT)/third-64/openssl/lib/ -lcrypto\
				-lpthread -lrt \

CPPFLAGS = -g -DHAVE_NETINET_IN_H -Wall -pipe -fpermissive -DHAVE_NETDB_H=1  $(INCLUDES) -DVERSION="\"${SCMPF_MODULE_VERSION}\""
AR       = ar
CC       = g++ $(CPPFLAGS)

MODULE_NAME =  bbts_tracker
OBJ = redis_conf.pb.o tracker_conf.pb.o hash_crc32.o encode.o HttpServer.o RedisManager.o encode_util.o \
		PeerHandler.o PeerInfo.o TrackerErrorCategory.o AnnounceHandler.o \
		InfoHashGarbageCleaner.o RemotePeersSyncronizer.o StatusManager.o main.o


all : $(MODULE_NAME)
	cp $(MODULE_NAME) ../output/bin/

$(MODULE_NAME) : $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

%.o	: %.cc
	$(CC) -c $< -o $@
%.o	: %.cpp
	$(CC) -c $< -o $@
	
clean:
	rm -rf  $(MODULE_NAME) *.o
