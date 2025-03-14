# Author: Tu Le
#  Date: 3/13/2025
#  CS4760 Project 3

CC = gcc
CFLAGS = -Wall -g
OSS_TARGET = oss
WORKER_TARGET = worker

OSS_OBJS = oss.o
WORKER_OBJS = worker.o

all: $(OSS_TARGET) $(WORKER_TARGET)

$(OSS_TARGET): $(OSS_OBJS)
	$(CC) $(CFLAGS) -o $(OSS_TARGET) $(OSS_OBJS) -lrt

$(WORKER_TARGET): $(WORKER_OBJS)
	$(CC) $(CFLAGS) -o $(WORKER_TARGET) $(WORKER_OBJS) -lrt

oss.o: oss.c
	$(CC) $(CFLAGS) -c oss.c

worker.o: worker.c
	$(CC) $(CFLAGS) -c worker.c

# Apply clean rule
clean: 
	rm -f $(OSS_TARGET) $(WORKER_TARGET) *.o
