# Cambios

## 1.0.2 (sin publicar)

- CSV: «256.1.1.1», «1.2.3», «.5» o «5.» ya no cuentan como números al
  ordenar y filtrar. Solo lo que está bien escrito, con los grupos de
  miles en su sitio.
- Un filtro como `>` sin nada detrás dejaba un poco de memoria sin
  liberar. Arreglado.
- El paquete ahora recomienda wl-clipboard, xclip o xsel en vez de solo
  sugerirlos, para que Ctrl+C llegue al portapapeles del sistema sin
  hacer nada en terminales como GNOME Terminal.
- Pruebas nuevas: que las 23 traducciones usen los mismos `%s` y `%d`
  que el español, y que Argon2id dé la misma clave que libsodium.
- `make sanitiza` pasa las pruebas con AddressSanitizer y UBSan, y
  `make fuzz` mete entradas raras en el CSV, los IOCs, los filtros y
  las tablas. Los dos se ejecutan en GitHub en cada cambio, junto con
  clang, el analizador de gcc y cppcheck.
- `make MONOCYPHER=sistema` para compilar con la Monocypher del sistema.
- Documentado el formato cifrado (FORMATO.md) y qué protege y qué no
  (SECURITY.md).

## 1.0.1 (24-11-2026)

- Monocypher 4.0.2 → 4.0.3.
- `make check` con pruebas de IOCs, CSV, filtros y cifrado, incluido un
  fichero cifrado con la 1.0.0 que se tiene que seguir abriendo.
- Página de manual en inglés y en español.

## 1.0.0 (18-11-2026)

Primera versión.
