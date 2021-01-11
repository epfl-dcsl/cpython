CC = gcc
SRCS = $(wildcard *.c)
LIB_OBJS = $(SRCS:.c=.o)
CFLAGS = -Wall -fPIC

ifneq (,$(DEBUG))
override CFLAGS+=-O0 -g
else
override CFLAGS+=-O2
endif

all: $(LIB_OBJS) libsandbox.so

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

libsandbox.so: $(LIB_OBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ *.o
