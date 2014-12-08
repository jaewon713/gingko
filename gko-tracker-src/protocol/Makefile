WORKROOT = ../../../../../../
INCLUDES =-I$(WORKROOT)/op/oped/noah/thirdparty/thrift/output/include \
					-I$(WORKROOT)/third-64/boost/include  \
 					-I$(WORKROOT)/third-64/libevent/include  \
					-I./

CPPFLAGS = -g -DHAVE_NETINET_IN_H -Wall -pipe -fpermissive $(INCLUDES)
AR       = ar
CC       = g++ $(CPPFLAGS)

MODULE_NAME =  libbbts_protocol.a
OBJ = tracker_constants.o tracker_types.o announce_constants.o announce_types.o Announce.o \
		inner_constants.o inner_types.o 


all : $(MODULE_NAME)
	cp $(MODULE_NAME) ../output/lib/
	cp *.h  ../output/include/bbts/tracker/

$(MODULE_NAME) : $(OBJ)
	$(AR) rcv $@ $^

%.o	: %.cc
	$(CC) -c $< -o $@
%.o	: %.cpp
	$(CC) -c $< -o $@
	
clean:
	rm -rf  $(MODULE_NAME) *.o
