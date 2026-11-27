VERSION = 1.0.4
PREFIX ?= /usr/local
CC ?= cc
CFLAGS ?= -Os -Wall -Wextra
CPPFLAGS ?= -D_FORTIFY_SOURCE=2
DURO = -fstack-protector-strong -fPIE
ENLACE = -pie -Wl,-z,relro,-z,now
MONOCYPHER ?= incluido

ifeq ($(MONOCYPHER),sistema)
MONO_C = $(shell pkg-config --cflags monocypher)
MONO_L = $(shell pkg-config --libs monocypher)
MONO =
else
MONO_C = -Imonocypher
MONO_L =
MONO = monocypher/monocypher.c monocypher/monocypher.h
endif

all: isi

isi: isi.c idiomas.h $(MONO)
	$(CC) -std=c99 $(DURO) $(MONO_C) $(CPPFLAGS) $(CFLAGS) -o $@ isi.c $(filter %.c,$(MONO)) $(ENLACE) $(LDFLAGS) $(MONO_L)

ESTATICO ?= -static-pie

estatico: clean
	$(MAKE) isi ENLACE="$(ESTATICO) -Wl,-z,relro,-z,now -s"

deb: estatico
	sh paquete/hacer-deb.sh isi $(VERSION)

pruebas/pruebas: pruebas/pruebas.c isi.c idiomas.h $(MONO)
	$(CC) -std=c99 $(DURO) $(MONO_C) $(CPPFLAGS) $(CFLAGS) -o $@ pruebas/pruebas.c $(filter %.c,$(MONO)) $(ENLACE) $(LDFLAGS) $(MONO_L)

check: pruebas/pruebas
	./pruebas/pruebas pruebas

SANITIZA = -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

sanitiza:
	rm -f pruebas/pruebas
	$(MAKE) check CFLAGS="$(SANITIZA) -Wall -Wextra" LDFLAGS="$(SANITIZA)"
	rm -f pruebas/pruebas

ENTRADAS ?= 100000

fuzz: pruebas/fuzz.c isi.c idiomas.h $(MONO)
	$(CC) -std=c99 $(MONO_C) $(CPPFLAGS) $(SANITIZA) -o pruebas/fuzz pruebas/fuzz.c $(filter %.c,$(MONO)) $(MONO_L)
	./pruebas/fuzz $(ENTRADAS) 1
	./pruebas/fuzz $(ENTRADAS) 2

install: isi
	install -Dm755 isi $(DESTDIR)$(PREFIX)/bin/isi
	install -Dm644 isi.1 $(DESTDIR)$(PREFIX)/share/man/man1/isi.1
	install -Dm644 man/es/isi.1 $(DESTDIR)$(PREFIX)/share/man/es/man1/isi.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/isi $(DESTDIR)$(PREFIX)/share/man/man1/isi.1 \
		$(DESTDIR)$(PREFIX)/share/man/es/man1/isi.1

clean:
	rm -rf isi pruebas/pruebas pruebas/fuzz dist

.PHONY: all estatico deb check sanitiza fuzz install uninstall clean
