CC=clang++
CFLAGS=-g -Wall
LIBLINK=-lwiringPi -lsqlite3
OBF=-c
SRC=src/

all: kisslight

client: client/kl-client.go
	go build client/kl-client.go

kisslight: $(SRC)common.o $(SRC)log.o $(SRC)server.o \
$(SRC)RCSwitch.o $(SRC)daemon.o $(SRC)ini.o \
$(SRC)INIReader.o
	$(CC) $(CFLAGS) $(LIBLINK) $(SRC)common.o \
	$(SRC)log.o $(SRC)RCSwitch.o \
	$(SRC)server.o $(SRC)daemon.o \
	$(SRC)INIReader.o $(SRC)ini.o -o kisslight

common.o: $(SRC)common.cpp $(SRC)common.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)common.cpp

log.o: $(SRC)log.cpp $(SRC)log.h $(SRC)common.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)log.cpp

server.o: $(SRC)server.cpp $(SRC)server.h $(SRC)log.h $(SRC)common.h
	$(CC) $(OBF) $(CFLAG) $(SRC)server.cpp

RCSwitch.o: $(SRC)RCSwitch.cpp $(SRC)RCSwitch.h $(SRC)log.h
	$(CC) $(OBF) $(CFLAG) $(SRC)RCSwitch.cpp

daemon.o: $(SRC)daemon.cpp $(SRC)daemon.h $(SRC)common.h $(SRC)log.h
	$(CC) $(OBF) $(CFLAG) $(SRC)daemon.cpp

INIReader.o: $(SRC)INIReader.cpp $(SRC)INIReader.h $(SRC)ini.o
	$(CC) $(OBF) $(CFLAG) $(SRC)INIReader.cpp

ini.o: $(SRC)ini.c $(SRC)ini.h
	gcc $(OBF) $(CFLAG) $(SRC)ini.c

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
	rm -f $(SRC)*.o kl-client kisslight
