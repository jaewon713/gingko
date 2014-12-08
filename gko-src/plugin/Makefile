TOP=../
include ../Makefile.def

#JRE_HOME = /home/opt/java6/jre
#HADOOP_HOME = /home/opt/hadoop
JRE_HOME = /noah/thirdparty/jre6
HADOOP_HOME = /noah/thirdparty/hadoop

SO_OBJ = hdfstool.o
TARGET_LIB = libhdfstool.so
INCLD_DIR = $(TOP)/include/bbts/plugin

HADOOP_LIB = -L$(TOP)/lib
HDFSTOOL_INC = -I$(TOP)/include

all : $(TARGET_LIB)
	cp $(TARGET_LIB) ../lib

$(TARGET_LIB) : $(SO_OBJ)
	g++ -fPIC -shared $(SO_OBJ) $(FLAGS) $(HADOOP_LIB) -Wl,--rpath,$(HADOOP_HOME)/libhdfs,--rpath,$(JRE_HOME)/lib/amd64/server \
		-Wl,--rpath-link=./ -ljsig -ljvm -lhdfs -o $(TARGET_LIB)

jvm_helper.o : jvm_helper.c
	gcc -c -fPIC $(INCLD) $(HADOOP_INK) $<

%.o : %.cpp $(INCLD_DIR)/%.h
	$(CC) -c $(INCLD) $(FLAGS) $<

%.o : %.cpp
	$(CC) -c $(INCLD) $(FLAGS) $<

.PHONY : clean

clean :
	rm -rf $(TARGET_LIB) ../lib/$(TARGET_LIB) $(SO_OBJ)
