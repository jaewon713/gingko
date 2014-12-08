TOP=..
include ../Makefile.def

LOCAL_OBJ = sqlite3.o

all : $(LOCAL_OBJ)

%.o : %.c %.h
	gcc -c -I./ $< -D_XOPEN_SOURE=500 -D_GNU_SOURCE -g -O2 -W -WALL -Wno-unused-parameter -Winline \
         -fPIC -pipe -Wpointer-arith -fPIC -pipe -Wswitch -Wformat -Wshadow -Wredundant-decls

.PHONY : clean

clean :
	rm -rf $(LOCAL_OBJ)
