
# speck - a very tiny window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = speck.c util.c
OBJ = ${SRC:.c=.o}

all: options speck

options:
	@echo speck build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

speck: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ${OBJ}

install: all
	mkdir -p $(HOME)/bin
	cp -f speck $(HOME)/bin
	chmod 755 $(HOME)/bin/speck

uninstall:
	rm -f $(HOME)/bin/speck

.PHONY: all options clean install uninstall
