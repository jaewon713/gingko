WORKROOT = ../../../../../../
INCLUDES = -I$(WORKROOT)/op/oped/noah/hermes/bbts-tracker/protocol \
					-I$(WORKROOT)/op/oped/noah/hermes/bbts-tracker/include \
					-I$(WORKROOT)/op/oped/noah/thirdparty/thrift/output/include/ \
					-I$(WORKROOT)/third-64/boost/include/  \
					-I$(WORKROOT)/third-64/boost/include/boost/tr1 \
					-I$(WORKROOT)/op/oped/noah/argus/lib/utility/output/include \
					-I./

LIBS  = -L$(WORKROOT)/op/oped/noah/hermes/bbts-tracker/output/lib -lbbts_protocol \
				-L$(WORKROOT)/op/oped/noah/thirdparty/thrift/output/lib  -lthriftnb -lthrift -lthriftz\
				-L$(WORKROOT)/third-64/protobuf/lib -lprotobuf \
				-L$(WORKROOT)/third-64/boost/lib/ -lboost_system -lboost_thread \
				-lpthread -lrt \

CPPFLAGS = -g -Wall -pipe -fpermissive $(INCLUDES)
AR       = ar
CC       = g++ $(CPPFLAGS)

MODULE_NAME1 =  client_mock
OBJ1 = client_mock.o

MODULE_NAME2 =  client_mock_s
OBJ2 = client_mock_s.o

MODULE_NAME3 = client_mock_infohash
OBJ3 = client_mock_infohash.o

MODULE_NAME4 = ControlByInfohash
OBJ4 = ControlByInfohash.o

all : $(MODULE_NAME3) $(MODULE_NAME1) $(MODULE_NAME2) $(MODULE_NAME4)

$(MODULE_NAME3) : $(OBJ3)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME1) : $(OBJ1)
	$(CC) -o $@ $^ $(LIBS)
	
$(MODULE_NAME2) : $(OBJ2)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME4) : $(OBJ4)
	$(CC) -o $@ $^ $(LIBS)

%.o	: %.cc
	$(CC) -c $< -o $@
%.o	: %.cpp
	$(CC) -c $< -o $@
	
clean:
	rm -rf  $(MODULE_NAME3)
	rm -rf  $(MODULE_NAME1)
	rm -rf  $(MODULE_NAME2)
	rm -rf  $(MODULE_NAME4)
	rm -rf ./*.o
