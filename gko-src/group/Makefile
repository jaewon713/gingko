TOP=..
include ../Makefile.def

LOCAL_OBJ = bbts_group.o GroupConf.pb.o group_manager.o group_manager_service_handler.o \
						group_client.o group_task.o utils.o group_cluster_task.o \
						gen-cpp/GroupManagerService.o gen-cpp/GroupService_constants.o gen-cpp/GroupService_types.o
MAIN_OBJ =
PROGRAM =
PROTOC = $(PROTOBUF)/bin/protoc

INCLD += -I../include/


all : $(DEPEN_OBJ) $(LOCAL_OBJ)

%.o : %.cpp %.h
	$(CC) -c $(INCLD) $(FLAGS) $<

%.o : %.cpp
	$(CC) -c $(INCLD) $(FLAGS) $<

%.o : %.cc %.h
	$(CC) -c $(INCLD) $(FLAGS) $<

.PHONY : clean

proto:
	$(PROTOC) ./GroupConf.proto --cpp_out=./
	$(THRIFT)/output/bin/thrift --gen cpp GroupService.thrift

clean :
	rm -rf $(LOCAL_OBJ) $(MAIN_OBJ) $(PROGRAM) 
