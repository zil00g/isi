# El formato de los ficheros cifrados

[English](FORMAT.md)

Cuando cifras un fichero con isi, se guarda así. No hay nada más: ni
metadatos, ni nombre del fichero, ni versión del programa.

| Posición | Tamaño | Qué es |
|---|---|---|
| 0 | 4 | `ISI1` |
| 4 | 16 | sal de Argon2id |
| 20 | 24 | nonce de XChaCha20 |
| 44 | 16 | etiqueta Poly1305 |
| 60 | el resto | el texto cifrado |

La clave sale de la contraseña con **Argon2id**: 64 MiB de memoria,
3 pasadas, 1 hilo y 32 bytes de salida. La contraseña se usa tal cual
la escribes, en UTF-8, sin normalizar.

El texto se cifra con **XChaCha20-Poly1305**. Los primeros 44 bytes
(`ISI1`, la sal y el nonce) van como datos asociados, así que si alguien
toca la cabecera el fichero ya no se abre.

Las primitivas son las de [Monocypher](https://monocypher.org) 4.0.3,
que va dentro del código sin cambios. No he escrito nada de criptografía
yo: isi solo junta las piezas.

## Cuándo cambia cada cosa

- La sal es nueva cada vez que pones o cambias la contraseña, y también
  cuando guardas el fichero con otro nombre.
- El nonce es nuevo cada vez que guardas, sacado de `/dev/urandom`.
  Si no se puede leer, isi no guarda.
- Los ficheros cifrados nuevos se crean con permisos 0600.

## Comprobarlo sin isi

No hace falta fiarse de mí. Con Python y libsodium
(`sudo apt install python3-nacl`) puedes abrir un fichero de isi:

```python
import sys
from nacl import bindings as b

d = open(sys.argv[1], "rb").read()
assert d[:4] == b"ISI1"
sal, nonce, tag, cif = d[4:20], d[20:44], d[44:60], d[60:]
k = b.crypto_pwhash_alg(32, sys.argv[2].encode(), sal, 3, 64 << 20,
                        b.crypto_pwhash_ALG_ARGON2ID13)
print(b.crypto_aead_xchacha20poly1305_ietf_decrypt(cif + tag, d[:44], nonce, k).decode())
```

```
python3 abrir.py pruebas/cifrado-1.0.0.isi 'Prueba-de-isi-2026!'
```

Ese fichero de prueba lo cifró isi 1.0.0 y `make check` comprueba en
cada versión que se sigue abriendo y que Argon2id da la misma clave que
libsodium (`d32eab28…49eea6`).
