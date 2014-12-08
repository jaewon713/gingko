TOP = ..
THIRDPARTY = $(TOP)/..
INCLUDES = -I$(THIRDPARTY)/libevent/include/  \
			 -I$(THIRDPARTY)/glog/include/ \
			 -I$(THIRDPARTY)/gtest/include/ \
		   -I./

LIBS     = -L$(THIRDPARTY)/libevent/lib/  -levent \
				-L$(THIRDPARTY)/glog/lib -lglog \
				-L$(THIRDPARTY)/gtest/lib -lgtest \
		   -lpthread -lrt -lz

CPPFLAGS = -g -Wall -pipe -static $(INCLUDES)
AR       = ar
CC       = g++ $(CPPFLAGS)

MODULE_NAME = libminihttpd.a
OBJS        = minihttpd.o eventloop.o current_thread.o timestamp.o process_info.o\
							process_inspector.o monitor_helper.o

.PHONY: all clean

all: $(MODULE_NAME)

$(MODULE_NAME) : $(OBJS)
	$(AR) rcv $@ $^
	mkdir -p ../output/lib
	mkdir -p ../output/include/libminihttpd
	cp $(MODULE_NAME) ../output/lib
	cp *.h  ../output/include/libminihttpd

%.o	: %.cc 
	$(CC) -c $< -o $@
%.o	: %.cpp 
	$(CC) -c $< -o $@
	
clean:
	rm -rf $(OBJS) $(MODULE_NAME)

