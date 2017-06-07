CFLAGS += -std=c99 -D_GNU_SOURCE -O2 -s -pthread -Wall -Wpedantic -Wextra ${UI_FLAGS} -export-dynamic
LDFLAGS += $(shell pkg-config --libs gtk+-2.0)
PREFIX = /usr/local
UI_FLAGS := $(shell pkg-config --cflags gtk+-2.0)

NAME = bfm

SRC = src/main.c

all: clean options ${NAME}

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${NAME}:
	@$(CC) $(LDFLAGS) ${SRC} -o ${NAME} $(CFLAGS)

config:
	@echo creating default config.h from config.def.h
	@cp config.def.h src/config.h

clean:
	@echo cleaning directory
	@rm -f ${NAME} ${OBJ}

install: all
	@echo installing ${NAME} to ${PREFIX}/bin
	@mkdir -p ${PREFIX}/bin
	@cp -f ${NAME} ${PREFIX}/bin
	@chmod 755 ${PREFIX}/bin/${NAME}

uninstall:
	@echo removing ${NAME} from ${PREFIX}/bin
	@rm -f ${PREFIX}/${NAME}

options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.PHONY: clean
