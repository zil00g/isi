#!/bin/sh
set -e

bin=$1
version=$2
arch=${3:-$(dpkg --print-architecture)}

if [ ! -x "$bin" ] || [ -z "$version" ]; then
	echo "uso: $0 binario versión [arquitectura]" >&2
	exit 1
fi

raiz=$(mktemp -d)
chmod 755 "$raiz"
trap 'rm -rf "$raiz"' EXIT

install -Dm755 "$bin" "$raiz/usr/bin/isi"
install -d "$raiz/usr/share/man/man1" "$raiz/usr/share/doc/isi" "$raiz/DEBIAN"
gzip -9n < isi.1 > "$raiz/usr/share/man/man1/isi.1.gz"
cp paquete/copyright "$raiz/usr/share/doc/isi/copyright"
chmod 644 "$raiz/usr/share/man/man1/isi.1.gz" "$raiz/usr/share/doc/isi/copyright"

(cd "$raiz" && find usr -type f | sort | xargs md5sum > DEBIAN/md5sums)
chmod 644 "$raiz/DEBIAN/md5sums"

sed '/^#DEBHELPER#$/d' debian/isi.postinst > "$raiz/DEBIAN/postinst"
sed '/^#DEBHELPER#$/d' debian/isi.prerm > "$raiz/DEBIAN/prerm"
chmod 755 "$raiz/DEBIAN/postinst" "$raiz/DEBIAN/prerm"

tam=$(du -sk "$raiz/usr" | cut -f1)
sed -e "s/@VERSION@/$version/" -e "s/@ARCH@/$arch/" -e "s/@TAM@/$tam/" \
	paquete/control > "$raiz/DEBIAN/control"

find "$raiz" -type d -exec chmod 755 {} +
if [ -n "$SOURCE_DATE_EPOCH" ]; then
	find "$raiz" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
fi

mkdir -p dist
dpkg-deb --root-owner-group -Zxz --build "$raiz" "dist/isi_${version}_${arch}.deb"
