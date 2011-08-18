PREFIX=
INCLUDES= -I/usr/include/mysql
LIB_PREFIX=lib
TARGETDIR= /usr/lib64/mysql/plugin

all: json_get

json_get:
	gcc -Wall -s -O2 -o $(LIB_PREFIX)json_get.so -shared -fPIC $(INCLUDES) jsonget.c -lyajl
