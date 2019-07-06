CC=clang++
CFLAGS=-g -Wall
LIBLINK=-lwiringPi
OBF=-c
SRC=src/

all: server #client

client: $(SRC)client/client.go
	go build $(SRC)client/client.go

server: $(SRC)server/common.o $(SRC)server/log.o $(SRC)server/server.o \
$(SRC)server/RCSwitch.o $(SRC)server/daemon.o
	$(CC) $(CFLAGS) $(LIBLINK) $(SRC)server/common.o \
	$(SRC)server/log.o $(SRC)server/RCSwitch.o \
	$(SRC)server/server.o $(SRC)server/daemon.o -o server

common.o: $(SRC)server/common.cpp $(SRC)server/common.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)server/common.cpp

log.o: $(SRC)server/log.cpp $(SRC)server/log.h $(SRC)server/common.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)server/log.cpp

server.o: $(SRC)server/server.cpp $(SRC)server/server.h $(SRC)server/log.h $(SRC)server/common.h
	$(CC) $(OBF) $(CFLAG) $(SRC)server/server.cpp

RCSwitch.o: $(SRC)server/RCSwitch.cpp $(SRC)server/RCSwitch.h $(SRC)server/log.h
	$(CC) $(OBF) $(CFLAG) $(SRC)server/RCSwitch.cpp

daemon.o: $(SRC)server/daemon.cpp $(SRC)server/daemon.h $(SRC)server/common.h $(SRC)server/log.h
	$(CC) $(OBF) $(CFLAG) $(SRC)server/daemon.cpp

clean:
	rm -f $(SRC)server/*.o client server
