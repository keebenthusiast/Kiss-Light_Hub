CC=arm-linux-gnueabihf-g++.exe
CFLAGS=-g -Wall -fpermissive
LIBLINK=-lwiringPi -lsqlite3
OBF=-c
SRC=..\src

all: kisslight

client: client/kl-client.go
	go build client/kl-client.go

client-install: client
	mkdir -p /home/$(USER)/.config/kisslight
	cp client/kl-client.ini /home/$(USER)/.config/kisslight/
	sudo cp kl-client /usr/bin/

client-uninstall:
	rm -r /home/$(USER)/.config/kisslight
	sudo rm /usr/bin/kl-client

kisslight: common.o log.o server.o \
RCSwitch.o daemon.o ini.o \
INIReader.o
	$(CC) $(CFLAGS) $(LIBLINK) $(SRC)\common.o \
	$(SRC)\log.o $(SRC)\RCSwitch.o \
	$(SRC)\server.o $(SRC)\daemon.o \
	$(SRC)\INIReader.o $(SRC)\ini.o -o ..\kisslight

common.o: $(SRC)\common.cpp $(SRC)\common.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)\common.cpp -o $(SRC)\common.o

log.o: $(SRC)\log.cpp $(SRC)\log.h $(SRC)\common.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)\log.cpp -o $(SRC)\log.o

server.o: $(SRC)\server.cpp $(SRC)\server.h $(SRC)\log.h $(SRC)\common.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)\server.cpp -o $(SRC)\server.o

RCSwitch.o: $(SRC)\RCSwitch.cpp $(SRC)\RCSwitch.h $(SRC)\log.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)\RCSwitch.cpp -o $(SRC)\RCSwitch.o

daemon.o: $(SRC)\daemon.cpp $(SRC)\daemon.h $(SRC)\common.h $(SRC)\log.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)\daemon.cpp -o $(SRC)\daemon.o

INIReader.o: $(SRC)\INIReader.cpp $(SRC)\INIReader.h $(SRC)\ini.o
	$(CC) $(OBF) $(CFLAGS) $(SRC)\INIReader.cpp -o $(SRC)\INIReader.o

ini.o: $(SRC)\ini.c $(SRC)\ini.h
	$(CC) $(OBF) $(CFLAGS) $(SRC)\ini.c -o $(SRC)\ini.o

install: kisslight
	cp resources/kisslight.ini /etc/
	cp resources/kisslight.service /etc/systemd/system/
	cp kisslight /usr/bin/
	mkdir /var/lib/kisslight
	sqlite3 /var/lib/kisslight/kisslight.db < resources/server-db.sql
	systemctl daemon-reload
	systemctl start kisslight.service
	systemctl enable kisslight.service

uninstall:
	systemctl stop kisslight.service
	systemctl disable kisslight.service
	rm -f /etc/kisslight.ini /etc/systemd/system/kisslight.service /usr/bin/kisslight
	rm -rf /var/lib/kisslight
	systemctl daemon-reload

clean:
	del /f $(SRC)\*.o ..\kl-client ..\kisslight
