# Encrypted file format

[Español](FORMATO.md)

When you encrypt a file with isi, this is what gets written. Nothing
else: no metadata, no file name, no program version.

| Offset | Size | What |
|---|---|---|
| 0 | 4 | `ISI1` |
| 4 | 16 | Argon2id salt |
| 20 | 24 | XChaCha20 nonce |
| 44 | 16 | Poly1305 tag |
| 60 | rest | ciphertext |

The key comes from the password with **Argon2id**: 64 MiB of memory,
3 passes, 1 lane, 32 bytes of output. The password is used exactly as
typed, in UTF-8, with no normalization.

The text is encrypted with **XChaCha20-Poly1305**. The first 44 bytes
(`ISI1`, salt and nonce) are the associated data, so if anyone changes
the header the file won't open.

The primitives come from [Monocypher](https://monocypher.org) 4.0.3,
shipped unmodified in the source. I didn't write any crypto myself, isi
only puts the pieces together.

## When each part changes

- The salt is new every time you set or change the password, and also
  when you save the file under another name.
- The nonce is new on every save, read from `/dev/urandom`. If it can't
  be read, isi doesn't save.
- New encrypted files are created with mode 0600.

## Checking it without isi

You don't have to trust me. With Python and libsodium
(`sudo apt install python3-nacl`) you can open an isi file:

```python
import sys
from nacl import bindings as b

d = open(sys.argv[1], "rb").read()
assert d[:4] == b"ISI1"
salt, nonce, tag, ct = d[4:20], d[20:44], d[44:60], d[60:]
k = b.crypto_pwhash_alg(32, sys.argv[2].encode(), salt, 3, 64 << 20,
                        b.crypto_pwhash_ALG_ARGON2ID13)
print(b.crypto_aead_xchacha20poly1305_ietf_decrypt(ct + tag, d[:44], nonce, k).decode())
```

```
python3 open.py pruebas/cifrado-1.0.0.isi 'Prueba-de-isi-2026!'
```

That test file was encrypted by isi 1.0.0, and `make check` makes sure
on every release that it still opens and that Argon2id gives the same
key as libsodium (`d32eab28…49eea6`).
