#define main isi_main
#include "../isi.c"
#undef main

#define CLAVE "Prueba-de-isi-2026!"

static int hechas, falladas;
static const char *dir = "pruebas";

static void ok(int c, const char *q)
{
	hechas++;
	if (!c) {
		falladas++;
		printf("FALLO: %s\n", q);
	}
}

static void igual_s(const char *a, const char *b, const char *q)
{
	hechas++;
	if (strcmp(a, b)) {
		falladas++;
		printf("FALLO: %s\n  sale:   %s\n  espera: %s\n", q, a, b);
	}
}

static void conv(const char *s, int activar, const char *e)
{
	Lin o = { 0 };

	cambia(s, strlen(s), &o, activar);
	add(&o, "", 1);
	igual_s(o.s, e, s);
	free(o.s);
}

static void ida_vuelta(const char *s)
{
	Lin a = { 0 }, b = { 0 };

	cambia(s, strlen(s), &a, 0);
	cambia(a.s, a.n, &b, 1);
	add(&b, "", 1);
	igual_s(b.s, s, "desactivar y reactivar");
	free(a.s);
	free(b.s);
}

static void pruebas_ioc(void)
{
	int f;

	conv("visita http://evil.com/a/b.php?x=1 ya", 0, "visita hxxp://evil[.]com/a/b.php?x=1 ya");
	conv("https://www.ejemplo.es/descarga.exe", 0, "hxxps://www[.]ejemplo[.]es/descarga.exe");
	conv("Visit HTTP://EVIL.COM/", 0, "Visit HXXP://EVIL[.]COM/");
	conv("ftp://10.0.0.5/x", 0, "fxp://10[.]0[.]0[.]5/x");
	conv("conecta a 185.220.101.4:443 ahora", 0, "conecta a 185[.]220[.]101[.]4:443 ahora");
	conv("ejecuta payload.exe y svchost.dll", 0, "ejecuta payload[.]exe y svchost[.]dll");
	conv("dominio malo.example.org.", 0, "dominio malo[.]example[.]org.");
	conv("correo user@evil.com", 0, "correo user@evil[.]com");
	conv("versión 1.2.3 y fichero.txt", 0, "versión 1.2.3 y fichero.txt");
	conv("hxxps://evil[.]com/a y 1[.]2[.]3[.]4 y malo[.]example[.]org", 1,
		"https://evil.com/a y 1.2.3.4 y malo.example.org");
	conv("hXXp://evil[.]com/", 1, "hTTp://evil.com/");
	conv("fxp://10[.]0[.]0[.]5/x", 1, "ftp://10.0.0.5/x");
	conv("hxxp://evil[.]com/", 0, "hxxp://evil[.]com/");
	ida_vuelta("GET http://c2.example.net/gate.php de 203.0.113.9 y dropper.exe");
	ok(ioc("44d88612fea8a8f36de82e1278abb02f", 0, 32, &f) == 5 && f == 32, "hash MD5");
	ok(ioc("3395856ce81f2b7382dee72602f798b642f14140", 0, 40, &f) == 5, "hash SHA-1");
	ok(ioc("275a021bbfb6489e54d471899f7db9d1663fc695ec2fe2a2c4538aabf651fd0f", 0, 64, &f) == 5,
		"hash SHA-256");
	ok(ioc("deadbeefdeadbeefdeadbeefdeadbeef", 0, 32, &f) != 5, "hex sin dígitos no es hash");
}

static char *fichero(const char *nombre, char *p, size_t n)
{
	snprintf(p, n, "%s/%s", dir, nombre);
	return p;
}

static int abre_cifrado(const char *p, const char *pw, char **b, size_t *n)
{
	int e;

	if (!(*b = leetodo(p, n, &e)))
		return 0;
	if (*n < CAB + 16 || memcmp(*b, "ISI1", 4) || !descifra(b, n, pw)) {
		free(*b);
		*b = 0;
		return 0;
	}
	return 1;
}

static int vector(void)
{
	static const uint8_t s[16] = { 0x51, 0x83, 0xef, 0x56, 0xf4, 0xa7, 0x83, 0x0f,
		0x4c, 0x55, 0xc6, 0xd4, 0x38, 0xe7, 0xb2, 0xd9 };
	static const uint8_t e[32] = { 0xd3, 0x2e, 0xab, 0x28, 0xea, 0xe2, 0xe6, 0x8b,
		0xa3, 0x7f, 0x3d, 0xb1, 0xdd, 0x20, 0xa2, 0x69, 0x57, 0x81, 0xf3, 0x36, 0xc1, 0x2c,
		0x41, 0xe1, 0xd6, 0x35, 0x9a, 0x95, 0x88, 0x49, 0xee, 0xa6 };
	uint8_t k[32];

	return deriva(CLAVE, s, k) && !memcmp(k, e, 32);
}

static int manipulado(const char *orig, size_t n, size_t corta, int pos)
{
	char q[256], p[300], *b;
	size_t m;
	int r, fd;

	snprintf(q, sizeof q, "%.200s/isi-prueba-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	if (!mkdtemp(q))
		return 0;
	snprintf(p, sizeof p, "%s/m.isi", q);
	b = malloc(n + 1);
	memcpy(b, orig, n);
	b[n] = 'x';
	if (pos >= 0)
		b[pos] ^= 1;
	fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	r = fd >= 0 && write(fd, b, corta) == (ssize_t)corta;
	if (fd >= 0)
		close(fd);
	free(b);
	if (r && abre_cifrado(p, CLAVE, &b, &m)) {
		free(b);
		r = 0;
	}
	unlink(p);
	rmdir(q);
	return r;
}

static void pruebas_manipulado(const char *p)
{
	static const int pos[] = { 0, 3, 4, 19, 20, 43, 44, 59, 60, -2 };
	static const size_t cortes[] = { 0, 4, 43, 44, 59, 60 };
	char *b, q[96];
	size_t n, i;
	int e, bien = 1;

	if (!(b = leetodo(p, &n, &e))) {
		ok(0, "leer el fichero cifrado de prueba");
		return;
	}
	ok(!manipulado(b, n, n, -1), "sin tocarlo se abre");
	for (i = 0; i < sizeof pos / sizeof *pos; i++) {
		snprintf(q, sizeof q, "cambiar el byte %d no deja abrirlo", pos[i] < 0 ? (int)n - 1 : pos[i]);
		ok(manipulado(b, n, n, pos[i] < 0 ? (int)n - 1 : pos[i]), q);
	}
	for (i = 0; i < sizeof cortes / sizeof *cortes; i++)
		bien &= manipulado(b, n, cortes[i], -1);
	ok(bien, "cortado a 0, 4, 43, 44, 59 o 60 bytes no se abre");
	ok(manipulado(b, n, n - 1, -1), "sin el último byte no se abre");
	ok(manipulado(b, n, n + 1, -1), "con un byte de más no se abre");
	free(b);
}

static void pruebas_cifrado(void)
{
	static const char t[] = "línea uno\nñandú 東京\n\nfin";
	char p[PATH_MAX], q[256], *b;
	size_t n;
	int e;

	if (!abre_cifrado(fichero("cifrado-1.0.0.isi", p, sizeof p), CLAVE, &b, &n))
		ok(0, "abrir fichero cifrado por isi 1.0.0");
	else {
		ok(n > 0 && strstr(b, "ñandú, café, 東京") != 0, "contenido del fichero de isi 1.0.0");
		free(b);
	}
	ok(!abre_cifrado(p, "Otra-clave-mala-123", &b, &n), "contraseña incorrecta");
	ok(vector(), "Argon2id da la misma clave que libsodium");
	pruebas_manipulado(p);

	snprintf(q, sizeof q, "%.200s/isi-prueba-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	if (!mkdtemp(q)) {
		ok(0, "carpeta temporal");
		return;
	}
	snprintf(p, sizeof p, "%s/c.isi", q);
	b = strdup(t);
	cargatexto(b, strlen(b));
	free(b);
	cif = 1;
	ok(azar(sal, 16) && deriva(CLAVE, sal, clave), "derivar clave");
	ok(guarda(p), "guardar cifrado");
	cif = 0;
	crypto_wipe(clave, 32);
	if (!abre_cifrado(p, CLAVE, &b, &n))
		ok(0, "abrir lo que se acaba de cifrar");
	else {
		ok(n == strlen(t) && !memcmp(b, t, n), "ida y vuelta del cifrado");
		free(b);
	}
	if ((b = leetodo(p, &n, &e))) {
		ok(!memmem(b, n, "ñandú", strlen("ñandú")), "el texto no queda en claro");
		b[n - 1] ^= 1;
		ok(!descifra(&b, &n, CLAVE), "fichero alterado se rechaza");
		free(b);
	} else
		ok(0, "leer fichero cifrado");
	unlink(p);
	rmdir(q);
}

static void pruebas_csv(void)
{
	static const char c[] = "ip,\"nota, con coma\",\"di \"\"hola\"\"\"\r\n10.0.0.1,a|b,\"dos\nlíneas\"\r\n";
	Tabla t = { 0 };
	char *b;
	size_t n;

	ok(separador("a;b;c\n1;2;3", 11) == ';', "separador ;");
	ok(separador("a\tb\n", 4) == '\t', "separador tabulador");
	ok(separador("a|b|c\n", 6) == '|', "separador |");
	ok(separador("\"x;y\",b\n", 8) == ',', "separador con comillas");

	b = strdup(c);
	n = strlen(c);
	t.e = 1;
	leecsv(b, n, &t);
	free(b);
	ok(t.f == 2 && t.k == 3, "filas y columnas del CSV");
	if (t.f == 2 && t.k == 3) {
		igual_s(CE(&t, 0, 1), "nota, con coma", "celda con coma");
		igual_s(CE(&t, 0, 2), "di \"hola\"", "comillas dobles");
		igual_s(CE(&t, 1, 1), "a/b", "barra en tabla Markdown");
		igual_s(CE(&t, 1, 2), "dos líneas", "salto de línea dentro de celda");
	}
	suelta_t(&t);

	b = strdup("\xef\xbb\xbf" "caf\xe9 ma\xf1" "ana");
	n = strlen(b);
	b = autf8(b, &n);
	ok(n == strlen("café mañana") && !memcmp(b, "café mañana", n), "Latin-1 a UTF-8 y BOM");
	free(b);
}

static void tipo(const char *s, int t, double v)
{
	double x = 0;
	char q[128];

	snprintf(q, sizeof q, "tipo de «%s»", s);
	ok(tipode(s, &x) == t && (t > 2 || (x - v < 1e-6 && v - x < 1e-6)), q);
}

static void filtro(const char *e, const char *s, int r)
{
	Filtro f = { 0 };
	char q[192];

	compila(&f, e);
	snprintf(q, sizeof q, "filtro «%s» con «%s»", e, s);
	ok(cumple(&f, s) == r, q);
	suelta_f(&f);
}

static void pruebas_hoja(void)
{
	static const char *v[] = { "b", "10.0.0.10", "", "2", "Ábaco", "10.0.0.2", "ñu", "nz", "-3,5" };
	static const char *e[] = { "-3,5", "2", "10.0.0.2", "10.0.0.10", "Ábaco", "b", "nz", "ñu", "" };
	Clave k[9];
	int i, bien = 1;

	tipo("10.0.0.1", 2, 167772161.0);
	tipo("2026-11-24", 1, 20261124000000.0);
	tipo("24/11/2026", 1, 20261124000000.0);
	tipo("2026-11-24T10:20:30Z", 1, 20261124102030.0);
	tipo("3,5", 1, 3.5);
	tipo("1.234,5", 1, 1234.5);
	tipo("-7", 1, -7);
	tipo("texto", 3, 0);
	tipo("", 4, 0);
	tipo("1.234.567", 1, 1234567);
	tipo("1.234.567,89", 1, 1234567.89);
	tipo("1,234,567.89", 1, 1234567.89);
	tipo("999.999.999.999", 1, 999999999999.0);
	tipo("1.234", 1, 1234);
	tipo("1234.567", 1, 1234.567);
	tipo("1.204,75 €", 1, 1204.75);
	tipo("0.5", 1, 0.5);
	tipo("256.1.1.1", 3, 0);
	tipo("300.1.1.1", 3, 0);
	tipo("1.2.3", 3, 0);
	tipo("10.0.0", 3, 0);
	tipo("12.34.56", 3, 0);
	tipo("1,2,3", 3, 0);
	tipo("1.23.456", 3, 0);
	tipo("1.234.56", 3, 0);
	tipo("1..2", 3, 0);
	tipo("1,5.3", 3, 0);
	tipo(".5", 3, 0);
	tipo("5.", 3, 0);
	idi = 1;
	tipo("1.5", 1, 1.5);
	tipo("1,234", 1, 1234);
	tipo("1,234,567.89", 1, 1234567.89);
	tipo("1.2.3", 3, 0);
	idi = 0;

	for (i = 0; i < 9; i++) {
		k[i].r = i;
		k[i].t = tipode(v[i], &k[i].v);
		k[i].k = clavetexto(v[i]);
	}
	orden = 1;
	qsort(k, 9, sizeof *k, compara);
	for (i = 0; i < 9; i++) {
		bien &= !strcmp(v[k[i].r], e[i]);
		free(k[i].k);
	}
	ok(bien, "ordenar números, IP, texto con tildes y ñ, vacías al final");

	filtro("192.168.1.0/24", "192.168.1.77", 1);
	filtro("192.168.1.0/24", "192.168.2.1", 0);
	filtro("!192.168.1.0/24", "8.8.8.8", 1);
	filtro("*.exe", "PAYLOAD.EXE", 1);
	filtro("*.exe", "payload.dll", 0);
	filtro("evil", "www.EVIL.com", 1);
	filtro(">= 10", "10", 1);
	filtro("> 10", "9,5", 0);
	filtro("2026-11-01..2026-11-30", "2026-11-30 23:59", 1);
	filtro("2026-11-01..2026-11-30", "2026-12-01", 0);
	filtro("dll | exe", "a.exe", 1);
	filtro("!tmp", "C:\\TMP\\x", 0);
	filtro("canción", "CANCION", 1);
}

static void formatos(const char *s, char *o, size_t n)
{
	size_t j = 0;

	for (; *s && j + 2 < n; s++)
		if (*s == '%') {
			s++;
			while (*s && strchr("-+ #0123456789.*lhzjt", *s))
				o[j++] = *s++;
			if (!*s)
				break;
			o[j++] = *s;
			o[j++] = ' ';
		}
	o[j] = 0;
}

static void pruebas_idiomas(void)
{
	int n = sizeof idiomas / sizeof *idiomas, i, k, bien = 1, guarda = idi;
	char a[64], b[64];

	for (i = 0; i < n; i++) {
		idi = i;
		for (k = 1; k < T_FIN; k++) {
			formatos(l_es[k], a, sizeof a);
			formatos(tx(k), b, sizeof b);
			if (strcmp(a, b)) {
				bien = 0;
				printf("  %s %d: «%s» frente a «%s»\n", idiomas[i].cod, k, b, a);
			}
		}
	}
	idi = guarda;
	ok(bien, "las traducciones usan los mismos %s y %d que el español");
}

static void pruebas_binario(void)
{
	char q[256], p[300], b[1100], *l;
	size_t n, i;
	int e;

	for (i = 0; i < 1024; i++)
		b[i] = i;
	memcpy(b + 1024, "\0\0ELF\x7f\n\xff\xfe\r\n", 11);
	snprintf(q, sizeof q, "%.200s/isi-prueba-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	if (!mkdtemp(q)) {
		ok(0, "carpeta temporal");
		return;
	}
	snprintf(p, sizeof p, "%s/bin", q);
	l = malloc(1035);
	memcpy(l, b, 1035);
	cargatexto(l, 1035);
	free(l);
	cif = 0;
	ok(guarda(p), "guardar un binario");
	l = leetodo(p, &n, &e);
	ok(l && n == 1035 && !memcmp(l, b, n), "un binario se guarda sin perder ni cambiar un byte");
	free(l);
	unlink(p);
	rmdir(q);
}

static int esexpresion(const char *q)
{
	snprintf(qb, sizeof qb, "%s", q);
	return expresion();
}

static void pruebas_busqueda(void)
{
	ok(!esexpresion("/tmp/") && !esexpresion("/var/log/") && !esexpresion("/home/ana/.config/"),
		"una ruta entre barras se busca tal cual");
	ok(!esexpresion("//") && !esexpresion("tmp") && !esexpresion("/a.b/"), "sin barras o sin caracteres especiales no es expresión");
	ok(esexpresion("/[0-9a-f]{64}/") == 1 && esexpresion("/^2026/") == 1 && esexpresion("/a|b/") == 1,
		"con caracteres especiales entre barras es expresión");
	ok(esexpresion("/([0-9]/") < 0, "una expresión mal escrita no se usa");
	qb[0] = 0;
}

int main(int argc, char **argv)
{
	if (argc > 1)
		dir = argv[1];
	pruebas_ioc();
	pruebas_cifrado();
	pruebas_csv();
	pruebas_hoja();
	pruebas_idiomas();
	pruebas_binario();
	pruebas_busqueda();
	printf("%d pruebas, %d fallos\n", hechas, falladas);
	return falladas != 0;
}
