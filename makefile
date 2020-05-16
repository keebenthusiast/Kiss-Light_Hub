SRC = src
CC = g++
CFLAGS = -g -Wall -fpermissive
LIBS = -lsqlite3 -pthread -lrt

_DEPS = common.h daemon.h ini.h \
INIReader.h log.h server.h
DEPS = $(patsubst %,$(SRC)/%,$(_DEPS))

_OBJ = common.o log.o server.o \
daemon.o ini.o INIReader.o
OBJ = $(patsubst %,$(SRC)/%,$(_OBJ))

all: kisslight

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

kisslight: $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

install: kisslight
	cp resources/kisslight.ini /etc/
	mkdir /etc/kisslight
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

client: client/kl-client.go
	go build client/kl-client.go

client-install: client
	mkdir -p /home/$(USER)/.config/kisslight
	cp client/kl-client.ini /home/$(USER)/.config/kisslight/
	sudo cp kl-client /usr/bin/

client-uninstall:
	rm -r /home/$(USER)/.config/kisslight
	sudo rm /usr/bin/kl-client

clean:
	rm -f $(SRC)/*.o kl-client kisslight
