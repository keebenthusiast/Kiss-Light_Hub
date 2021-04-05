SRC = src
OBJ = obj
BIN = bin/kisslight
CLIENT_BIN = bin/kl-client
CC = clang
CFLAGS = -Wall -DSQLITE_ENABLE_MEMSYS5 \
#-DUSING_TOOLCHAIN #-DLOG_USE_COLOR -DDEBUG -g
LIBS = -pthread -lrt -ldl

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) \
$(filter $(subst *,%,$2),$d))
DEPS = $(call rwildcard,src,*.h)
SRCS = $(call rwildcard,src,*.c)
OBJS = $(SRCS:.c=.o)

all: $(BIN)

release: CFLAGS = -Wall -DSQLITE_ENABLE_MEMSYS5 -O2
release: $(BIN)

debug: CFLAGS= -Wall -DSQLITE_ENABLE_MEMSYS5 -DLOG_USE_COLOR -DDEBUG -g
debug: $(BIN)

target: CFLAGS = -Wall -DSQLITE_ENABLE_MEMSYS5 -DUSING_TOOLCHAIN
target: $(BIN)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

install: $(BIN)
	cp resources/kisslight.ini /etc/
	cp resources/kisslight.service /etc/systemd/system/
	cp kisslight /usr/bin/
	mkdir /var/lib/kisslight
	mkdir /var/log/kisslight
	#sqlite3 /var/lib/kisslight/kisslight.db < resources/server-db.sql
	cp resources/kisslight.db /var/lib/kisslight/kisslight.db
	systemctl daemon-reload
	systemctl start kisslight.service
	systemctl enable kisslight.service

uninstall:
	systemctl stop kisslight.service
	systemctl disable kisslight.service
	rm -f /etc/kisslight.ini /etc/systemd/system/kisslight.service
	rm -f /usr/bin/kisslight
	rm -rf /var/lib/kisslight
	rm -rf /var/log/kisslight
	systemctl daemon-reload

kl-client: client/kl-client.go
	go build -o $(CLIENT_BIN) client/kl-client.go

client-install: client
	mkdir -p /home/$(USER)/.config/kisslight
	cp client/kl-client.ini /home/$(USER)/.config/kisslight/
	sudo cp kl-client /usr/bin/

client-uninstall:
	rm -r /home/$(USER)/.config/kisslight
	sudo rm /usr/bin/kl-client

clean:
	rm -f $(OBJS) $(CLIENT_BIN) $(BIN)
