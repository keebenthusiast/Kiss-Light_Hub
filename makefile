CC=clang++
CFLAGS=-g -Wall
LIBLINK=-lwiringPi -lsqlite3
OBF=-c
SRC=src/

all: kisslight

client: $(SRC)client/client.go
	go build $(SRC)client/client.go

kisslight: $(SRC)server/common.o $(SRC)server/log.o $(SRC)server/server.o \
$(SRC)server/RCSwitch.o $(SRC)server/daemon.o $(SRC)server/ini.o \
$(SRC)server/INIReader.o
	$(CC) $(CFLAGS) $(LIBLINK) $(SRC)server/common.o \
	$(SRC)server/log.o $(SRC)server/RCSwitch.o \
	$(SRC)server/server.o $(SRC)server/daemon.o \
	$(SRC)server/INIReader.o $(SRC)server/ini.o -o server

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

INIReader.o: $(SRC)server/INIReader.cpp $(SRC)server/INIReader.h $(SRC)server/ini.o
	$(CC) $(OBF) $(CFLAG) $(SRC)server/INIReader.cpp

ini.o: $(SRC)server/ini.c $(SRC)server/ini.h
	gcc $(OBF) $(CFLAG) $(SRC)server/ini.c

install: kisslight
	cp resources/kisslight.ini /etc/
	cp resources/kisslight.service /etc/systemd/system/
	mkdir /var/lib/kisslight
	sqlite3 /var/lib/kisslight/kisslight.db < resources/server-db.sql
	systemctl start kisslight.service
	systemctl enable kisslight.service

uninstall:
	systemctl stop kisslight.service
	systemctl disalbe kisslight.service
	rm -f /etc/kisslight.ini /etc/systemd/system/kisslight.service /usr/bin/kisslight
	rm -rf /var/lib/kisslight

clean:
	rm -f $(SRC)server/*.o client server
