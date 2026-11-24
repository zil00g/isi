#!/bin/sh
set -e

arch=$1
estatico=${2:--static-pie}

if [ -z "$arch" ] || [ -z "$SOURCE_DATE_EPOCH" ]; then
	echo "uso: SOURCE_DATE_EPOCH=... $0 arquitectura [-static-pie|-static]" >&2
	exit 1
fi

ulimit -c 0
apk add --no-cache \
	gcc=13.2.1_git20240309-r1 musl=1.2.5-r4 musl-dev=1.2.5-r4 binutils=2.42-r1 \
	make=4.4.1-r2 dpkg=1.22.6-r1 tar=1.35-r2 gzip=1.13-r0 xz=5.8.3-r0 \
	xz-libs=5.8.3-r0 coreutils=9.5-r2 > /dev/null

make check
make estatico ESTATICO="$estatico"
./isi -v

v=$(sed -n 's/^VERSION = //p' Makefile)
sh paquete/hacer-deb.sh isi "$v" "$arch"

d=isi-$v-linux-$arch
cp isi "dist/$d"
touch -d "@$SOURCE_DATE_EPOCH" "dist/$d"
tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
	-C dist -cf - "$d" | gzip -9n > "dist/$d.tar.gz"
rm "dist/$d"
sha256sum dist/*
