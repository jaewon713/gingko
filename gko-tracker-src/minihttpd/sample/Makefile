TOP = ..
THIRDPARTY = $(TOP)/..
CPPFLAGS     = -g -Wall -pipe $(INCLUDES)
CC 		     = g++ $(CPPFLAGS)


INCLUDES =  -I$(TOP)/output/include \
			 -I$(THIRDPARTY)/libevent/include/  \
			 -I$(THIRDPARTY)/glog/include/ \
			 -I$(THIRDPARTY)/gtest/include/ \
			-I./
LIBS     =  -L$(TOP)/output/lib  -lminihttpd \
				-L$(THIRDPARTY)/libevent/lib/  -levent \
				-L$(THIRDPARTY)/glog/lib -lglog \
				-L$(THIRDPARTY)/gtest/lib -lgtest \
			-lpthread -lrt \

MODULE_NAME1 = httpd_test
OBJ1 = httpd_test.o

MODULE_NAME2 = timer_test
OBJ2 = timer_test.o

MODULE_NAME3 = timer_in_mt_test
OBJ3 = timer_in_mt_test.o

MODULE_NAME4 = eventloop_test
OBJ4 = eventloop_test.o

all : $(MODULE_NAME1) $(MODULE_NAME2) $(MODULE_NAME3) $(MODULE_NAME4)
	cp $(MODULE_NAME1) ../output/sample
	cp $(MODULE_NAME2) ../output/sample
	cp $(MODULE_NAME3) ../output/sample
	cp $(MODULE_NAME4) ../output/sample
	cp *.cpp ../output/sample
	rm -rf *.o

$(MODULE_NAME1) : $(OBJ1)
	$(CC) -o $@ $^ $(LIBS)
$(MODULE_NAME2) : $(OBJ2)
	$(CC) -o $@ $^ $(LIBS)
$(MODULE_NAME3) : $(OBJ3)
	$(CC) -o $@ $^ $(LIBS)
$(MODULE_NAME4) : $(OBJ4)
	$(CC) -o $@ $^ $(LIBS)

%.o	: %.cc 
	$(CC) -c $< -o $@
%.o	: %.cpp 
	$(CC) -c $< -o $@
	
clean:
	rm -rf  $(MODULE_NAME1)
	rm -rf  $(MODULE_NAME2)
	rm -rf  $(MODULE_NAME3)
	rm -rf  $(MODULE_NAME4)
	rm -rf ./*.o
