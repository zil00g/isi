VERSION = 1.0.0
PREFIX ?= /usr/local
CC ?= cc
CFLAGS ?= -Os -Wall -Wextra
CPPFLAGS ?= -D_FORTIFY_SOURCE=2
DURO = -fstack-protector-strong -fPIE
ENLACE = -pie -Wl,-z,relro,-z,now
FUENTES = isi.c monocypher/monocypher.c

all: isi

isi: $(FUENTES) idiomas.h monocypher/monocypher.h
	$(CC) -std=c99 $(DURO) $(CPPFLAGS) $(CFLAGS) -o $@ $(FUENTES) $(ENLACE) $(LDFLAGS)

ESTATICO ?= -static-pie

estatico: clean
	$(MAKE) isi ENLACE="$(ESTATICO) -Wl,-z,relro,-z,now -s"

deb: estatico
	sh paquete/hacer-deb.sh isi $(VERSION)

install: isi
	install -Dm755 isi $(DESTDIR)$(PREFIX)/bin/isi
	install -Dm644 isi.1 $(DESTDIR)$(PREFIX)/share/man/man1/isi.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/isi $(DESTDIR)$(PREFIX)/share/man/man1/isi.1

clean:
	rm -rf isi dist

.PHONY: all estatico deb install uninstall clean
