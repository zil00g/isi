#define main isi_main
#include "../isi.c"
#undef main

static unsigned long long semz = 88172645463325252ULL;
static char *sem[64];
static size_t semn[64];
static int nsem;

static const char *trozos[] = { "\"", "\"\"", ",", ";", "\t", "|", "\n", "\r\n", "\r", "http://", "hxxp://",
	"[.]", ".", "ñ", "\xe2\x94\x82", "\xe2\x94\x8c", "\xe2\x94\x9c", "\xe2\x94\x94", "---", "|---|", "10.0.0.1",
	"1.234,5", "-", "*", "!", "..", ">=", "/24", "\xff", "\xc3", "\xe2\x94", "%s", "%n", "1e309",
	"999999999999999999999", "0x1p4", "ISI1", " ", ":443", "C:\\x", "evil.exe", "a@b.c", "::1", "[", "]" };

static unsigned azr(void)
{
	semz ^= semz << 13;
	semz ^= semz >> 7;
	semz ^= semz << 17;
	return (unsigned)semz;
}

static size_t muta(char *b, size_t n, size_t cap)
{
	int k = 1 + azr() % 8;
	size_t p, l;
	const char *d;

	while (k--) {
		p = n ? azr() % (n + 1) : 0;
		switch (azr() % 5) {
		case 0:
			if (n)
				b[azr() % n] = azr();
			break;
		case 1:
			l = azr() % (n - p + 1);
			memmove(b + p, b + p + l, n - p - l);
			n -= l;
			break;
		default:
			d = trozos[azr() % (sizeof trozos / sizeof *trozos)];
			l = strlen(d);
			if (n + l < cap) {
				memmove(b + p + l, b + p, n - p);
				memcpy(b + p, d, l);
				n += l;
			}
		}
	}
	return n;
}

static void siembra(const char *s, size_t n)
{
	if (nsem < 64) {
		sem[nsem] = malloc(n + 1);
		memcpy(sem[nsem], s, n);
		semn[nsem++] = n;
	}
}

static void tablas(char *b, size_t n)
{
	Tabla t;
	Lin o;
	char *c = malloc(n + 1);
	int y, ny, nx;

	memcpy(c, b, n + 1);
	cargatexto(c, n);
	free(c);
	for (y = 0; y < nl; y++) {
		if (leer(y, &t, 0, 0, 0)) {
			memset(&o, 0, sizeof o);
			hazt(&t, &o, 0, 0, 0, &ny, &nx);
			free(o.s);
		}
		suelta_t(&t);
		if (t.viva)
			y = t.y1;
	}
}

int main(int argc, char **argv)
{
	static char b[1 << 16];
	long it = argc > 1 ? atol(argv[1]) : 100000, i;
	size_t n, h;
	Tabla t;
	Lin o;
	Filtro fl;
	double v;
	char *c, q;
	FILE *f;
	int j;

	if (argc > 2)
		semz ^= strtoull(argv[2], 0, 10);
	for (j = 3; j < argc; j++)
		if ((f = fopen(argv[j], "rb"))) {
			siembra(b, fread(b, 1, 8192, f));
			fclose(f);
		}
	siembra("ip,\"nota, con coma\",\"di \"\"hola\"\"\"\r\n10.0.0.1,a|b,\"dos\nlíneas\"\r\n", 50);
	siembra("| a | b |\n|---|---|\n| 1 | 2 |\n", 30);
	siembra("\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x90\n\xe2\x94\x82 a \xe2\x94\x82\n\xe2\x94\x94\xe2\x94\x80\xe2\x94\x98\n", 27);
	siembra("visita http://evil.com/a ya 185.220.101.4:443 payload.exe", 56);
	for (i = 0; i < it; i++) {
		j = azr() % nsem;
		n = semn[j] < sizeof b / 2 ? semn[j] : sizeof b / 2;
		memcpy(b, sem[j], n);
		n = muta(b, n, sizeof b - 1);
		b[n] = 0;
		switch (i % 6) {
		case 0:
			memset(&t, 0, sizeof t);
			t.e = azr() % 3;
			c = malloc(n + 1);
			memcpy(c, b, n + 1);
			leecsv(c, n, &t);
			free(c);
			suelta_t(&t);
			break;
		case 1:
			memset(&o, 0, sizeof o);
			cambia(b, (int)n, &o, azr() & 1);
			free(o.s);
			break;
		case 2:
			tipode(b, &v);
			numero(b, &v);
			free(clavetexto(b));
			break;
		case 3:
			memset(&fl, 0, sizeof fl);
			h = n / 2;
			q = b[h];
			b[h] = 0;
			compila(&fl, b);
			b[h] = q;
			cumple(&fl, b + h);
			suelta_f(&fl);
			break;
		default:
			tablas(b, n);
		}
	}
	for (j = 0; j < nsem; j++)
		free(sem[j]);
	vacio();
	printf("%ld entradas sin errores\n", it);
	return 0;
}
