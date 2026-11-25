# Cambios

## 1.0.3 (25-11-2026)

- Si alguien cambia el fichero mientras lo tienes abierto, al guardar
  isi te pregunta antes de pisarlo. Como hace nano.
- `isi +120 fichero` abre directamente en la línea 120. También vale
  `isi fichero:120`, que es como lo saca `grep -n`, así que lo copias y
  lo pegas.
- Se puede buscar y reemplazar con expresiones regulares. Lo pones entre
  barras, por ejemplo `/[0-9a-f]{64}/` para sacar hashes. Si entre las
  barras no hay ningún carácter de expresión se busca el texto tal cual,
  así que las rutas como `/tmp/` funcionan igual que siempre.
- Arreglado el filtro de columnas del CSV: si escribías una ñ o una
  letra con tilde y la borrabas, se quedaba medio carácter escondido y
  ya no encontraba nada.
- Arreglado el explorador de ficheros: si tecleabas rápido la inicial de
  una carpeta y luego la de un fichero de dentro, juntaba las dos letras
  y no lo encontraba.
- Desactivar IOCs en un texto grande va más rápido. Con un timeline de
  46 MB y 200.000 líneas pasa de 3,4 a 2,6 segundos.
- Importar un CSV grande como tabla gasta menos memoria. Con ese mismo
  timeline baja de 367 a 322 MB.
- Más pruebas. Un fichero cifrado con un solo byte cambiado o cortado no
  se abre, lo pruebo de 13 formas distintas. Un binario se guarda sin
  tocar ni un byte. Y una ruta entre barras nunca se toma por expresión.
- Por dentro he partido el explorador de ficheros y el filtro de
  columnas en funciones más pequeñas. Hacen exactamente lo mismo, pero
  se leen mejor.

## 1.0.2 (25-11-2026)

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
