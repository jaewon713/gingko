TOP=..
include ../Makefile.def

INCLD_DIR = ../include/bbts

LOCAL_OBJ = Task.o TaskManager.o util.o WorkerPool.o host_info_util.o ErrorCategory.o CUrl.o \
		socket_util.o message.pb.o TrackerConf.pb.o configure.pb.o path.o bbts_agent.o ThriftTracker.o \
		TaskDB.o number_util.o Syms.o make_torrent.o dump_torrent.o string_util.o BBTSClient.o \
		UnixSocketConnection.o UnixSocketServer.o UnixSocketClient.o delete_files.o BBTSTorrentPlugin.o \
		subnet_mask.o

DEPEN_OBJ = ../sqlite/sqlite3.o ../download/*.o  ../group/*.o
MAIN_OBJ = gko3.o file_is_locked.o #create_padding_torrent.o
PROGRAM  = gko3 file_is_locked #create_padding_torrent

LIBARY  = -lconfig
LIBARY += -lullib
LIBARY += -ltorrent-rasterbar
LIBARY += -lbbts_protocol
LIBARY += -lboost_system
LIBARY += -lboost_thread
LIBARY += -lboost_regex
LIBARY += -lthriftnb
LIBARY += -lthrift
LIBARY += -lpthread
LIBARY += -ldl
LIBARY += -lssl
LIBARY += -lcrypto
LIBARY += -lrt
LIBARY += -lprotobuf
LIBARY += -lcurl
LIBARY += -lbsl
LIBARY += -lspreg
LIBARY += -lsailor_utility
LIBARY += -lhermes_utility

all : $(PROGRAM) 
	cp $(PROGRAM) ../bin

gko3 : $(DEPEN_OBJ) $(LOCAL_OBJ) gko3.o
	$(CC) -o gko3 gko3.o $(LOCAL_OBJ) $(DEPEN_OBJ) $(FLAGS) $(LNK) $(LIBARY)
	
file_is_locked : file_is_locked.o
	$(CC) -o file_is_locked file_is_locked.o

#create_padding_torrent : create_padding_torrent.o
#	$(CC) -o create_padding_torrent create_padding_torrent.o $(FLAGS) $(LNK) -lboost_system -lboost_thread -ltorrent-rasterbar -lpthread -lrt -lullib -ldns_cache -lsailor_utility -lglog

%.o : %.cpp $(INCLD_DIR)/%.h ../include/bbts/bbts_def.h
	$(CC) -c $(INCLD) $(FLAGS) $<

%.o : %.cpp $(INCLD_DIR)/agent/%.h ../include/bbts/bbts_def.h
	$(CC) -c $(INCLD) $(FLAGS) $<
	
%.o : %.cpp $(INCLD_DIR)/tool/%.h ../include/bbts/bbts_def.h
	$(CC) -c $(INCLD) $(FLAGS) $<

%.o : %.cpp
	$(CC) -c $(INCLD) $(FLAGS) $<

.PHONY : clean

clean :
	rm -rf $(LOCAL_OBJ) $(MAIN_OBJ) $(PROGRAM)
