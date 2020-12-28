# Makefile for libcbinder

SRCS+= src/binder_hal.c
SRCS+= src/binder_io.c
SRCS+= src/binder_ipc.c

CFLAGS+= -O3
CFLAGS+= -Wall -Wno-unused-parameter
CFLAGS+= -I$(shell pwd)/include


SVC_SRC := service_manager/svc_mgr.c

LDFLAGS= -static 

LIBS= -lpthread

TOOLCHAIN=
CC= $(TOOLCHAIN)gcc

OBJS= $(SRCS:%.c=%.o)

all: libcbinder.a svc_mgr test

libcbinder.a: $(OBJS)
	$(AR) -cr $@ $^

svc_mgr: libcbinder.a
	$(CC) $(CFLAGS) $(SVC_SRC) $^ -o $@ $(LIBS) 

test: libcbinder.a
	$(CC) $(CFLAGS) test/fd_service.c $^ -o fd_service $(LIBS) 
	$(CC) $(CFLAGS) test/fd_client.c  $^ -o fd_cleint  $(LIBS)

clean:
	rm -rf libcbinder.a svc_mgr fd_service fd_cleint $(OBJS)
