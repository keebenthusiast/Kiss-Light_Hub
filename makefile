SRC = src
CC = clang
CFLAGS = -Wall -DSQLITE_ENABLE_MEMSYS5 -DLOG_USE_COLOR -DDEBUG -g
LIBS = -pthread -lrt -ldl
CONSTS = -DSQLITE_ENABLE_MEMSYS5

_DEPS =  daemon.h ini.h args.h log.h config.h \
server.h main.h mqtt.h mqtt_pal.h database.h \
statejson.h jsmn.h sqlite3/sqlite.h
DEPS = $(patsubst %,$(SRC)/%,$(_DEPS))

_OBJ = log.o server.o daemon.o args.o config.o \
ini.o main.o mqtt.o mqtt_pal.o database.o \
statejson.o sqlite3/sqlite3.o
OBJ = $(patsubst %,$(SRC)/%,$(_OBJ))

all: kisslight

%.o: %.c $(DEPS)
	ifeq ($(DEBUG), true)
		$(CC) $(CONSTS) -c -o $@ $< $(CFLAGS)
	else
		$(CC) $(DEBUG_CONSTS) -c -o $@ $< $(CFLAGS)
	endif

kisslight: $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

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
	rm -f /etc/kisslight.ini /etc/systemd/system/kisslight.service
	rm -f /usr/bin/kisslight
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
	rm -f $(SRC)/*.o $(SRC)/sqlite3/*.o kl-client kisslight
