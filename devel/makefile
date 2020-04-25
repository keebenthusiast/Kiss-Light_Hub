SRC = ..\src
CC = arm-linux-gnueabihf-g++.exe
CFLAGS = -g -Wall -fpermissive
LIBS = -lsqlite3 -pthread -lpigpio -lrt

_DEPS = common.h daemon.h ini.h \
INIReader.h log.h RCSwitch.h server.h \
discovery.h
DEPS = $(patsubst %,$(SRC)\\%,$(_DEPS))

_OBJ = common.o log.o server.o \
RCSwitch.o daemon.o ini.o \
INIReader.o discovery.o
OBJ = $(patsubst %,$(SRC)\\%,$(_OBJ))

all: kisslight

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

kisslight: $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) $^ -o ..\$@

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

.PHONY: clean

clean:
	del /f $(SRC)\*.o ..\kisslight
