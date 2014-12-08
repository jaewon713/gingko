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
				-L$(THIRDPARTY)/gtest/lib -lgtest -lgtest_main\
			-lpthread -lrt \

MODULE_NAME1 =  closure_test
OBJ1 = closure_test.o

MODULE_NAME2 = current_thread_test
OBJ2 = current_thread_test.o

MODULE_NAME3 = eventloop_test
OBJ3 = eventloop_test.o

MODULE_NAME4 = monitor_helper_test
OBJ4 = monitor_helper_test.o

MODULE_NAME5 = mutex_test
OBJ5 = mutex_test.o

MODULE_NAME6 = spinlock_test
OBJ6 = spinlock_test.o

MODULE_NAME7 = path_test
OBJ7 = path_test.o

MODULE_NAME8 = process_info_test
OBJ8 = process_info_test.o

MODULE_NAME9 = strutil_test
OBJ9 = strutil_test.o

MODULE_NAME10 = timestamp_test
OBJ10 = timestamp_test.o


all : $(MODULE_NAME1) $(MODULE_NAME2) $(MODULE_NAME3) $(MODULE_NAME4) $(MODULE_NAME5) $(MODULE_NAME6) $(MODULE_NAME7) \
	$(MODULE_NAME8) $(MODULE_NAME9) $(MODULE_NAME10)
	cp $(MODULE_NAME1) ../output/test
	cp $(MODULE_NAME2) ../output/test
	cp $(MODULE_NAME3) ../output/test
	cp $(MODULE_NAME4) ../output/test
	cp $(MODULE_NAME5) ../output/test
	cp $(MODULE_NAME6) ../output/test
	cp $(MODULE_NAME7) ../output/test
	cp $(MODULE_NAME8) ../output/test
	cp $(MODULE_NAME9) ../output/test
	cp $(MODULE_NAME10) ../output/test
	cp *.cpp ../output/test
	rm -rf *.o

$(MODULE_NAME1) : $(OBJ1)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME2) : $(OBJ2)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME3) : $(OBJ3)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME4) : $(OBJ4)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME5) : $(OBJ5)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME6) : $(OBJ6)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME7) : $(OBJ7)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME8) : $(OBJ8)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME9) : $(OBJ9)
	$(CC) -o $@ $^ $(LIBS)

$(MODULE_NAME10) : $(OBJ10)
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
	rm -rf  $(MODULE_NAME5)
	rm -rf  $(MODULE_NAME6)
	rm -rf  $(MODULE_NAME7)
	rm -rf  $(MODULE_NAME8)
	rm -rf  $(MODULE_NAME9)
	rm -rf  $(MODULE_NAME10)
	rm -rf ./*.o
