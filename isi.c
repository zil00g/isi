/*
 * isi - editor de texto para la terminal
 * Copyright (C) 2026 zil00g <zil00g@protonmail.com>
 *
 * Este programa es software libre: puedes redistribuirlo y/o modificarlo
 * bajo los términos de la Licencia Pública General GNU, versión 3 o
 * (a tu elección) cualquier versión posterior. Se distribuye SIN NINGUNA
 * GARANTÍA. Consulta el fichero LICENSE para más detalles.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "monocypher.h"
#include "idiomas.h"

#define VERSION "1.0.1"
#define TABU 4
#define MAXC 256
#define GRANDE 400
#define CAB 44
#define MEMA 65536
#define NM 7

enum { K_ESC = 0x110000, K_ARR, K_ABA, K_IZQ, K_DER, K_INI, K_FIN, K_REP, K_AVP,
	K_SUP, K_INS, K_F1, K_F3, K_F10, K_STAB, K_RATON, K_PEGAR, K_TAM, K_NADA };
#define MAY (1 << 24)
#define ALT (2 << 24)
#define CTR (4 << 24)

enum { NOR, BOR, MEN, INV, NUM, SEL, CTL, NUMA, TIT, CAR, CAM, IOC };
static const char *sgr[] = { "0", "0;36", "0;1", "0;7", "0;33", "0;7", "0;7;31",
	"0;1;33", "0;1;36", "0;1;34", "0;4", "0;1;35" };

enum { A_NADA, A_NUEVO, A_ABRIR, A_GUARDAR, A_COMO, A_CIFRAR, A_DESCIFRAR, A_SALIR,
	A_DESH, A_REH, A_CORTAR, A_COPIAR, A_PEGAR, A_TODO, A_DUP, A_BORRALIN, A_VACIAS,
	A_BUSCAR, A_SIG, A_ANT, A_REEMPL, A_IRA, A_NUM, A_TABLA, A_FILA, A_COL,
	A_QFILA, A_QCOL, A_ATAJOS, A_ACERCA, A_CSV, A_IDIOMA,
	A_RESALTA, A_AUTO, A_DESACT, A_REACT, A_LISTA, A_ABRIRCSV, A_ORDAZ, A_ORDZA, A_FILTRAR, A_SINFILTROS };

typedef struct { char *s; int n, c; } Lin;
typedef struct { char c[8]; unsigned char n, w, a; } Celda;
typedef struct { int t, y, x, g, n; char *s; } Paso;
typedef struct { int t; const char *k; int a; } Op;
typedef struct { char *n; int d; } Ent;
typedef struct { char **c; int f, fc, k, kc, e, y0, y1, viva; } Tabla;
#define CE(t, i, j) ((t)->c[(size_t)(i) * (t)->kc + (j)])

static Lin *L, ob, clip, pg;
static int nl, capl, cy, cx, px = -1, fy, fx, sel, ay, ax, arr;
static int filas, cols, xs[NM + 1], gw, x0, th, tw, barra, pide, abierto = -1;
static int numeros = 1, mod, solo, crlf, cif, grupo, prev, idi, resalta = 1, autoioc;
static int mmx, mmw, mmn, rb, rx, ry, rs;
static int sy1, sx1, sy2, sx2, hay;
static Celda *pan, *ant;
static Paso *pd, *pr;
static int nd, nr, cdp, crp;
static size_t peso;
static char ruta[PATH_MAX], aviso[512], qb[256], rp[256], titulo[PATH_MAX + 16];
static time_t avt;
static uint8_t clave[32], sal[16];
static struct termios orig;
static volatile sig_atomic_t cambio, colgado;
static unsigned char eb[8192];
static int ebn, ebi;
static int hoja, forzado;
static char hsep = ',', pista[512];

static const int cero[][2] = { {0x300, 0x36F}, {0x483, 0x489}, {0x591, 0x5BD},
	{0x5BF, 0x5BF}, {0x5C1, 0x5C2}, {0x5C4, 0x5C5}, {0x5C7, 0x5C7}, {0x610, 0x61A},
	{0x64B, 0x65F}, {0x670, 0x670}, {0x6D6, 0x6DC}, {0x6DF, 0x6E4}, {0x6E7, 0x6E8},
	{0x6EA, 0x6ED}, {0x900, 0x902}, {0x93A, 0x93A}, {0x93C, 0x93C}, {0x941, 0x948},
	{0x94D, 0x94D}, {0x951, 0x957}, {0x962, 0x963}, {0xE31, 0xE31}, {0xE34, 0xE3A},
	{0xE47, 0xE4E}, {0x1160, 0x11FF}, {0x20D0, 0x20FF}, {0xFE00, 0xFE0F},
	{0xFE20, 0xFE2F}, {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF}, {0xE0100, 0xE01EF} };
static const int doble[][2] = { {0x1100, 0x115F}, {0x2E80, 0x303E}, {0x3041, 0x33FF},
	{0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xA000, 0xA4CF}, {0xAC00, 0xD7A3},
	{0xF900, 0xFAFF}, {0xFE30, 0xFE4F}, {0xFF00, 0xFF60}, {0xFFE0, 0xFFE6},
	{0x1F300, 0x1F64F}, {0x1F900, 0x1F9FF}, {0x20000, 0x3FFFD} };

static const Op m0[] = { {T_NUEVO, "", A_NUEVO}, {T_ABRIR, "Ctrl+A", A_ABRIR},
	{T_ABRIRCSV, "", A_ABRIRCSV}, {T_CSV, "", A_CSV}, {T_GUARDAR, "Ctrl+G", A_GUARDAR}, {T_COMO, "Ctrl+T", A_COMO}, {0, 0, 0},
	{T_CIFRAR, "", A_CIFRAR}, {T_DESCIFRAR, "", A_DESCIFRAR}, {0, 0, 0},
	{T_SALIR, "Ctrl+Q", A_SALIR}, {0, 0, -1} };
static const Op m1[] = { {T_DESH, "Ctrl+Z", A_DESH}, {T_REH, "Ctrl+Y", A_REH}, {0, 0, 0},
	{T_CORTAR, "Ctrl+X", A_CORTAR}, {T_COPIAR, "Ctrl+C", A_COPIAR}, {T_PEGAR, "Ctrl+V", A_PEGAR},
	{T_TODO, "Ctrl+E", A_TODO}, {0, 0, 0}, {T_DUP, "Ctrl+D", A_DUP}, {T_BORRALIN, "Ctrl+B", A_BORRALIN},
	{T_VACIAS, "Ctrl+K", A_VACIAS}, {0, 0, -1} };
static const Op m2[] = { {T_BUSCARM, "Ctrl+F", A_BUSCAR}, {T_SIG, "F3", A_SIG}, {T_ANT, "S+F3", A_ANT},
	{T_REEMPL, "Ctrl+R", A_REEMPL}, {T_IRA, "Ctrl+L", A_IRA}, {0, 0, -1} };
static const Op m3[] = { {T_NUMEROS, "Ctrl+N", A_NUM}, {T_IDIOMA, "", A_IDIOMA}, {0, 0, -1} };
static const Op m4[] = { {T_INSERTAR, "", A_TABLA}, {0, 0, 0}, {T_FILA, "", A_FILA},
	{T_COL, "", A_COL}, {T_QFILA, "", A_QFILA}, {T_QCOL, "", A_QCOL}, {0, 0, 0},
	{T_ORDAZ, "", A_ORDAZ}, {T_ORDZA, "", A_ORDZA}, {T_FILTRAR, "Alt+↓", A_FILTRAR},
	{T_SINFILTROS, "", A_SINFILTROS}, {0, 0, -1} };
static const Op m5[] = { {T_RESALTAR, "", A_RESALTA}, {T_AUTO, "", A_AUTO}, {0, 0, 0},
	{T_DESACT, "Ctrl+W", A_DESACT}, {T_REACT, "", A_REACT}, {T_LISTA, "", A_LISTA}, {0, 0, -1} };
static const Op m6[] = { {T_ATAJOS, "Ctrl+H", A_ATAJOS}, {T_ACERCA, "", A_ACERCA}, {0, 0, -1} };
static const Op *menus[] = { m0, m1, m2, m3, m4, m5, m6 };
static const int atajo[32] = { [1] = A_ABRIR, [2] = A_BORRALIN, [3] = A_COPIAR,
	[4] = A_DUP, [5] = A_TODO, [6] = A_BUSCAR, [7] = A_GUARDAR, [8] = A_ATAJOS, [11] = A_VACIAS,
	[12] = A_IRA, [14] = A_NUM, [17] = A_SALIR, [18] = A_REEMPL, [20] = A_COMO,
	[22] = A_PEGAR, [23] = A_DESACT, [24] = A_CORTAR, [25] = A_REH, [26] = A_DESH };

static const char *tx(int i)
{
	const char *s = idiomas[idi].t[i];

	if (!s && idiomas[idi].b)
		s = idiomas[idi].b[i];
	return s ? s : l_es[i];
}

static const char *ops(int n, ...)
{
	static char b[256];
	va_list v;
	int i;

	b[0] = 0;
	va_start(v, n);
	for (i = 0; i < n; i++) {
		if (i)
			strcat(b, "|");
		strncat(b, tx(va_arg(v, int)), 60);
	}
	va_end(v);
	return b;
}

static void normal(void);
static void haz(int);
static void pintahoja(void);
static void ajustahoja(void);
static void iniciahoja(void);
static int escsv(const char *p);
static void ratonhoja(void);
static void pegahoja(const char *s, int n);

static void muere(const char *m)
{
	normal();
	fprintf(stderr, "isi: %s\n", m);
	exit(1);
}

static void *re(void *p, size_t n)
{
	if (!(p = realloc(p, n ? n : 1)))
		muere(tx(T_SINMEM));
	return p;
}

static void crece(Lin *l, int n)
{
	if (n > l->c)
		l->s = re(l->s, l->c = n + n / 2 + 16);
}

static void add(Lin *l, const char *s, int n)
{
	if (n <= 0)
		return;
	crece(l, l->n + n);
	memcpy(l->s + l->n, s, n);
	l->n += n;
}

static void adds(Lin *l, const char *s)
{
	add(l, s, strlen(s));
}

static void addf(Lin *l, const char *f, ...)
{
	char b[1024];
	va_list v;

	va_start(v, f);
	vsnprintf(b, sizeof b, f, v);
	va_end(v);
	adds(l, b);
}

static void avisa(const char *f, ...)
{
	va_list v;

	va_start(v, f);
	vsnprintf(aviso, sizeof aviso, f, v);
	va_end(v);
	avt = time(0);
}

static const char *fallo(int e)
{
	switch (e) {
	case ENOENT: return tx(T_E_NOENT);
	case EACCES: case EPERM: return tx(T_E_PERM);
	case EROFS: return tx(T_E_ROFS);
	case ENOSPC: case EDQUOT: return tx(T_E_NOSPC);
	case EISDIR: return tx(T_E_ISDIR);
	case ENOTDIR: return tx(T_E_NOTDIR);
	case EINVAL: return tx(T_E_INVAL);
	case ENAMETOOLONG: return tx(T_E_LARGO);
	case EIO: return tx(T_E_IO);
	case ENOMEM: return tx(T_E_MEM);
	case EBUSY: case ETXTBSY: return tx(T_E_BUSY);
	case EFBIG: return tx(T_E_GRANDE);
	}
	return strerror(e);
}

static const char *base(const char *p)
{
	const char *b = strrchr(p, '/');

	return b ? b + 1 : p;
}

static int dec(const char *t, int n, int *cp)
{
	const unsigned char *s = (const unsigned char *)t;
	int c = s[0], l, i;

	*cp = c;
	if (c < 0x80)
		return 1;
	l = c >= 0xC2 && c < 0xE0 ? 2 : c >= 0xE0 && c < 0xF0 ? 3 : c >= 0xF0 && c < 0xF5 ? 4 : 0;
	*cp = -1;
	if (!l || l > n)
		return 1;
	c &= 0x7F >> l;
	for (i = 1; i < l; i++) {
		if ((s[i] & 0xC0) != 0x80)
			return 1;
		c = c << 6 | (s[i] & 0x3F);
	}
	if ((l == 3 && (c < 0x800 || (c >= 0xD800 && c < 0xE000))) || (l == 4 && (c < 0x10000 || c > 0x10FFFF)))
		return 1;
	*cp = c;
	return l;
}

static int cod(int c, char *s)
{
	if (c < 0x80)
		return s[0] = c, 1;
	if (c < 0x800)
		return s[0] = 0xC0 | c >> 6, s[1] = 0x80 | (c & 63), 2;
	if (c < 0x10000)
		return s[0] = 0xE0 | c >> 12, s[1] = 0x80 | (c >> 6 & 63), s[2] = 0x80 | (c & 63), 3;
	s[0] = 0xF0 | c >> 18;
	s[1] = 0x80 | (c >> 12 & 63);
	s[2] = 0x80 | (c >> 6 & 63);
	s[3] = 0x80 | (c & 63);
	return 4;
}

static int entre(const int (*t)[2], int n, int c)
{
	while (n--)
		if (c >= t[n][0] && c <= t[n][1])
			return 1;
	return 0;
}

static int raro(int c)
{
	return c < 0 || (c >= 0x80 && c < 0xA0) || (c >= 0x200B && c <= 0x200F) ||
		(c >= 0x202A && c <= 0x202E) || (c >= 0x2060 && c <= 0x2069) || c == 0xFEFF;
}

static int mira(const char *s, int i, int n, int col, int *w, int *c)
{
	int l = dec(s + i, n - i, c);

	if (*c == '\t')
		*w = TABU - col % TABU;
	else if ((*c >= 0 && *c < 32) || *c == 127)
		*w = 2;
	else if (raro(*c) || *c < 0x300)
		*w = 1;
	else
		*w = entre(cero, sizeof cero / sizeof *cero, *c) ? 0 : entre(doble, sizeof doble / sizeof *doble, *c) ? 2 : 1;
	return i + l;
}

static int ancho(const char *s)
{
	int i = 0, n = strlen(s), w, c, col = 0;

	while (i < n) {
		i = mira(s, i, n, col, &w, &c);
		col += w;
	}
	return col;
}

static int colde(int y, int x)
{
	int i = 0, w, c, col = 0;

	while (i < x) {
		i = mira(L[y].s, i, L[y].n, col, &w, &c);
		col += w;
	}
	return col;
}

static int bytede(int y, int want)
{
	int i = 0, j, w, c, col = 0;

	while (i < L[y].n) {
		j = mira(L[y].s, i, L[y].n, col, &w, &c);
		if (col + w > want)
			break;
		col += w;
		i = j;
	}
	return i;
}

static void salida(const char *s, int n)
{
	int r;

	while (n > 0) {
		r = write(1, s, n);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			return;
		s += r;
		n -= r;
	}
}

static void normal(void)
{
	static const char s[] = "\033[0m\033[8h\033[?2004l\033[?1006l\033[?1002l\033[?25h\033[2J\033[?1049l\033[23;0t";

	salida(s, sizeof s - 1);
	tcsetattr(0, TCSAFLUSH, &orig);
}

static void corta(int s)
{
	(void)s;
	colgado = 1;
}

static void avisotam(int s)
{
	(void)s;
	cambio = 1;
}

static void crudo(void)
{
	static const char s[] = "\033[?1049h\033[22;0t\033[8l\033[?1002h\033[?1006h\033[?2004h\033[H\033[2J";
	struct termios t;

	if (tcgetattr(0, &orig))
		muere(tx(T_CONFTERM));
	t = orig;
	t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	t.c_oflag &= ~OPOST;
	t.c_cflag |= CS8;
	t.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(0, TCSAFLUSH, &t);
	salida(s, sizeof s - 1);
}

static void tamano(void)
{
	struct winsize w;

	if (ioctl(1, TIOCGWINSZ, &w) || !w.ws_row || !w.ws_col) {
		w.ws_row = 24;
		w.ws_col = 80;
	}
	filas = w.ws_row;
	cols = w.ws_col;
	pan = re(pan, filas * cols * sizeof *pan);
	ant = re(ant, filas * cols * sizeof *ant);
	memset(ant, 0xFF, filas * cols * sizeof *ant);
	salida("\033[2J", 4);
}

static void rescata(void);

static int lee(int ms)
{
	struct pollfd p = { 0, POLLIN, 0 };
	int r;

	if (ebi < ebn)
		return eb[ebi++];
	if (colgado)
		rescata();
	if (cambio)
		return -2;
	r = poll(&p, 1, ms);
	if (colgado)
		rescata();
	if (r < 0)
		return errno == EINTR && cambio ? -2 : -1;
	if (!r)
		return -1;
	ebn = read(0, eb, sizeof eb);
	ebi = 0;
	if (ebn < 0 && (errno == EINTR || errno == EAGAIN)) {
		ebn = 0;
		return -1;
	}
	if (ebn <= 0)
		rescata();
	return eb[ebi++];
}

static int pegado(void)
{
	int c;

	pg.n = 0;
	while ((c = lee(1000)) >= 0) {
		char b = c;
		add(&pg, &b, 1);
		if (pg.n >= 6 && !memcmp(pg.s + pg.n - 6, "\033[201~", 6)) {
			pg.n -= 6;
			break;
		}
	}
	return K_PEGAR;
}

static int tecla(int ms)
{
	int c, p[4] = { 0 }, np = 0, m = 0, l, i, d;
	char b[4];

	c = lee(ms);
	if (c == -2) {
		cambio = 0;
		tamano();
		return K_TAM;
	}
	if (c < 0)
		return K_NADA;
	if (c == 27) {
		c = lee(30);
		if (c < 0)
			return K_ESC;
		if (c == '[') {
			c = lee(30);
			if (c == '<') {
				m = 1;
				c = lee(30);
			}
			while ((c >= '0' && c <= '9') || c == ';') {
				if (c == ';')
					np += np < 3;
				else if (p[np] < 100000)
					p[np] = p[np] * 10 + c - '0';
				c = lee(30);
			}
			if (c < 0)
				return K_NADA;
			if (m) {
				rb = p[0];
				rx = p[1] - 1;
				ry = p[2] - 1;
				rs = c == 'm';
				return K_RATON;
			}
			d = np && p[1] > 1 ? (p[1] - 1) << 24 : 0;
			switch (c) {
			case 'A': return K_ARR | d;
			case 'B': return K_ABA | d;
			case 'C': return K_DER | d;
			case 'D': return K_IZQ | d;
			case 'H': return K_INI | d;
			case 'F': return K_FIN | d;
			case 'Z': return K_STAB;
			case 'P': return K_F1 | d;
			case 'R': return K_F3 | d;
			case '~':
				switch (p[0]) {
				case 1: case 7: return K_INI | d;
				case 4: case 8: return K_FIN | d;
				case 2: return K_INS | d;
				case 3: return K_SUP | d;
				case 5: return K_REP | d;
				case 6: return K_AVP | d;
				case 11: return K_F1 | d;
				case 13: return K_F3 | d;
				case 21: return K_F10 | d;
				case 200: return pegado();
				}
			}
			return K_NADA;
		}
		if (c == 'O') {
			switch (lee(30)) {
			case 'A': return K_ARR;
			case 'B': return K_ABA;
			case 'C': return K_DER;
			case 'D': return K_IZQ;
			case 'H': return K_INI;
			case 'F': return K_FIN;
			case 'P': return K_F1;
			case 'R': return K_F3;
			}
			return K_NADA;
		}
		return c < 128 ? c | ALT : K_NADA;
	}
	if (c >= 0x80 && c < 0xC0)
		return K_NADA;
	if (c >= 0xC0) {
		b[0] = c;
		l = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
		for (i = 1; i < l; i++) {
			if ((c = lee(30)) < 0)
				return K_NADA;
			b[i] = c;
		}
		dec(b, l, &c);
		return c < 0 ? K_NADA : c;
	}
	return c;
}

static long ahora(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static int doble_clic(void)
{
	static long t;
	static int x, y;
	long n = ahora();
	int d = n - t < 400 && x == rx && y == ry;

	t = d ? 0 : n;
	x = rx;
	y = ry;
	return d;
}

static void vacia(Celda *c)
{
	memset(c->c, 0, sizeof c->c);
	c->c[0] = ' ';
	c->n = c->w = 1;
}

static void pon(int y, int x, const char *s, int l, int w, int a)
{
	Celda *c;

	if (y < 0 || y >= filas || x < 0 || x + w > cols || l > 8)
		return;
	c = pan + y * cols + x;
	if (!c->n && x)
		vacia(c - 1);
	if (c->w == 2 && x + 1 < cols)
		vacia(c + 1);
	memset(c->c, 0, sizeof c->c);
	memcpy(c->c, s, l);
	c->n = l;
	c->w = w;
	c->a = a;
	if (w == 2) {
		if (c[1].w == 2 && x + 2 < cols)
			vacia(c + 2);
		memset(c[1].c, 0, sizeof c->c);
		c[1].n = c[1].w = 0;
		c[1].a = a;
	}
}

static void glifo(int y, int x, int c, const char *s, int l, int w, int a)
{
	char t;
	Celda *p;

	if (c == '\t')
		while (w--)
			pon(y, x++, " ", 1, 1, a);
	else if ((c >= 0 && c < 32) || c == 127) {
		t = c == 127 ? '?' : c + 64;
		pon(y, x, "^", 1, 1, CTL);
		pon(y, x + 1, &t, 1, 1, CTL);
	} else if (raro(c))
		pon(y, x, "?", 1, 1, CTL);
	else if (!w) {
		if (y < 0 || y >= filas || x < 1 || x > cols)
			return;
		p = pan + y * cols + x - 1;
		if (!p->n && x > 1)
			p--;
		if (p->n + l <= 8) {
			memcpy(p->c + p->n, s, l);
			p->n += l;
		}
	} else
		pon(y, x, s, l, w, a);
}

static int diestra(int c)
{
	return (c >= 0x590 && c <= 0x8FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF);
}

static const char *visual(const char *s)
{
	static char o[2048];
	int p[512], t[512], n = 0, i = 0, j, k, a, b, m, l = strlen(s), c, w, hay = 0;
	char *q = o;

	if (l >= (int)sizeof o)
		return s;
	while (i < l && n < 511) {
		p[n] = i;
		i += dec(s + i, l - i, &c);
		t[n] = diestra(c) ? 2 : c < 128 && !isalnum(c) ? 1 : 0;
		hay |= t[n] == 2;
		while (i < l) {
			k = mira(s, i, l, 0, &w, &c);
			if (w || c < 0x300)
				break;
			i = k;
		}
		n++;
	}
	if (!hay || i < l)
		return s;
	p[n] = l;
	for (a = 0; a < n; a = b) {
		if (t[a] != 2) {
			b = a + 1;
			memcpy(q, s + p[a], p[b] - p[a]);
			q += p[b] - p[a];
			continue;
		}
		for (m = b = a; b < n && t[b]; b++)
			if (t[b] == 2)
				m = b;
		for (b = m + 1, k = m; k >= a; k--) {
			j = p[k + 1] - p[k];
			memcpy(q, s + p[k], j);
			if (j == 1 && strchr("()[]", *q))
				*q = "()[]"[(strchr("()[]", *q) - "()[]") ^ 1];
			q += j;
		}
	}
	*q = 0;
	return o;
}

static int texto(int y, int x, const char *s, int a, int max)
{
	int i = 0, j, n, w, c, col = 0;

	s = visual(s);
	n = strlen(s);

	while (i < n) {
		j = mira(s, i, n, col, &w, &c);
		if (col + w > max)
			break;
		glifo(y, x + col, c, s + i, j - i, w, a);
		col += w;
		i = j;
	}
	return col;
}

static void rellena(int y, int x, int n, const char *s, int a)
{
	while (n-- > 0)
		pon(y, x++, s, strlen(s), 1, a);
}

static void caja(int y, int x, int h, int w, const char *t)
{
	int i;

	for (i = 1; i < h - 1; i++) {
		pon(y + i, x, "│", 3, 1, BOR);
		rellena(y + i, x + 1, w - 2, " ", NOR);
		pon(y + i, x + w - 1, "│", 3, 1, BOR);
	}
	pon(y, x, "┌", 3, 1, BOR);
	rellena(y, x + 1, w - 2, "─", BOR);
	pon(y, x + w - 1, "┐", 3, 1, BOR);
	pon(y + h - 1, x, "└", 3, 1, BOR);
	rellena(y + h - 1, x + 1, w - 2, "─", BOR);
	pon(y + h - 1, x + w - 1, "┘", 3, 1, BOR);
	if (t) {
		i = ancho(t) + 2;
		pon(y, x + (w - i) / 2, " ", 1, 1, TIT);
		texto(y, x + (w - i) / 2 + 1, t, TIT, w - 4);
		pon(y, x + (w - i) / 2 + i - 1, " ", 1, 1, TIT);
	}
}

static void raya(int y, int x, int w)
{
	pon(y, x, "├", 3, 1, BOR);
	rellena(y, x + 1, w - 2, "─", BOR);
	pon(y, x + w - 1, "┤", 3, 1, BOR);
}

static void medidas(void)
{
	int i, n = NM, in = cols - 2 - (n - 1), w[NM], t = 0;

	for (i = 0; i < n; i++)
		t += w[i] = ancho(tx(T_M_ARCHIVO + i)) + 2;
	xs[0] = 0;
	for (i = 0; i < n; i++)
		xs[i + 1] = xs[i] + 1 + (t <= in ? w[i] + (in - t) / n + (i < (in - t) % n) : in / n + (i < in % n));
	for (i = nl, gw = 2; i; i /= 10)
		gw++;
	if (gw < 4)
		gw = 4;
	x0 = numeros ? gw + 3 : 2;
	tw = cols - 1 - x0;
	barra = aviso[0] || pide || pista[0];
	th = filas - 4 - 2 * barra;
	if (th < 1)
		th = 1;
	if (tw < 1)
		tw = 1;
}

static void ajusta(void)
{
	int c;

	if (hoja) {
		ajustahoja();
		return;
	}
	if (cy >= nl)
		cy = nl - 1;
	if (cy < 0)
		cy = 0;
	if (cx > L[cy].n)
		cx = L[cy].n;
	c = colde(cy, cx);
	if (fy > nl - th)
		fy = nl - th > 0 ? nl - th : 0;
	if (cy < fy)
		fy = cy;
	if (cy >= fy + th)
		fy = cy - th + 1;
	if (c < fx)
		fx = c;
	if (c >= fx + tw)
		fx = c - tw + 1;
}

static int rango(int *y1, int *x1, int *y2, int *x2)
{
	if (!sel || (ay == cy && ax == cx))
		return 0;
	if (ay < cy || (ay == cy && ax < cx)) {
		*y1 = ay; *x1 = ax; *y2 = cy; *x2 = cx;
	} else {
		*y1 = cy; *x1 = cx; *y2 = ay; *x2 = ax;
	}
	return 1;
}

static int ensel(int y, int x)
{
	if (!hay || y < sy1 || y > sy2)
		return 0;
	if (y == sy1 && x < sx1)
		return 0;
	return y < sy2 || x < sx2;
}

static const char *tlds = " com net org biz io co me tv cc xyz gq ga ml cf tk pw su ru ua by kz cn hk"
	" tw jp kr in id my th vn ph sg au nz uk de fr es it pt nl be ch at se no dk fi cz sk hu ro bg gr tr"
	" il ir iq sa ae eg za ng ke br ar mx cl pe ve us ca eu asia gov edu mil onion ai lt lv ee is ie lu"
	" hr si ba info top online site club shop store live app dev cloud tech space website fun icu vip"
	" work link click buy win bid download stream review party trade science pro mobi int ";
static const char *palabras = " info top online site club shop store live app dev cloud tech space"
	" website fun icu vip work link click buy win bid download stream review party trade science pro ";
static const char *codigo = " this self window document parent super obj ctx req res err event target"
	" module exports console logger log app config settings options props state data item node ";
static const char *ejes = " exe dll scr bat cmd ps1 psm1 vbs vbe jse wsf wsh hta lnk msi msp pif cpl"
	" sys jar apk iso docm xlsm pptm ";

static int enlista(const char *l, const char *s, int n)
{
	char b[16];
	int i;

	if (n < 2 || n > 12)
		return 0;
	b[0] = ' ';
	for (i = 0; i < n; i++)
		b[i + 1] = tolower((unsigned char)s[i]);
	b[n + 1] = ' ';
	b[n + 2] = 0;
	return strstr(l, b) != 0;
}

static int punto(const char *s, int i, int n)
{
	static const char *f[] = { ".", "[.]", "(.)", "{.}", "[dot]", "(dot)" };
	int k, l;

	for (k = 0; k < 6; k++) {
		l = strlen(f[k]);
		if (i + l <= n && !strncasecmp(s + i, f[k], l))
			return l;
	}
	return 0;
}

static int alfa(int c)
{
	return isalnum(c) || c == '-' || c == '_';
}

static int nombre(const char *s, int i, int n, int ejecutable)
{
	int j = i, a, k, fin = -1, partes = 0;

	while (j < n) {
		for (a = j; j < n && (isalnum((unsigned char)s[j]) || s[j] == '-' || (ejecutable && s[j] == '_')); j++)
			;
		if (j == a || s[a] == '-')
			break;
		if (++partes >= 2 && enlista(ejecutable ? ejes : tlds, s + a, j - a))
			fin = j;
		k = punto(s, j, n);
		if (!k || j + k >= n || !isalnum((unsigned char)s[j + k]))
			break;
		j += k;
	}
	if (fin < 0 || (fin < n && alfa((unsigned char)s[fin])))
		return 0;
	if ((k = punto(s, fin, n)) && fin + k < n && isalnum((unsigned char)s[fin + k]))
		return 0;
	if (!ejecutable) {
		for (a = fin; a > i && isalnum((unsigned char)s[a - 1]); a--)
			;
		if (enlista(palabras, s + a, fin - a)) {
			for (k = i; k < n && isalnum((unsigned char)s[k]); k++)
				;
			if ((fin < n && s[fin] == '(') || enlista(codigo, s + i, k - i))
				return 0;
		}
	}
	return fin;
}

static int ip(const char *s, int i, int n)
{
	int j = i, p, v, k, d;

	for (p = 0; p < 4; p++) {
		for (v = d = 0; j < n && isdigit((unsigned char)s[j]) && d < 4; j++, d++)
			v = v * 10 + s[j] - '0';
		if (!d || d > 3 || v > 255)
			return 0;
		if (p < 3) {
			if (!(k = punto(s, j, n)))
				return 0;
			j += k;
		}
	}
	if (j < n && alfa((unsigned char)s[j]))
		return 0;
	if ((k = punto(s, j, n)) && j + k < n && isdigit((unsigned char)s[j + k]))
		return 0;
	return j;
}

static int esquema(const char *s, int i, int n)
{
	static const char *e[] = { "https", "http", "hxxps", "hxxp", "ftp", "fxp" };
	static const char *x[] = { "://", "[://]", "[:]//" };
	int k, l, m;

	for (k = 0; k < 6; k++) {
		l = strlen(e[k]);
		if (i + l > n || strncasecmp(s + i, e[k], l))
			continue;
		for (m = 0; m < 3; m++)
			if (i + l + (int)strlen(x[m]) <= n && !strncmp(s + i + l, x[m], strlen(x[m])))
				return l + strlen(x[m]);
	}
	return 0;
}

static int ioc(const char *s, int i, int n, int *fin)
{
	int j, k, h, d;
	unsigned char c = s[i];

	if (i && (alfa((unsigned char)s[i - 1]) || s[i - 1] == '.'))
		return 0;
	if ((k = esquema(s, i, n))) {
		j = i + k;
		if (!(h = ip(s, j, n)) && !(h = nombre(s, j, n, 0)))
			return 0;
		for (j = h; j < n && !strchr(" \t\"'<>`", s[j]); j++)
			;
		while (j > h && strchr(".,;:)]}!?", s[j - 1]))
			j--;
		*fin = j;
		return 1;
	}
	if (isdigit(c) && (*fin = ip(s, i, n)))
		return 2;
	if (isxdigit(c)) {
		for (j = i, h = d = 0; j < n && isxdigit((unsigned char)s[j]); j++)
			if (isdigit((unsigned char)s[j]))
				d++;
			else
				h++;
		if ((j - i == 32 || j - i == 40 || j - i == 64) && d && h && (j == n || !alfa((unsigned char)s[j]))) {
			*fin = j;
			return 5;
		}
	}
	if (isalnum(c) && (*fin = nombre(s, i, n, 1)))
		return 3;
	if (isalnum(c) && (*fin = nombre(s, i, n, 0)))
		return 4;
	return 0;
}

static int cambia(const char *s, int n, Lin *o, int activar)
{
	int i = 0, j, t, k, c = 0, a, h;
	Lin u = { 0 };

	o->n = 0;
	while (i < n) {
		if (!(t = ioc(s, i, n, &j))) {
			add(o, s + i, 1);
			i++;
			continue;
		}
		u.n = 0;
		k = i;
		h = j;
		if (t == 1) {
			a = esquema(s, i, n);
			for (k = i; k < i + a && s[k] != ':' && s[k] != '['; k++)
				;
			if (activar) {
				if (!strncasecmp(s + i, "hxxp", 4)) {
					add(&u, s + i, 1);
					add(&u, s[i + 1] == 'X' ? "TT" : "tt", 2);
					add(&u, s + i + 3, k - i - 3);
				} else if (!strncasecmp(s + i, "fxp", 3))
					adds(&u, "ftp");
				else
					add(&u, s + i, k - i);
			} else if (!strncasecmp(s + i, "http", 4)) {
				add(&u, s + i, 1);
				add(&u, s[i + 1] == 'T' ? "XX" : "xx", 2);
				add(&u, s + i + 3, k - i - 3);
			} else if (!strncasecmp(s + i, "ftp", 3))
				adds(&u, "fxp");
			else
				add(&u, s + i, k - i);
			adds(&u, "://");
			k = i + a;
			for (h = k; h < j && s[h] != '/' && s[h] != '?' && s[h] != '#'; h++)
				;
		}
		while (k < h) {
			if ((a = punto(s, k, h))) {
				adds(&u, activar ? "." : "[.]");
				k += a;
			} else
				add(&u, s + k++, 1);
		}
		add(&u, s + h, j - h);
		if (t != 5 && (u.n != j - i || memcmp(u.s, s + i, u.n)))
			c++;
		add(o, u.s, u.n);
		i = j;
	}
	free(u.s);
	return c;
}

static void pintalinea(int f, int y)
{
	Lin *l = &L[y];
	int i = 0, j, col = 0, w, c, x, a, k, ri = 0, rf = -1;

	while (i < l->n) {
		j = mira(l->s, i, l->n, col, &w, &c);
		x = col - fx;
		if (x >= tw)
			return;
		if (resalta && i >= rf) {
			for (rf = ri = i; rf < l->n && !ioc(l->s, rf, l->n, &k); rf++)
				;
			ri = rf;
			if (rf < l->n)
				rf = k;
		}
		a = ensel(y, i) ? SEL : resalta && i >= ri && i < rf ? IOC : NOR;
		if (x < 0) {
			for (k = 0; k < x + w; k++)
				pon(f, x0 + k, " ", 1, 1, a);
		} else if (x + w > tw) {
			for (k = x; k < tw; k++)
				pon(f, x0 + k, " ", 1, 1, a);
		} else if (w || x > 0)
			glifo(f, x0 + x, c, l->s + i, j - i, w, a);
		col += w;
		i = j;
	}
	x = col - fx;
	if (ensel(y, l->n) && x >= 0 && x < tw)
		pon(f, x0 + x, " ", 1, 1, SEL);
}

static void pinta(void)
{
	static const Celda bl = { " ", 1, 1, NOR };
	int i, y, gx, n;
	char nb[16];

	medidas();
	for (i = 0; i < filas * cols; i++)
		pan[i] = bl;
	if (cols < 40 || filas < 8) {
		texto(0, 0, tx(T_PEQUENA), NOR, cols);
		return;
	}
	hay = rango(&sy1, &sx1, &sy2, &sx2);
	gx = gw + 1;
	rellena(0, 1, cols - 2, "─", BOR);
	rellena(2, 1, cols - 2, "─", BOR);
	rellena(filas - 1, 1, cols - 2, "─", BOR);
	pon(0, 0, "┌", 3, 1, BOR);
	pon(0, cols - 1, "┐", 3, 1, BOR);
	pon(2, 0, "├", 3, 1, BOR);
	pon(2, cols - 1, "┤", 3, 1, BOR);
	pon(filas - 1, 0, "└", 3, 1, BOR);
	pon(filas - 1, cols - 1, "┘", 3, 1, BOR);
	for (i = 0; i <= NM; i++)
		pon(1, xs[i], "│", 3, 1, BOR);
	for (i = 1; i < NM; i++) {
		pon(0, xs[i], "┬", 3, 1, BOR);
		pon(2, xs[i], "┴", 3, 1, BOR);
	}
	for (i = 0; i < NM; i++) {
		n = xs[i + 1] - xs[i] - 1;
		y = ancho(tx(T_M_ARCHIVO + i));
		if (abierto == i)
			rellena(1, xs[i] + 1, n, " ", INV);
		texto(1, xs[i] + 1 + (n > y ? (n - y) / 2 : 0), tx(T_M_ARCHIVO + i), abierto == i ? INV : MEN, n);
	}
	if (numeros) {
		for (i = 1; i < NM && xs[i] != gx; i++)
			;
		pon(2, gx, i < NM ? "┼" : "┬", 3, 1, BOR);
	}
	for (i = 0; i < th; i++) {
		y = 3 + i;
		pon(y, 0, "│", 3, 1, BOR);
		pon(y, cols - 1, "│", 3, 1, BOR);
		if (numeros) {
			pon(y, gx, "│", 3, 1, BOR);
			if (fy + i < nl && !hoja) {
				n = snprintf(nb, sizeof nb, "%d", fy + i + 1);
				texto(y, 1 + (gw - n + 1) / 2, nb, fy + i == cy ? NUMA : NUM, gw);
			}
		}
		if (fy + i < nl && !hoja)
			pintalinea(y, fy + i);
	}
	if (hoja)
		pintahoja();
	if (barra) {
		y = filas - 3;
		raya(y, 0, cols);
		pon(y + 1, 0, "│", 3, 1, BOR);
		pon(y + 1, cols - 1, "│", 3, 1, BOR);
		if (numeros)
			pon(y, gx, "┴", 3, 1, BOR);
		if (!pide)
			texto(y + 1, 2, aviso[0] ? aviso : pista, NOR, cols - 4);
	} else if (numeros)
		pon(filas - 1, gx, "┴", 3, 1, BOR);
}

static void vuelca(int y, int x)
{
	int f, i, a = -1, p0;
	Celda *p;
	char t[sizeof titulo];

	ob.n = 0;
	adds(&ob, "\033[?25l");
	for (f = 0; f < filas; f++) {
		p = pan + f * cols;
		if (!memcmp(p, ant + f * cols, cols * sizeof *p))
			continue;
		addf(&ob, "\033[%d;1H", f + 1);
		for (i = 0; i < cols; i++) {
			if (!p[i].n)
				continue;
			if (p[i].a != a) {
				a = p[i].a;
				addf(&ob, "\033[%sm", sgr[a]);
			}
			add(&ob, p[i].c, p[i].n);
			if (p[i].w == 2 && i + 2 < cols)
				addf(&ob, "\033[%d;%dH", f + 1, i + 3);
		}
	}
	adds(&ob, "\033[0m");
	snprintf(t, sizeof t, "isi · %s%s", ruta[0] ? base(ruta) : tx(T_SINNOMBRE), mod ? " *" : "");
	for (i = 0, f = strlen(t); i < f; i += a) {
		a = dec(t + i, f - i, &p0);
		if (p0 < 32 || p0 == 127 || raro(p0))
			memset(t + i, '?', a);
	}
	if (strcmp(t, titulo)) {
		strcpy(titulo, t);
		addf(&ob, "\033]0;%s\007", t);
	}
	if (y >= 0)
		addf(&ob, "\033[%d;%dH\033[?25h", y + 1, x + 1);
	salida(ob.s, ob.n);
	memcpy(ant, pan, filas * cols * sizeof *pan);
}

static void hueco(int y, int k)
{
	if (nl + k > capl)
		L = re(L, (capl = (nl + k) * 3 / 2 + 16) * sizeof *L);
	memmove(L + y + k, L + y, (nl - y) * sizeof *L);
	memset(L + y, 0, k * sizeof *L);
	nl += k;
}

static void mete(int y, int x, const char *s, int n, int *ey, int *ex)
{
	int k = 0, i, a;
	Lin t = { 0 };

	for (i = 0; i < n; i++)
		k += s[i] == '\n';
	if (!k) {
		crece(&L[y], L[y].n + n);
		memmove(L[y].s + x + n, L[y].s + x, L[y].n - x);
		memcpy(L[y].s + x, s, n);
		L[y].n += n;
		*ey = y;
		*ex = x + n;
		return;
	}
	add(&t, L[y].s + x, L[y].n - x);
	L[y].n = x;
	hueco(y + 1, k);
	for (i = a = 0; i < n; i++)
		if (s[i] == '\n') {
			add(&L[y++], s + a, i - a);
			a = i + 1;
		}
	add(&L[y], s + a, n - a);
	*ey = y;
	*ex = L[y].n;
	add(&L[y], t.s, t.n);
	free(t.s);
}

static char *trozo(int y1, int x1, int y2, int x2, int *n)
{
	Lin t = { 0 };
	int i;

	if (y1 == y2)
		add(&t, L[y1].s + x1, x2 - x1);
	else {
		add(&t, L[y1].s + x1, L[y1].n - x1);
		for (i = y1 + 1; i <= y2; i++) {
			add(&t, "\n", 1);
			add(&t, L[i].s, i == y2 ? x2 : L[i].n);
		}
	}
	*n = t.n;
	return t.s ? t.s : re(0, 1);
}

static char *saca(int y1, int x1, int y2, int x2, int *n)
{
	char *s = trozo(y1, x1, y2, x2, n);
	int i;

	if (y1 == y2) {
		memmove(L[y1].s + x1, L[y1].s + x2, L[y1].n - x2);
		L[y1].n -= x2 - x1;
		return s;
	}
	L[y1].n = x1;
	add(&L[y1], L[y2].s + x2, L[y2].n - x2);
	for (i = y1 + 1; i <= y2; i++)
		free(L[i].s);
	memmove(L + y1 + 1, L + y2 + 1, (nl - y2 - 1) * sizeof *L);
	nl -= y2 - y1;
	return s;
}

static void apila(Paso **v, int *n, int *c, Paso p)
{
	if (*n == *c)
		*v = re(*v, (*c = *c * 2 + 64) * sizeof **v);
	(*v)[(*n)++] = p;
}

static void suelta(Paso *v, int *n)
{
	while (*n)
		free(v[--*n].s);
}

static void poda(void)
{
	int k, g;
	size_t q;

	while (peso > (size_t)256 << 20 && nd > 1) {
		for (k = 0, g = pd[0].g, q = 0; k < nd && pd[k].g == g; k++) {
			q += pd[k].n;
			free(pd[k].s);
		}
		if (k == nd)
			break;
		memmove(pd, pd + k, (nd - k) * sizeof *pd);
		nd -= k;
		peso -= q;
	}
}

static int bloqueado(void)
{
	if (solo)
		avisa("%s", tx(T_SOLO));
	return solo;
}

static void ins(int y, int x, const char *s, int n)
{
	Paso p = { 1, y, x, grupo, n, 0 };

	if (n <= 0 || bloqueado())
		return;
	p.s = re(0, n);
	memcpy(p.s, s, n);
	mete(y, x, s, n, &cy, &cx);
	apila(&pd, &nd, &cdp, p);
	peso += n;
	suelta(pr, &nr);
	poda();
	mod = 1;
	px = -1;
}

static void del(int y1, int x1, int y2, int x2)
{
	Paso p = { 0, y1, x1, grupo, 0, 0 };

	if ((y1 == y2 && x1 == x2) || bloqueado())
		return;
	p.s = saca(y1, x1, y2, x2, &p.n);
	apila(&pd, &nd, &cdp, p);
	peso += p.n;
	suelta(pr, &nr);
	poda();
	cy = y1;
	cx = x1;
	mod = 1;
	px = -1;
}

static int deshaz(Paso **a, int *na, Paso **b, int *nb, int *cb, int atras)
{
	Paso p;
	int g, ey, ex, n;

	if (!*na || bloqueado())
		return 0;
	g = (*a)[*na - 1].g;
	while (*na && (*a)[*na - 1].g == g) {
		p = (*a)[--*na];
		if (atras)
			peso -= p.n;
		else
			peso += p.n;
		if ((p.t == 1) == atras) {
			ey = p.y;
			ex = p.x;
			for (n = 0; n < p.n; n++)
				if (p.s[n] == '\n')
					ey++, ex = 0;
				else
					ex++;
			free(saca(p.y, p.x, ey, ex, &n));
			cy = p.y;
			cx = p.x;
		} else
			mete(p.y, p.x, p.s, p.n, &cy, &cx);
		apila(b, nb, cb, p);
	}
	sel = 0;
	mod = 1;
	px = -1;
	return 1;
}

static int borrasel(void)
{
	int y1, x1, y2, x2;

	if (!rango(&y1, &x1, &y2, &x2)) {
		sel = 0;
		return 0;
	}
	del(y1, x1, y2, x2);
	sel = 0;
	return 1;
}

static void vacio(void)
{
	while (nl)
		free(L[--nl].s);
	hueco(0, 1);
	cy = cx = fy = fx = sel = mod = crlf = 0;
	px = -1;
	suelta(pd, &nd);
	suelta(pr, &nr);
	peso = 0;
}

static void cargatexto(char *b, size_t n)
{
	size_t i, a, k = 0, r = 0;

	for (i = 0; i < n; i++)
		if (b[i] == '\n') {
			k++;
			r += i && b[i - 1] == '\r';
		}
	vacio();
	crlf = k && r == k;
	hueco(1, k);
	for (i = a = 0, k = 0; i <= n; i++)
		if (i == n || b[i] == '\n') {
			add(&L[k++], b + a, i - a - (crlf && i < n));
			a = i + 1;
		}
}

static int azar(void *p, size_t n)
{
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	ssize_t r;
	size_t t = 0;

	if (fd < 0)
		return 0;
	while (t < n && ((r = read(fd, (char *)p + t, n - t)) > 0 || (r < 0 && errno == EINTR)))
		if (r > 0)
			t += r;
	close(fd);
	return t == n;
}

static int deriva(const char *pw, const uint8_t *s, uint8_t *k)
{
	crypto_argon2_config c = { CRYPTO_ARGON2_ID, MEMA, 3, 1 };
	crypto_argon2_inputs e = { (const uint8_t *)pw, s, strlen(pw), 16 };
	void *w = malloc((size_t)MEMA * 1024);

	if (!w)
		return 0;
	crypto_argon2(k, 32, w, c, e, crypto_argon2_no_extras);
	crypto_wipe(w, (size_t)MEMA * 1024);
	free(w);
	return 1;
}

static int descifra(char **b, size_t *n, const char *pw)
{
	uint8_t *s = (uint8_t *)*b, k[32], *p;
	size_t l = *n - CAB - 16;

	if (!deriva(pw, s + 4, k))
		return 0;
	p = re(0, l + 1);
	if (crypto_aead_unlock(p, s + CAB, k, s + 20, s, CAB, s + CAB + 16, l)) {
		crypto_wipe(k, 32);
		free(p);
		return 0;
	}
	memcpy(sal, s + 4, 16);
	memcpy(clave, k, 32);
	crypto_wipe(k, 32);
	free(*b);
	*b = (char *)p;
	*n = l;
	return 1;
}

static int todo(int fd, const char *d, size_t n)
{
	ssize_t r;

	while (n) {
		r = write(fd, d, n);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			return -1;
		d += r;
		n -= r;
	}
	return 0;
}

static int escribe(const char *p, const char *d, size_t n)
{
	char real[PATH_MAX], tmp[PATH_MAX + 16];
	const char *b;
	struct stat st;
	int fd, ex, e;
	mode_t um;

	if (!realpath(p, real))
		snprintf(real, sizeof real, "%s", p);
	ex = !stat(real, &st);
	if (ex && access(real, W_OK))
		return errno;
	b = strrchr(real, '/');
	snprintf(tmp, sizeof tmp, "%.*s.isi-XXXXXX", b ? (int)(b - real + 1) : 0, real);
	um = umask(0);
	umask(um);
	if ((fd = mkstemp(tmp)) < 0) {
		if ((fd = open(real, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, cif ? 0600 : 0666)) < 0)
			return errno;
		if (todo(fd, d, n) || fsync(fd)) {
			e = errno;
			close(fd);
			return e;
		}
		return close(fd) ? errno : 0;
	}
	if (fchmod(fd, ex ? st.st_mode & 07777 : (cif ? 0600 : 0666) & ~um)) {}
	if (ex && fchown(fd, st.st_uid, st.st_gid)) {}
	if (todo(fd, d, n) || fsync(fd)) {
		e = errno;
		close(fd);
		unlink(tmp);
		return e;
	}
	if (close(fd) || rename(tmp, real)) {
		e = errno;
		unlink(tmp);
		return e;
	}
	return 0;
}

static int guarda(const char *p)
{
	Lin o = { 0 };
	char *c = 0;
	size_t n;
	int i, e;

	for (i = 0; i < nl; i++) {
		add(&o, L[i].s, L[i].n);
		if (i < nl - 1)
			add(&o, "\r\n" + !crlf, 1 + crlf);
	}
	n = o.n;
	if (cif) {
		n = CAB + 16 + o.n;
		c = re(0, n);
		memcpy(c, "ISI1", 4);
		memcpy(c + 4, sal, 16);
		if (!azar(c + 20, 24)) {
			crypto_wipe(o.s, o.c);
			free(o.s);
			free(c);
			avisa("%s", tx(T_URANDOM));
			return 0;
		}
		crypto_aead_lock((uint8_t *)c + CAB + 16, (uint8_t *)c + CAB, clave, (uint8_t *)c + 20,
			(uint8_t *)c, CAB, (uint8_t *)o.s, o.n);
	}
	e = escribe(p, c ? c : o.s ? o.s : "", n);
	if (o.s)
		crypto_wipe(o.s, o.c);
	free(o.s);
	free(c);
	if (e) {
		avisa(tx(T_NOGUARDA), base(p), fallo(e));
		return 0;
	}
	if (p != ruta)
		snprintf(ruta, sizeof ruta, "%s", p);
	mod = 0;
	avisa(tx(cif ? T_GUARDADOC : T_GUARDADO), base(ruta));
	return 1;
}

static void barrapide(const char *et, int *x)
{
	pinta();
	*x = 2 + texto(filas - 2, 2, et, TIT, cols - 4);
	*x += texto(filas - 2, *x, ": ", NOR, cols - 2 - *x);
}

static int fuerza(const char *s, int *r)
{
	int i = 0, n = strlen(s), c, l = 0, p, b = 0, mi = 0, ma = 0, di = 0, es = 0, ot = 0;

	while (i < n) {
		i += dec(s + i, n - i, &c);
		l++;
		if (c >= 'a' && c <= 'z')
			mi = 1;
		else if (c >= 'A' && c <= 'Z')
			ma = 1;
		else if (c >= '0' && c <= '9')
			di = 1;
		else if (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7)
			ot = 1;
		else if (c != ' ')
			es = 1;
	}
	r[0] = l >= 15;
	r[1] = mi || ma || ot;
	r[2] = di;
	r[3] = es;
	p = 26 * mi + 26 * ma + 10 * di + 33 * es + 64 * ot;
	while (p >> b > 1)
		b++;
	return l * b;
}

static int pedir(const char *et, char *b, int max, int modo)
{
	int n, k, x, w, i, r[4] = { 0 }, f, nuevo = !modo;
	char t[1024], m[256];

	pide = 1;
	for (;;) {
		barrapide(et, &x);
		n = strlen(b);
		w = cols - 3 - x - (modo == 2 ? 46 : 0);
		if (w < 4)
			w = 4;
		if (modo) {
			for (i = 0, t[0] = 0; b[i]; i++)
				if ((b[i] & 0xC0) != 0x80 && strlen(t) + 4 < sizeof t)
					strcat(t, "•");
		} else
			snprintf(t, sizeof t, "%s", b);
		for (i = 0; ancho(t + i) > w; )
			for (i++; (t[i] & 0xC0) == 0x80; i++)
				;
		x += texto(filas - 2, x, t + i, CAM, w);
		if (modo == 2) {
			f = fuerza(b, r);
			i = f < 60 ? T_DEBIL : f < 80 ? T_ACEPTABLE : f < 100 ? T_BUENA : T_MUYBUENA;
			snprintf(m, sizeof m, tx(T_REQ), r[0] ? "✓" : "✗", r[1] ? "✓" : "✗", r[2] ? "✓" : "✗",
				r[3] ? "✓" : "✗", n ? tx(i) : "");
			texto(filas - 2, x + 2, m, NOR, cols - 4 - x);
		}
		vuelca(filas - 2, x);
		k = tecla(-1);
		if (k == K_ESC || k == 3 || k == 17) {
			pide = 0;
			return 0;
		}
		if (k == 13) {
			if (modo == 2 && !(r[0] && r[1] && r[2] && r[3]))
				continue;
			pide = 0;
			return 1;
		}
		if (nuevo && (k == K_PEGAR || (k >= 32 && k < K_ESC && k != 127)))
			b[n = 0] = 0;
		if (k != K_TAM && k != K_NADA)
			nuevo = 0;
		if (k == 127 || k == 8) {
			while (n && (b[n - 1] & 0xC0) == 0x80)
				n--;
			b[n ? n - 1 : 0] = 0;
		} else if (k == K_PEGAR) {
			for (i = 0; i < pg.n && pg.s[i] != '\n' && pg.s[i] != '\r' && n + 1 < max; i++)
				b[n++] = pg.s[i];
			b[n] = 0;
		} else if (k >= 32 && k < K_ESC && k != 127) {
			i = cod(k, t);
			if (n + i < max) {
				memcpy(b + n, t, i);
				b[n + i] = 0;
			}
		} else if (k == K_RATON && !rs && (rb & ~28) == 0 && ry == filas - 2 && doble_clic()) {
			pide = 0;
			return 0;
		}
	}
}

static int elegir(const char *q, const char *ops)
{
	char op[4][64], b[128];
	int n = 0, i, f = 0, k, x, xo[5];
	const char *s = ops, *e;

	while (n < 4 && *s) {
		e = strchr(s, '|');
		if (!e)
			e = s + strlen(s);
		snprintf(op[n++], sizeof op[0], "%.*s", (int)(e - s), s);
		s = *e ? e + 1 : e;
	}
	pide = 1;
	for (;;) {
		pinta();
		x = 2 + texto(filas - 2, 2, q, TIT, cols - 4) + 2;
		for (i = 0; i < n; i++) {
			snprintf(b, sizeof b, "[ %s ]", op[i]);
			xo[i] = x;
			x += texto(filas - 2, x, b, i == f ? INV : NOR, cols - 2 - x) + 2;
		}
		xo[n] = x;
		vuelca(-1, -1);
		k = tecla(-1);
		if (k == 9 || k == K_DER || k == K_ABA)
			f = (f + 1) % n;
		else if (k == K_STAB || k == K_IZQ || k == K_ARR)
			f = (f + n - 1) % n;
		else if (k == 13)
			break;
		else if (k == K_ESC) {
			f = -1;
			break;
		} else if (k == K_RATON && !rs && (rb & ~28) == 0 && ry == filas - 2) {
			for (i = 0; i < n && !(rx >= xo[i] && rx < xo[i + 1] - 2); i++)
				;
			if (i < n) {
				f = i;
				break;
			}
		} else if (k < 128) {
			for (i = 0; i < n && tolower((unsigned char)op[i][0]) != tolower(k); i++)
				;
			if (i < n) {
				f = i;
				break;
			}
		}
	}
	pide = 0;
	return f;
}

static int guardar(void);

static char *leetodo(const char *p, size_t *n, int *e);

static int abre(const char *p)
{
	size_t n;
	int e;
	char *b, pw[256];

	if (!(b = leetodo(p, &n, &e))) {
		if (e != ENOENT) {
			avisa(tx(T_NOABRE), base(p), fallo(e));
			return 0;
		}
		vacio();
		cif = 0;
		snprintf(ruta, sizeof ruta, "%s", p);
		hoja = 0;
		if (forzado || escsv(p))
			iniciahoja();
		avisa(tx(T_NUEVOF), base(p));
		return 1;
	}
	if (n >= CAB + 16 && !memcmp(b, "ISI1", 4)) {
		for (e = 0;; e = 1) {
			pw[0] = 0;
			if (!pedir(tx(e ? T_PASSMAL : T_PASS), pw, sizeof pw, 1)) {
				crypto_wipe(pw, sizeof pw);
				free(b);
				return -1;
			}
			avisa("%s", tx(T_DESCIFRANDO));
			pinta();
			vuelca(-1, -1);
			if (descifra(&b, &n, pw))
				break;
		}
		crypto_wipe(pw, sizeof pw);
		cif = 1;
		aviso[0] = 0;
	} else {
		cif = 0;
		crypto_wipe(clave, 32);
	}
	cargatexto(b, n);
	if (cif)
		crypto_wipe(b, n);
	free(b);
	snprintf(ruta, sizeof ruta, "%s", p);
	hoja = 0;
	if (forzado || escsv(p))
		iniciahoja();
	return 1;
}

static int pliega(int c)
{
	static const char t[] = "aaaaaaaceeeeiiiidnooooo*ouuuuyts";

	if (c >= 0xC0 && c <= 0xFF && c != 0xD7 && c != 0xF7)
		return t[(c - 0xC0) & 31];
	return c < 128 ? tolower(c) : c;
}

static int cmpnom(const char *a, const char *b)
{
	int i = 0, j = 0, n = strlen(a), m = strlen(b), c, d;

	while (i < n && j < m) {
		i += dec(a + i, n - i, &c);
		j += dec(b + j, m - j, &d);
		if ((c = pliega(c)) != (d = pliega(d)))
			return c - d;
	}
	return i < n ? 1 : j < m ? -1 : strcmp(a, b);
}

static int prefijo(const char *a, const char *p)
{
	int i = 0, j = 0, n = strlen(a), m = strlen(p), c, d;

	while (j < m) {
		if (i >= n)
			return 0;
		i += dec(a + i, n - i, &c);
		j += dec(p + j, m - j, &d);
		if (pliega(c) != pliega(d))
			return 0;
	}
	return 1;
}

static int cmpent(const void *a, const void *b)
{
	const Ent *x = a, *y = b;

	if (!strcmp(x->n, ".."))
		return -1;
	if (!strcmp(y->n, ".."))
		return 1;
	if (x->d != y->d)
		return y->d - x->d;
	return cmpnom(x->n, y->n);
}

static int lista(const char *dir, Ent **e, int n)
{
	DIR *d = opendir(dir);
	struct dirent *de;
	struct stat st;
	char p[PATH_MAX * 2];
	int c = 0;

	if (!d)
		return -1;
	while (n)
		free((*e)[--n].n);
	free(*e);
	*e = 0;
	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, "."))
			continue;
		if (n == c)
			*e = re(*e, (c = c * 2 + 32) * sizeof **e);
		snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
		(*e)[n].n = strdup(de->d_name);
		(*e)[n++].d = !stat(p, &st) && S_ISDIR(st.st_mode);
	}
	closedir(d);
	qsort(*e, n, sizeof **e, cmpent);
	return n;
}

static int explora(int g, int tit, char *out)
{
	char dir[PATH_MAX], nom[256] = "", p[PATH_MAX * 2], t[PATH_MAX], vie[256], pre[64] = "";
	long tpre = 0;
	Ent *e = 0;
	int n, s = 0, top = 0, foco = g, k, h, w, y0, xb, lh, i, y, r = 0, act, d;
	struct stat st;

	if (ruta[0]) {
		snprintf(p, sizeof p, "%s", ruta);
		if (strrchr(p, '/'))
			*strrchr(p, '/') = 0;
		else
			strcpy(p, ".");
		if (!p[0])
			strcpy(p, "/");
		if (g)
			snprintf(nom, sizeof nom, "%.255s", base(ruta));
	} else
		strcpy(p, ".");
	if (!realpath(p, dir) && !getcwd(dir, sizeof dir))
		strcpy(dir, "/");
	if ((n = lista(dir, &e, 0)) < 0) {
		strcpy(dir, "/");
		n = lista(dir, &e, 0);
	}
	if (n < 0) {
		avisa("%s", tx(T_NOLEE));
		return 0;
	}
	for (;;) {
		pinta();
		h = th;
		w = cols - 8 > 76 ? 76 : cols - 8;
		y0 = 3;
		xb = (cols - w) / 2;
		lh = h - 6 - (g ? 2 : 0);
		if (lh < 1)
			lh = 1;
		if (s >= n)
			s = n - 1;
		if (s < 0)
			s = 0;
		if (s < top)
			top = s;
		if (s >= top + lh)
			top = s - lh + 1;
		caja(y0, xb, h, w, tx(g ? T_T_COMO : tit));
		snprintf(t, sizeof t, "%s", dir);
		for (k = 0; ancho(t + k) > w - 4; )
			for (k++; (t[k] & 0xC0) == 0x80; k++)
				;
		texto(y0 + 1, xb + 2, t + k, NUM, w - 4);
		raya(y0 + 2, xb, w);
		for (i = 0; i < lh && top + i < n; i++) {
			y = y0 + 3 + i;
			act = top + i == s;
			if (act)
				rellena(y, xb + 1, w - 2, " ", foco ? CAM : INV);
			snprintf(t, sizeof t, "%s %s%s", e[top + i].d ? "▸" : " ", e[top + i].n, e[top + i].d ? "/" : "");
			texto(y, xb + 2, t, act ? (foco ? CAM : INV) : e[top + i].d ? CAR : NOR, w - 4);
		}
		y = y0 + 3 + lh;
		if (g) {
			raya(y, xb, w);
			k = 2 + texto(y + 1, xb + 2, tx(T_NOMBRE), TIT, w - 4);
			k += texto(y + 1, xb + k, nom, foco ? CAM : NOR, w - 2 - k);
			y += 2;
		}
		raya(y, xb, w);
		texto(y + 1, xb + 2, tx(g ? T_PIE_COMO : T_PIE_ABRIR), NUM, w - 4);
		vuelca(g && foco ? y - 1 : -1, xb + k);
		k = tecla(-1);
		act = 0;
		if (k == K_ESC)
			break;
		if (k == K_RATON) {
			if (rs || (rb & 32))
				continue;
			if (rb == 64 || rb == 65) {
				s += rb == 64 ? -3 : 3;
				continue;
			}
			if (rb & 3)
				continue;
			d = doble_clic();
			y = ry - y0 - 3;
			if (rx > xb && rx < xb + w - 1 && y >= 0 && y < lh && top + y < n) {
				act = (top + y == s && !foco) || d;
				s = top + y;
				foco = 0;
				if (!act)
					continue;
			} else if (g && ry == y0 + 4 + lh) {
				foco = 1;
				continue;
			} else
				continue;
		}
		if (k == 9 || k == K_STAB) {
			foco = g && !foco;
			continue;
		}
		if (k == K_ARR || k == K_ABA || k == K_REP || k == K_AVP || k == K_INI || k == K_FIN) {
			switch (k) {
			case K_ARR: s--; break;
			case K_ABA: s++; break;
			case K_REP: s -= lh; break;
			case K_AVP: s += lh; break;
			case K_INI: s = 0; break;
			default: s = n - 1;
			}
			foco = 0;
			continue;
		}
		if (k == 127 || k == 8) {
			if (foco && nom[0]) {
				i = strlen(nom);
				while (i && (nom[i - 1] & 0xC0) == 0x80)
					i--;
				nom[i ? i - 1 : 0] = 0;
				continue;
			}
			snprintf(vie, sizeof vie, "%s", base(dir));
			snprintf(p, sizeof p, "%s/..", dir);
		} else if (k == 13 || act) {
			if (foco) {
				if (!nom[0])
					continue;
				snprintf(p, sizeof p, "%s/%s", strcmp(dir, "/") ? dir : "", nom);
				if (!stat(p, &st) && S_ISDIR(st.st_mode)) {
					nom[0] = 0;
					vie[0] = 0;
				} else {
					if (!stat(p, &st)) {
						snprintf(t, sizeof t, tx(T_EXISTE), base(p));
						if (elegir(t, ops(2, T_SI, T_NO)))
							continue;
					}
					if (strlen(p) >= PATH_MAX)
						continue;
					strcpy(out, p);
					r = 1;
					break;
				}
			} else {
				if (!n)
					continue;
				snprintf(p, sizeof p, "%s/%s", strcmp(dir, "/") ? dir : "", e[s].n);
				if (!e[s].d) {
					if (g) {
						snprintf(nom, sizeof nom, "%s", e[s].n);
						foco = 1;
						continue;
					}
					if (strlen(p) >= PATH_MAX)
						continue;
					strcpy(out, p);
					r = 1;
					break;
				}
				if (!strcmp(e[s].n, ".."))
					snprintf(vie, sizeof vie, "%s", base(dir));
				else
					vie[0] = 0;
			}
		} else if (k >= 32 && k < K_ESC) {
			if (g) {
				i = cod(k, t);
				if (strlen(nom) + i < sizeof nom && k != '/') {
					k = strlen(nom);
					memcpy(nom + k, t, i);
					nom[k + i] = 0;
					foco = 1;
				}
			} else {
				if (ahora() - tpre > 1000)
					pre[0] = 0;
				tpre = ahora();
				i = cod(k, t);
				if ((k = strlen(pre)) + i < (int)sizeof pre) {
					memcpy(pre + k, t, i);
					pre[k + i] = 0;
				}
				for (i = 0; i < n; i++)
					if (prefijo(e[(s + i) % n].n, pre)) {
						s = (s + i) % n;
						break;
					}
			}
			continue;
		} else
			continue;
		if (!realpath(p, t) || (i = lista(t, &e, n)) < 0) {
			avisa(tx(T_NOENTRA), base(p));
			continue;
		}
		strcpy(dir, t);
		n = i;
		s = top = 0;
		for (i = 0; vie[0] && i < n; i++)
			if (!strcmp(e[i].n, vie))
				s = i;
	}
	while (n > 0)
		free(e[--n].n);
	free(e);
	return r;
}

static void rescata(void)
{
	char p[PATH_MAX + 8];

	if (mod && nl) {
		snprintf(p, sizeof p, "%s~", ruta[0] ? ruta : "isi");
		if (guarda(p)) {
			normal();
			fprintf(stderr, "isi: ");
			fprintf(stderr, tx(T_RESCATADO), p);
			fputc('\n', stderr);
			crypto_wipe(clave, 32);
			_exit(1);
		}
	}
	normal();
	crypto_wipe(clave, 32);
	_exit(1);
}

static int guarda_como(void)
{
	char p[PATH_MAX];

	return explora(1, T_T_COMO, p) && guarda(p);
}

static int guardar(void)
{
	return ruta[0] ? guarda(ruta) : guarda_como();
}

static int sigue(void)
{
	if (!mod)
		return 1;
	switch (elegir(tx(T_CAMBIOS), ops(3, T_SI, T_NO, T_CANCELAR))) {
	case 0: return guardar();
	case 1: return 1;
	}
	return 0;
}

static void cifrar(void)
{
	char a[256] = "", b[256] = "";
	int ok = 0;

	if (pedir(tx(cif ? T_NUEVAPASS : T_PASS), a, sizeof a, 2)) {
		if (pedir(tx(T_REPITE), b, sizeof b, 1)) {
			if (strcmp(a, b))
				avisa("%s", tx(T_NOCOINC));
			else
				ok = 1;
		}
	}
	if (ok) {
		avisa("%s", tx(T_CALCULANDO));
		pinta();
		vuelca(-1, -1);
		if (azar(sal, 16) && deriva(a, sal, clave)) {
			cif = 1;
			mod = 1;
			if (!guardar())
				avisa("%s", tx(T_CIFRADOACT));
		} else
			avisa("%s", tx(T_NOCLAVE));
	}
	crypto_wipe(a, sizeof a);
	crypto_wipe(b, sizeof b);
}

static int sepen(int e, int y, int x)
{
	if (x < 0 || x >= L[y].n)
		return 0;
	return e == 1 ? L[y].s[x] == '|' : x + 3 <= L[y].n && !memcmp(L[y].s + x, "│", 3);
}

static int estab(int y)
{
	unsigned char *s = (unsigned char *)L[y].s;

	if (L[y].n && s[0] == '|')
		return 1;
	if (L[y].n >= 3 && s[0] == 0xE2 && s[1] == 0x94 && (s[2] == 0x8C || s[2] == 0x9C || s[2] == 0x94 || s[2] == 0x82))
		return 2;
	return 0;
}

static int filat(int y)
{
	int e = estab(y), i;

	if (e == 2)
		return (unsigned char)L[y].s[2] == 0x82;
	if (e == 1) {
		for (i = 0; i < L[y].n && L[y].s[i] && strchr("|-: ", L[y].s[i]); i++)
			;
		return i < L[y].n || !memmem(L[y].s, L[y].n, "---", 3);
	}
	return 0;
}

static void suelta_t(Tabla *t)
{
	size_t i, n = (size_t)t->f * t->kc;

	for (i = 0; i < n; i++)
		free(t->c[i]);
	free(t->c);
	t->c = 0;
	t->f = t->fc = t->kc = 0;
}

static char **matriz(int f, int k)
{
	char **m = re(0, (size_t)f * k * sizeof *m);

	memset(m, 0, (size_t)f * k * sizeof *m);
	return m;
}

static void reserva(Tabla *t, int f, int k)
{
	char **c;
	int i, n = t->kc;

	if (k > n) {
		n = k + 4;
		c = matriz(t->fc, n);
		for (i = 0; i < t->f; i++)
			memcpy(c + (size_t)i * n, t->c + (size_t)i * t->kc, t->kc * sizeof *c);
		free(t->c);
		t->c = c;
		t->kc = n;
	}
	if (f > t->fc) {
		i = f + f / 2 + 16;
		t->c = re(t->c, (size_t)i * t->kc * sizeof *t->c);
		memset(t->c + (size_t)t->fc * t->kc, 0, (size_t)(i - t->fc) * t->kc * sizeof *t->c);
		t->fc = i;
	}
}

static void mueve_filas(char **m, Tabla *t, int de, int a, int n)
{
	memmove(m + (size_t)a * t->kc, m + (size_t)de * t->kc, (size_t)n * t->kc * sizeof *m);
}

static void filanueva(Tabla *t, int at)
{
	reserva(t, t->f + 1, t->k > 1 ? t->k : 1);
	mueve_filas(t->c, t, at, at + 1, t->f - at);
	memset(&CE(t, at, 0), 0, t->kc * sizeof *t->c);
	t->f++;
}

static void quitafila(Tabla *t, int at)
{
	int j;

	for (j = 0; j < t->kc; j++)
		free(CE(t, at, j));
	mueve_filas(t->c, t, at + 1, at, t->f - at - 1);
	t->f--;
	memset(&CE(t, t->f, 0), 0, t->kc * sizeof *t->c);
}

static void colnueva(Tabla *t, int at)
{
	int i;

	reserva(t, t->f, t->k + 1);
	for (i = 0; i < t->f; i++) {
		memmove(&CE(t, i, at + 1), &CE(t, i, at), (t->k - at) * sizeof *t->c);
		CE(t, i, at) = 0;
	}
	t->k++;
}

static void quitacol(Tabla *t, int at)
{
	int i;

	for (i = 0; i < t->f; i++) {
		free(CE(t, i, at));
		memmove(&CE(t, i, at), &CE(t, i, at + 1), (t->kc - at - 1) * sizeof *t->c);
		CE(t, i, t->kc - 1) = 0;
	}
	t->k--;
}

static void bloque(int y, int *y0, int *y1)
{
	static int a = -1, b, n;
	int e = estab(y);

	if (n != nl || y < a || y > b || estab(a) != e || estab(b) != e || (a && estab(a - 1) == e) ||
		(b < nl - 1 && estab(b + 1) == e)) {
		for (a = y; a > 0 && estab(a - 1) == e; a--)
			;
		for (b = y; b < nl - 1 && estab(b + 1) == e; b++)
			;
		n = nl;
	}
	*y0 = a;
	*y1 = b;
}

static int grande(int y)
{
	int y0, y1;

	bloque(y, &y0, &y1);
	return y1 - y0 + 1 > GRANDE;
}

static int anchos(int y, int *w)
{
	static const int sep[] = { '|', 0x2502, 0x250C, 0x252C, 0x2510, 0x251C, 0x253C, 0x2524, 0x2514, 0x2534, 0x2518 };
	int i = 0, k = -1, col = 0, a, c, j;

	while (i < L[y].n) {
		i = mira(L[y].s, i, L[y].n, 0, &a, &c);
		for (j = 0; j < 11 && sep[j] != c; j++)
			;
		if (j == 11) {
			col += a;
			continue;
		}
		if (k >= 0 && k < MAXC)
			w[k] = col - 2;
		k++;
		col = 0;
	}
	return k;
}

static int anchon(const char *s, int n)
{
	int i = 0, w, c, col = 0;

	while (i < n) {
		i = mira(s, i, n, col, &w, &c);
		col += w;
	}
	return col;
}

static int finceld(int e, int y, int i)
{
	int p, b;

	for (p = i; p < L[y].n && !sepen(e, y, p); p++)
		;
	for (b = p; b > i && L[y].s[b - 1] == ' '; b--)
		;
	return b > i ? b : i + 1 < p ? i + 1 : i;
}

static int junta(int e, int y, int i, int borde)
{
	const unsigned char *s = (const unsigned char *)L[y].s + i;

	if (e == 1)
		return *s == '|';
	if (i + 3 > L[y].n || s[0] != 0xE2 || s[1] != 0x94)
		return 0;
	if (s[2] == 0x82)
		return 3;
	return borde && memchr("\x8c\x90\x94\x98\x9c\xa4\xac\xb4\xbc", s[2], 9) ? 3 : 0;
}

static void ensancha(int y0, int y1, int col, int d)
{
	Lin b = { 0 }, g = { 0 };
	int e = estab(y0), y, i, k, l, f, sy = cy, sx = cx;

	for (k = 0; k < d; k++) {
		adds(&b, " ");
		adds(&g, e == 1 ? "-" : "─");
	}
	for (y = y0; y <= y1; y++) {
		f = filat(y);
		for (i = k = 0; i < L[y].n; i += l ? l : 1)
			if ((l = junta(e, y, i, !f)) && k++ == col + 1)
				break;
		if (i < L[y].n)
			ins(y, i, f ? b.s : g.s, f ? b.n : g.n);
	}
	free(b.s);
	free(g.s);
	cy = sy;
	cx = sx;
}

static int rapido(int otra)
{
	int e = estab(cy), sl = e == 1 ? 1 : 3, w[MAXC], k, y0, y1, i, j, p, a, b, n = L[cy].n, x = cx, d, ok = 1;
	char *s = L[cy].s;
	Lin o = { 0 };

	bloque(cy, &y0, &y1);
	if (y0 == y1 || (k = anchos(cy > y0 ? cy - 1 : cy + 1, w)) <= 0 || k > MAXC)
		return 0;
	adds(&o, e == 1 ? "|" : "│");
	for (i = sl, j = 0; ok && j < k; j++) {
		for (p = i; p < n && !sepen(e, cy, p); p++)
			;
		if (i > n || (p == n && j < k - 1)) {
			ok = 0;
			break;
		}
		for (a = i; a < p && s[a] == ' '; a++)
			;
		for (b = p; b > a && s[b - 1] == ' '; b--)
			;
		d = -1;
		if (cx >= i && cx <= p) {
			if (cx > a && cx == b + 1)
				b = cx;
			d = cx > a ? (cx < b ? cx : b) - a : 0;
		}
		if (anchon(s + a, b - a) > w[j]) {
			free(o.s);
			if (!otra)
				return 0;
			ensancha(y0, y1, j, anchon(s + a, b - a) - w[j] + (y1 - y0 > 20000 ? 16 : 8));
			return rapido(0);
		}
		adds(&o, " ");
		if (d >= 0)
			x = o.n + d;
		add(&o, s + a, b - a);
		for (a = anchon(s + a, b - a); a <= w[j]; a++)
			adds(&o, " ");
		adds(&o, e == 1 ? "|" : "│");
		i = p + sl;
	}
	if (ok && i < n)
		ok = 0;
	if (ok && (o.n != n || memcmp(o.s, s, n))) {
		del(cy, 0, cy, n);
		ins(cy, 0, o.s, o.n);
	}
	free(o.s);
	if (ok) {
		cx = x;
		px = -1;
	}
	return ok;
}

static int navega(int a)
{
	int e = estab(cy), sl = e == 1 ? 1 : 3, y, i, p, q;

	if (a == 1 || a == 2) {
		for (p = cx; p < L[cy].n && !sepen(e, cy, p); p++)
			;
		if (a == 1 && p + sl < L[cy].n) {
			cx = finceld(e, cy, p + sl);
			return 1;
		}
		for (y = cy + 1; y < nl && estab(y) == e && !filat(y); y++)
			;
		if (y >= nl || estab(y) != e)
			return 0;
		cy = y;
		cx = finceld(e, y, sl);
		return 1;
	}
	for (i = cx; i > 0 && !sepen(e, cy, i - sl); i--)
		;
	for (q = i - sl, p = q; p > 0 && !sepen(e, cy, p - sl); p--)
		;
	if (q > 0) {
		cx = finceld(e, cy, p);
		return 1;
	}
	for (y = cy - 1; y >= 0 && estab(y) == e && !filat(y); y--)
		;
	if (y < 0 || estab(y) != e)
		return 1;
	for (q = L[y].n - sl, p = q; p > 0 && !sepen(e, y, p - sl); p--)
		;
	cy = y;
	cx = finceld(e, y, p);
	return 1;
}

static int leer(int y, Tabla *t, int *tr, int *tc, int *off)
{
	int e = estab(y), sl = e == 1 ? 1 : 3, yy, i, j, p, a, b, n;
	char *s;

	memset(t, 0, sizeof *t);
	if (!e)
		return 0;
	t->e = e;
	t->viva = 1;
	bloque(y, &t->y0, &t->y1);
	for (yy = t->y0; yy <= t->y1; yy++) {
		if (!filat(yy))
			continue;
		filanueva(t, t->f);
		s = L[yy].s;
		n = L[yy].n;
		if (tr && yy == cy) {
			*tr = t->f - 1;
			*tc = *off = 0;
		}
		for (i = sl, j = 0; i <= n && j < MAXC; j++) {
			for (p = i; p < n && !sepen(e, yy, p); p++)
				;
			for (a = i; a < p && s[a] == ' '; a++)
				;
			for (b = p; b > a && s[b - 1] == ' '; b--)
				;
			if (p == n && a == b)
				break;
			if (tr && yy == cy && cx >= i && cx <= p) {
				*tc = j;
				if (cx > a) {
					if (cx == b + 1)
						b = cx;
					*off = (cx < b ? cx : b) - a;
				}
			}
			reserva(t, t->f, j + 1);
			CE(t, t->f - 1, j) = strndup(s + a, b - a);
			if (p == n) {
				j++;
				break;
			}
			i = p + sl;
		}
		if (j > t->k)
			t->k = j;
	}
	if (!t->k)
		t->k = 1;
	if (tr && *tc >= t->k)
		*tc = t->k - 1;
	return t->f > 0;
}

static void borde(Lin *o, int k, int *w, const char *a, const char *m, const char *z)
{
	int j, i;

	adds(o, a);
	for (j = 0; j < k; j++) {
		for (i = 0; i < w[j] + 2; i++)
			adds(o, "─");
		adds(o, j < k - 1 ? m : z);
	}
	adds(o, "\n");
}

static void hazt(Tabla *t, Lin *o, int tr, int tc, int off, int *ny, int *nx)
{
	int w[MAXC], wp[MAXC], i, j, y = 0, ls, a, l;
	const char *v = t->e == 1 ? "|" : "│", *c;

	for (j = 0; j < t->k; j++)
		for (w[j] = 3, i = 0; i < t->f; i++)
			if (CE(t, i, j) && (a = ancho(CE(t, i, j))) > w[j])
				w[j] = a;
	if (t->viva && t->f > GRANDE && anchos(t->e == 1 ? t->y0 + 1 : t->y0, wp) == t->k) {
		for (j = 0; j < t->k; j++) {
			if (w[j] <= wp[j])
				w[j] = wp[j];
			else
				w[j] += t->f > 20000 ? 16 : 8;
		}
	}
	o->n = 0;
	*ny = *nx = 0;
	if (t->e == 2) {
		borde(o, t->k, w, "┌", "┬", "┐");
		y++;
	}
	for (i = 0; i < t->f; i++) {
		ls = o->n;
		adds(o, v);
		for (j = 0; j < t->k; j++) {
			c = CE(t, i, j) ? CE(t, i, j) : "";
			adds(o, " ");
			if (i == tr && j == tc) {
				l = strlen(c);
				*ny = y;
				*nx = o->n - ls + (off < l ? off : l);
			}
			adds(o, c);
			for (a = ancho(c); a < w[j] + 1; a++)
				adds(o, " ");
			adds(o, v);
		}
		adds(o, "\n");
		y++;
		if (!i && t->e == 1) {
			adds(o, "|");
			for (j = 0; j < t->k; j++) {
				for (a = 0; a < w[j] + 2; a++)
					adds(o, "-");
				adds(o, "|");
			}
			adds(o, "\n");
			y++;
		} else if (t->e == 2 && i < t->f - 1) {
			borde(o, t->k, w, "├", "┼", "┤");
			y++;
		}
	}
	if (t->e == 2)
		borde(o, t->k, w, "└", "┴", "┘");
	o->n--;
}

static void aplica(Tabla *t, Lin *o, int ny, int nx)
{
	int n;
	char *s = trozo(t->y0, 0, t->y1, L[t->y1].n, &n);

	if (n != o->n || memcmp(s, o->s, n)) {
		del(t->y0, 0, t->y1, L[t->y1].n);
		ins(t->y0, 0, o->s, o->n);
	}
	free(s);
	cy = t->y0 + ny;
	cx = nx;
	px = -1;
	sel = 0;
}

static void tfix(void)
{
	Tabla t = { 0 };
	Lin o = { 0 };
	int tr = -1, tc = 0, off = 0, ny, nx;

	if (!filat(cy) || (grande(cy) && rapido(1)))
		return;
	if (leer(cy, &t, &tr, &tc, &off)) {
		hazt(&t, &o, tr, tc, off, &ny, &nx);
		aplica(&t, &o, ny, nx);
	}
	suelta_t(&t);
	free(o.s);
}

static void topera(int a)
{
	Tabla t = { 0 };
	Lin o = { 0 };
	int tr = -1, tc = 0, off = 0, ny, nx;

	if ((a == 1 || a == -1 || a == 2) && filat(cy) && grande(cy) && navega(a))
		return;
	if (!filat(cy) || !leer(cy, &t, &tr, &tc, &off)) {
		suelta_t(&t);
		avisa("%s", tx(T_SINTABLA));
		return;
	}
	switch (a) {
	case 1:
		if (++tc >= t.k) {
			tc = 0;
			tr++;
		}
		break;
	case -1:
		if (--tc >= 0)
			break;
		tc = tr ? t.k - 1 : 0;
		if (tr)
			tr--;
		break;
	case 2:
		tr++;
		tc = 0;
		break;
	case A_FILA:
		filanueva(&t, ++tr);
		break;
	case A_COL:
		if (t.k < MAXC)
			colnueva(&t, ++tc);
		break;
	case A_QFILA:
		if (t.f < 2) {
			avisa("%s", tx(T_UNAFILA));
			break;
		}
		quitafila(&t, tr);
		if (tr >= t.f)
			tr = t.f - 1;
		break;
	case A_QCOL:
		if (t.k < 2) {
			avisa("%s", tx(T_UNACOL));
			break;
		}
		quitacol(&t, tc);
		if (tc >= t.k)
			tc = t.k - 1;
		break;
	}
	if (tr >= t.f)
		filanueva(&t, t.f);
	off = CE(&t, tr, tc) ? (int)strlen(CE(&t, tr, tc)) : 0;
	hazt(&t, &o, tr, tc, off, &ny, &nx);
	aplica(&t, &o, ny, nx);
	suelta_t(&t);
	free(o.s);
}

static int formtabla(int *k, int *f, int *e, const char *tt)
{
	int foco = 0, t, w, h = 10, y, x, xf, lw = 0, *v, i, xe[3], xb[3], a, c;
	char b[160], d[160];

	for (i = 0; i < 3; i++)
		if (ancho(tx(T_COLUMNAS + i)) > lw)
			lw = ancho(tx(T_COLUMNAS + i));
	for (;;) {
		pinta();
		snprintf(b, sizeof b, " %s %s ", *e == 1 ? "(•)" : "( )", tx(T_MARKDOWN));
		snprintf(d, sizeof d, " %s %s ", *e == 2 ? "(•)" : "( )", tx(T_CUADRO));
		a = ancho(b);
		c = ancho(d);
		w = 8 + lw + a + c;
		i = ancho(tx(T_B_INSERTAR)) + ancho(tx(T_CANCELAR)) + 16;
		if (i > w)
			w = i;
		if (ancho(tx(T_PIE_FORM)) + 6 > w)
			w = ancho(tx(T_PIE_FORM)) + 6;
		if (ancho(tt) + 8 > w)
			w = ancho(tt) + 8;
		if (w > cols - 2)
			w = cols - 2;
		y = filas - h > 6 ? 3 + (filas - 4 - h) / 2 : 1;
		x = (cols - w) / 2;
		xf = x + 4 + lw;
		caja(y, x, h, w, tt);
		for (i = 0; i < 3; i++)
			texto(y + 2 + i, x + 3, tx(T_COLUMNAS + i), TIT, lw);
		for (i = 0; i < 2; i++) {
			snprintf(b + 100, 50, " ◀ %3d ▶ ", i ? *f : *k);
			texto(y + 2 + i, xf, b + 100, foco == i ? INV : NOR, w - 4 - lw);
		}
		xe[0] = xf;
		xe[1] = xf + a + 1;
		xe[2] = xe[1] + c;
		snprintf(b, sizeof b, " %s %s ", *e == 1 ? "(•)" : "( )", tx(T_MARKDOWN));
		texto(y + 4, xe[0], b, foco == 2 ? INV : NOR, a);
		texto(y + 4, xe[1], d, foco == 2 ? INV : NOR, c);
		snprintf(b, sizeof b, "[ %s ]", tx(T_B_INSERTAR));
		snprintf(d, sizeof d, "[ %s ]", tx(T_CANCELAR));
		xb[0] = x + (w - ancho(b) - ancho(d) - 4) / 2;
		xb[1] = xb[0] + ancho(b) + 4;
		xb[2] = xb[1] + ancho(d);
		texto(y + 6, xb[0], b, foco == 3 ? INV : NOR, ancho(b));
		texto(y + 6, xb[1], d, foco == 4 ? INV : NOR, ancho(d));
		texto(y + 8, x + 3, tx(T_PIE_FORM), NUM, w - 6);
		vuelca(-1, -1);
		t = tecla(-1);
		v = foco ? f : k;
		if (t == K_ESC)
			return 0;
		if (t == 9 || t == K_ABA)
			foco = (foco + 1) % 5;
		else if (t == K_STAB || t == K_ARR)
			foco = (foco + 4) % 5;
		else if (t == K_IZQ || t == K_DER || t == '-' || t == '+' || t == ' ') {
			if (foco < 2)
				*v += t == K_IZQ || t == '-' ? -1 : 1;
			else if (foco == 2)
				*e = 3 - *e;
			else
				foco = 7 - foco;
		} else if (t >= '0' && t <= '9' && foco < 2) {
			i = *v * 10 + t - '0';
			*v = i > (foco ? 999 : MAXC) ? t - '0' : i;
		} else if ((t == 127 || t == 8) && foco < 2)
			*v /= 10;
		else if (t == 13) {
			if (foco == 4)
				return 0;
			if (*k >= 1 && *f >= 1)
				return 1;
		} else if (t == K_RATON && !rs && !(rb & ~28)) {
			i = ry - y - 2;
			if (i >= 0 && i < 2) {
				foco = i;
				v = i ? f : k;
				if (rx >= xf && rx <= xf + 2)
					--*v;
				else if (rx >= xf + 6 && rx <= xf + 8)
					++*v;
			} else if (i == 2) {
				foco = 2;
				if (rx >= xe[0] && rx < xe[1])
					*e = 1;
				else if (rx >= xe[1] && rx < xe[2])
					*e = 2;
			} else if (ry == y + 6 && rx >= xb[0] && rx < xb[1] - 4)
				return *k >= 1 && *f >= 1;
			else if (ry == y + 6 && rx >= xb[1] && rx < xb[2])
				return 0;
		}
		if (*k > MAXC)
			*k = MAXC;
		if (*f > 999)
			*f = 999;
		if (*k < 0)
			*k = 0;
		if (*f < 0)
			*f = 0;
	}
}

static void metetabla(Tabla *t)
{
	Lin o = { 0 }, p = { 0 };
	int ny, nx, y;

	hazt(t, &o, 0, 0, 0, &ny, &nx);
	sel = 0;
	while (estab(cy) && cy < nl - 1 && estab(cy + 1))
		cy++;
	cx = 0;
	if (L[cy].n) {
		cx = L[cy].n;
		adds(&p, estab(cy) ? "\n\n" : "\n");
	} else if (cy && estab(cy - 1))
		adds(&p, "\n");
	y = cy + p.n;
	add(&p, o.s, o.n);
	if (cy == nl - 1 || L[cy + 1].n)
		adds(&p, "\n");
	ins(cy, cx, p.s, p.n);
	cy = y + ny;
	cx = nx;
	free(o.s);
	free(p.s);
}

static const char *sinpuntos(const char *s)
{
	static char b[128];
	int n;

	snprintf(b, sizeof b, "%s", s);
	n = strlen(b);
	if (n >= 3 && !strcmp(b + n - 3, "…"))
		b[n - 3] = 0;
	return b;
}

static void nuevatabla(void)
{
	static int k = 3, f = 3, e = 1;
	Tabla t = { 0 };

	if (bloqueado() || !formtabla(&k, &f, &e, sinpuntos(tx(T_INSERTAR))))
		return;
	t.k = k;
	t.e = e;
	while (t.f < f)
		filanueva(&t, t.f);
	metetabla(&t);
	suelta_t(&t);
}

static char *leetodo(const char *p, size_t *n, int *e)
{
	struct stat st;
	char *b = 0;
	ssize_t r = 0;
	size_t c;
	int fd;

	*e = 0;
	*n = 0;
	if ((fd = open(p, O_RDONLY | O_CLOEXEC)) < 0)
		*e = errno;
	else if (fstat(fd, &st))
		*e = errno;
	else if (S_ISDIR(st.st_mode))
		*e = EISDIR;
	else if (!S_ISREG(st.st_mode))
		*e = EINVAL;
	else if (st.st_size > (off_t)1 << 30)
		*e = EFBIG;
	else {
		b = re(0, (c = st.st_size + 1));
		while ((r = read(fd, b + *n, c - *n)) > 0 || (r < 0 && errno == EINTR))
			if (r > 0 && (*n += r) == c) {
				if (c > (size_t)1 << 30) {
					*e = EFBIG;
					break;
				}
				b = re(b, c *= 2);
			}
		if (r < 0)
			*e = errno;
	}
	if (fd >= 0)
		close(fd);
	if (*e) {
		free(b);
		return 0;
	}
	return b;
}

static char *autf8(char *b, size_t *n)
{
	size_t i, j;
	int c, l;
	char *o;

	if (*n >= 3 && !memcmp(b, "\xef\xbb\xbf", 3)) {
		*n -= 3;
		memmove(b, b + 3, *n);
	}
	for (i = 0; i < *n; i += l) {
		l = dec(b + i, *n - i, &c);
		if (c < 0)
			break;
	}
	if (i >= *n)
		return b;
	o = re(0, *n * 2 + 1);
	for (i = j = 0; i < *n; i++)
		j += cod((unsigned char)b[i], o + j);
	free(b);
	*n = j;
	return o;
}

static char separador(const char *b, size_t n)
{
	static const char c[] = ",;\t|";
	int k[4] = { 0 }, i, m = 0, q = 0;
	size_t j;

	for (j = 0; j < n && (q || b[j] != '\n'); j++)
		if (b[j] == '"')
			q = !q;
		else if (!q)
			for (i = 0; i < 4; i++)
				k[i] += b[j] == c[i];
	for (i = 1; i < 4; i++)
		if (k[i] > k[m])
			m = i;
	return c[m];
}

static char *celdacsv(Lin *c, int e)
{
	char *s, *p;
	int i, a = 0, b = c->n;

	for (i = 0; i < c->n; i++)
		if (c->s[i] == '\t' || c->s[i] == '\r' || c->s[i] == '\n')
			c->s[i] = ' ';
	while (a < b && c->s[a] == ' ')
		a++;
	while (b > a && c->s[b - 1] == ' ')
		b--;
	s = strndup(c->s ? c->s + a : "", b - a);
	if (e == 1)
		for (p = s; (p = strchr(p, '|')); )
			*p = '/';
	else
		while ((p = strstr(s, "│"))) {
			*p = '|';
			memmove(p + 1, p + 3, strlen(p + 3) + 1);
		}
	c->n = 0;
	return s;
}

static int leecsv(char *b, size_t n, Tabla *t)
{
	Lin c = { 0 };
	size_t i;
	int col = 0, corte = 0, q = 0, j;
	char d = separador(b, n), ch;

	filanueva(t, 0);
	for (i = 0; i <= n; i++) {
		ch = i < n ? b[i] : '\n';
		if (q) {
			if (ch != '"')
				add(&c, &ch, 1);
			else if (i + 1 < n && b[i + 1] == '"')
				add(&c, b + i++, 1);
			else
				q = 0;
			continue;
		}
		if (ch == '"' && !c.n) {
			q = 1;
			continue;
		}
		if (ch == '\r' && i + 1 < n && b[i + 1] == '\n')
			continue;
		if (ch != d && ch != '\n' && ch != '\r') {
			add(&c, &ch, 1);
			continue;
		}
		if (col < MAXC) {
			reserva(t, t->f, col + 1);
			CE(t, t->f - 1, col) = celdacsv(&c, t->e);
		} else {
			corte = 1;
			c.n = 0;
		}
		col++;
		if (ch == d)
			continue;
		if (col == 1 && !CE(t, t->f - 1, 0)[0]) {
			free(CE(t, t->f - 1, 0));
			CE(t, t->f - 1, 0) = 0;
		} else {
			if (col > t->k)
				t->k = col < MAXC ? col : MAXC;
			if (i < n)
				filanueva(t, t->f);
		}
		col = 0;
	}
	free(c.s);
	for (j = 0; t->f && j < t->kc && !CE(t, t->f - 1, j); j++)
		;
	if (t->f && j == t->kc)
		t->f--;
	return corte;
}

static void importa(void)
{
	char p[PATH_MAX], *b;
	size_t n;
	int e, r, corte;
	Tabla t = { 0 };

	if (bloqueado() || !explora(0, T_T_CSV, p))
		return;
	if (!(b = leetodo(p, &n, &e))) {
		avisa(tx(T_NOABRE), base(p), fallo(e));
		return;
	}
	if (memchr(b, 0, n)) {
		free(b);
		avisa("%s", tx(T_CSV_VACIO));
		return;
	}
	r = elegir(tx(T_CSV_ESTILO), ops(3, T_MARKDOWN, T_CUADRO, T_CANCELAR));
	if (r < 0 || r == 2) {
		free(b);
		return;
	}
	t.e = r + 1;
	b = autf8(b, &n);
	corte = leecsv(b, n, &t);
	free(b);
	if (!t.f || !t.k)
		avisa("%s", tx(T_CSV_VACIO));
	else {
		metetabla(&t);
		if (corte)
			avisa(tx(T_CSV_CORTE), t.f, MAXC);
		else
			avisa(tx(T_CSV_OK), t.f);
	}
	suelta_t(&t);
}

static void ventana(const char *tt, const char **l)
{
	int n, top = 0, h, w, m = 0, y, x, i, k;

	for (n = 0; l[n]; n++)
		if ((i = ancho(l[n])) > m)
			m = i;
	for (;;) {
		pinta();
		w = m + 6 > cols - 4 ? cols - 4 : m + 6;
		h = n + 4 > filas - 4 ? filas - 4 : n + 4;
		if (top > n - (h - 4))
			top = n - (h - 4);
		if (top < 0)
			top = 0;
		y = 3 + (filas - 4 - h) / 2;
		x = (cols - w) / 2;
		caja(y, x, h, w, tt);
		for (i = 0; i < h - 4 && top + i < n; i++)
			texto(y + 2 + i, x + 3, l[top + i], l[top + i][0] == ' ' ? NOR : TIT, w - 6);
		if (top)
			texto(y + 1, x + w - 4, "▲", NUM, 1);
		if (top + h - 4 < n)
			texto(y + h - 2, x + w - 4, "▼", NUM, 1);
		vuelca(-1, -1);
		k = tecla(-1);
		if (k == K_ARR || (k == K_RATON && rb == 64))
			top -= k == K_ARR ? 1 : 3;
		else if (k == K_ABA || (k == K_RATON && rb == 65))
			top += k == K_ABA ? 1 : 3;
		else if (k == K_REP || k == K_AVP)
			top += k == K_REP ? 4 - h : h - 4;
		else if (k == K_RATON && !rs && !(rb & 32) && (ry < y || ry >= y + h || rx < x || rx >= x + w))
			return;
		else if (k != K_RATON && k != K_TAM && k != K_NADA)
			return;
	}
}

static void lineaayuda(char *b, const char *k, const char *d)
{
	int n = ancho(k);

	snprintf(b, 160, "  %s%*s%s", k, n < 20 ? 20 - n : 1, "", d);
}

static const char *etiqueta(const Op *o);
static const char *clave_de(const Op *o);

static void atajos(void)
{
	static const int orden[] = { 6, 0, 1, 2, 3, 4, 5 };
	static char b[120][160];
	const char *l[121];
	char k[64];
	int n = 0, m, i, j, uno;

	for (j = 0; j < NM; j++) {
		m = orden[j];
		for (i = uno = 0; menus[m][i].a >= 0 && n < 100; i++) {
			if (!menus[m][i].t || !menus[m][i].k[0])
				continue;
			if (!uno++) {
				if (n)
					b[n++][0] = 0;
				snprintf(b[n++], 160, "%s", tx(T_M_ARCHIVO + m));
			}
			snprintf(k, sizeof k, "%s", menus[m][i].a == A_ATAJOS ? "Ctrl+H / F1" : clave_de(menus[m] + i));
			lineaayuda(b[n++], k, etiqueta(menus[m] + i));
		}
	}
	b[n++][0] = 0;
	snprintf(b[n++], 160, "%s", tx(T_H_TECLADO));
	lineaayuda(b[n++], tx(T_K_SUP), tx(T_H_SUP));
	snprintf(k, sizeof k, "%s / %s", tx(T_K_INI), tx(T_K_FIN));
	lineaayuda(b[n++], k, tx(T_H_INIFIN));
	snprintf(k, sizeof k, "Alt+%s / %s", tx(T_K_INI), tx(T_K_FIN));
	lineaayuda(b[n++], k, tx(T_H_LINEA));
	lineaayuda(b[n++], "Ctrl+← / →", tx(T_H_PALABRA));
	snprintf(k, sizeof k, "%s / %s", tx(T_K_REP), tx(T_K_AVP));
	lineaayuda(b[n++], k, tx(T_H_PAGINA));
	snprintf(k, sizeof k, "%s+%s", tx(T_MAY), tx(T_K_FLECHAS));
	lineaayuda(b[n++], k, tx(T_H_SELEC));
	snprintf(k, sizeof k, "Ctrl+%s+V", tx(T_MAY));
	lineaayuda(b[n++], k, tx(T_H_PEGAR));
	b[n++][0] = 0;
	snprintf(b[n++], 160, "%s", tx(T_H_TABLAS));
	snprintf(k, sizeof k, "Tab / %s+Tab", tx(T_MAY));
	lineaayuda(b[n++], k, tx(T_H_TAB));
	lineaayuda(b[n++], "Enter", tx(T_H_ENTER));
	b[n++][0] = 0;
	snprintf(b[n++], 160, "%s", tx(T_H_HOJA));
	lineaayuda(b[n++], "Alt+↓", tx(T_H_COLUMNA));
	b[n++][0] = 0;
	snprintf(b[n++], 160, "%s", tx(T_H_MENUS));
	lineaayuda(b[n++], "F10", tx(T_H_F10));
	snprintf(k, sizeof k, "Alt+");
	for (i = 0; i < NM; i++)
		snprintf(k + strlen(k), sizeof k - strlen(k), "%c%s", toupper((unsigned char)idiomas[idi].letras[i]),
			i < NM - 1 ? " " : "");
	lineaayuda(b[n++], k, tx(T_H_ALT));
	snprintf(k, sizeof k, "Tab / %s", tx(T_K_FLECHAS));
	lineaayuda(b[n++], k, tx(T_H_MOVER));
	lineaayuda(b[n++], tx(T_K_RATON), tx(T_H_RATON));
	lineaayuda(b[n++], tx(T_K_DOBLE), tx(T_H_DOBLE));
	snprintf(k, sizeof k, "%s+%s", tx(T_MAY), tx(T_K_ARRASTRAR));
	lineaayuda(b[n++], k, tx(T_H_TERMINAL));
	for (i = 0; i < n; i++)
		l[i] = b[i];
	l[n] = 0;
	ventana(tx(T_ATAJOS), l);
}

static void acercade(void)
{
	const char *l[] = { "isi " VERSION, tx(T_DESC), "", "Copyright (C) 2026 zil00g", "zil00g@protonmail.com",
		tx(T_LICENCIA), "", tx(T_CIFRADO), tx(T_MONO), 0 };

	ventana(tx(T_ACERCA), l);
}

static const char *etiqueta(const Op *o)
{
	return tx(o->a == A_CIFRAR && cif ? T_CAMBIAR : o->t);
}

static const char *clave_de(const Op *o)
{
	static char b[64];
	char k[48];
	int on = o->a == A_NUM ? numeros : o->a == A_RESALTA ? resalta : o->a == A_AUTO ? autoioc : -1;

	if (!strncmp(o->k, "S+", 2))
		snprintf(k, sizeof k, "%s+%s", tx(T_MAY), o->k + 2);
	else
		snprintf(k, sizeof k, "%s", o->k);
	if (on < 0)
		snprintf(b, sizeof b, "%s", k);
	else
		snprintf(b, sizeof b, "%s%s%s", on ? "✓" : " ", k[0] ? " " : "", k);
	return b;
}

static void pintalista(const Op *o, int x, int y, int s, int *w, int *n)
{
	int i, l, a;

	for (*n = 0, *w = 0; o[*n].a >= 0; ++*n)
		if (o[*n].t && (l = ancho(etiqueta(o + *n)) + ancho(clave_de(o + *n)) + 7) > *w)
			*w = l;
	if (x + *w > cols - 1)
		x = cols - 1 - *w;
	caja(y, x, *n + 2, *w, 0);
	for (i = 0; i < *n; i++) {
		if (!o[i].t) {
			raya(y + 1 + i, x, *w);
			continue;
		}
		a = i == s ? INV : NOR;
		if (a == INV)
			rellena(y + 1 + i, x + 1, *w - 2, " ", INV);
		texto(y + 1 + i, x + 2, etiqueta(o + i), a, *w - 4);
		texto(y + 1 + i, x + *w - 2 - ancho(clave_de(o + i)), clave_de(o + i), a == INV ? INV : NUM, *w - 4);
	}
}

static void pintamenu(int m, int s)
{
	mmx = xs[m] + 1;
	pintalista(menus[m], mmx, 3, s, &mmw, &mmn);
	if (mmx + mmw > cols - 1)
		mmx = cols - 1 - mmw;
}

static int salta(int m, int s, int d)
{
	const Op *o = menus[m];
	int n, i;

	for (n = 0; o[n].a >= 0; n++)
		;
	for (i = 0; i < n; i++) {
		s = (s + d + n) % n;
		if (o[s].t)
			return s;
	}
	return 0;
}

static int letramenu(int k)
{
	const char *p, *l = idiomas[idi].letras;

	k &= 0xFFFFFF;
	if (k <= 0 || k >= 128 || !(p = strchr(l, tolower(k))))
		return -1;
	return p - l;
}

static int menu(int m)
{
	int s = 0, k, i, r = 0;

	for (;;) {
		abierto = m;
		pinta();
		pintamenu(m, s);
		vuelca(-1, -1);
		k = tecla(-1);
		if (k == K_ESC || k == K_F10)
			break;
		if (k == 9 || k == K_DER || k == K_STAB || k == K_IZQ) {
			m = (m + (k == 9 || k == K_DER ? 1 : NM - 1)) % NM;
			s = 0;
		} else if (k == K_ARR)
			s = salta(m, s, -1);
		else if (k == K_ABA)
			s = salta(m, s, 1);
		else if (k == 13 || k == ' ') {
			r = menus[m][s].a;
			break;
		} else if ((k & ALT) && letramenu(k) >= 0) {
			m = letramenu(k);
			s = 0;
		} else if (k == K_RATON) {
			if (rs || (rb & ~28) != 0)
				continue;
			if (ry == 1) {
				for (i = 0; i < NM && !(rx > xs[i] && rx < xs[i + 1]); i++)
					;
				if (i == m || i == NM)
					break;
				m = i;
				s = 0;
			} else if (rx > mmx && rx < mmx + mmw - 1 && ry >= 4 && ry < 4 + mmn) {
				if (menus[m][ry - 4].t) {
					s = ry - 4;
					r = menus[m][s].a;
					break;
				}
			} else
				break;
		}
	}
	abierto = -1;
	return r;
}

static int letra(int y, int x)
{
	unsigned char c = L[y].s[x];

	return isalnum(c) || c == '_' || c >= 0x80;
}

static void palabra(int d)
{
	if (d > 0) {
		if (cx >= L[cy].n) {
			if (cy < nl - 1)
				cy++, cx = 0;
			return;
		}
		while (cx < L[cy].n && !letra(cy, cx))
			cx++;
		while (cx < L[cy].n && letra(cy, cx))
			cx++;
	} else {
		if (!cx) {
			if (cy)
				cy--, cx = L[cy].n;
			return;
		}
		while (cx && !letra(cy, cx - 1))
			cx--;
		while (cx && letra(cy, cx - 1))
			cx--;
	}
}

static void vert(int d)
{
	if (px < 0)
		px = colde(cy, cx);
	cy += d;
	if (cy < 0)
		cy = 0;
	if (cy >= nl)
		cy = nl - 1;
	cx = bytede(cy, px);
}

static void mueve(int b, int m)
{
	int w, c, i;

	if (m & MAY) {
		if (!sel) {
			sel = 1;
			ay = cy;
			ax = cx;
		}
	} else
		sel = 0;
	switch (b) {
	case K_IZQ:
		if (m & CTR)
			palabra(-1);
		else if (cx)
			do
				cx--;
			while (cx && (L[cy].s[cx] & 0xC0) == 0x80);
		else if (cy)
			cx = L[--cy].n;
		px = -1;
		break;
	case K_DER:
		if (m & CTR)
			palabra(1);
		else if (cx < L[cy].n) {
			cx = mira(L[cy].s, cx, L[cy].n, 0, &w, &c);
			while (cx < L[cy].n) {
				i = mira(L[cy].s, cx, L[cy].n, 0, &w, &c);
				if (w)
					break;
				cx = i;
			}
		} else if (cy < nl - 1)
			cy++, cx = 0;
		px = -1;
		break;
	case K_ARR: vert(-1); break;
	case K_ABA: vert(1); break;
	case K_REP: vert(-th); break;
	case K_AVP: vert(th); break;
	case K_INI:
		if (m & ALT)
			cx = 0;
		else
			cy = cx = 0;
		px = -1;
		break;
	case K_FIN:
		if (!(m & ALT))
			cy = nl - 1;
		cx = L[cy].n;
		px = -1;
		break;
	}
}

static void posiciona(int r, int c)
{
	int y = fy + r - 3;

	if (r < 3)
		y = fy - 1;
	if (r >= 3 + th)
		y = fy + th;
	if (y < 0)
		y = 0;
	if (y >= nl)
		y = nl - 1;
	c += fx - x0;
	cy = y;
	cx = bytede(y, c < 0 ? 0 : c);
	px = -1;
}

static void celda(void)
{
	int e = estab(cy), sl = e == 1 ? 1 : 3, i, p, a, b;
	char *s = L[cy].s;

	for (i = L[cy].n - sl; i > 0 && !sepen(e, cy, i); i--)
		;
	if (i > 0 && cx > i)
		cx = i;
	for (i = cx < sl ? sl : cx; i > sl && !sepen(e, cy, i - sl); i--)
		;
	for (p = i; p < L[cy].n && !sepen(e, cy, p); p++)
		;
	for (a = i; a < p && s[a] == ' '; a++)
		;
	for (b = p; b > a && s[b - 1] == ' '; b--)
		;
	if (a == b)
		cx = i + 1 < p ? i + 1 : i;
	else if (cx < a)
		cx = a;
	else if (cx > b + 1)
		cx = b;
}

static void inserta(const char *s, int n)
{
	Lin t = { 0 };
	int i, tb = filat(cy);
	char c;

	if (tb)
		celda();
	for (i = 0; i < n; i++) {
		if (s[i] == '\r' && i + 1 < n && s[i + 1] == '\n')
			continue;
		c = s[i] == '\r' ? '\n' : s[i];
		if (tb && (c == '\n' || c == '\t'))
			c = ' ';
		add(&t, &c, 1);
	}
	if (autoioc) {
		Lin o = { 0 };
		cambia(t.s, t.n, &o, 0);
		free(t.s);
		t = o;
	}
	borrasel();
	ins(cy, cx, t.s, t.n);
	free(t.s);
	if (tb && filat(cy))
		tfix();
}

static void raton(void)
{
	int b = rb & ~28, i, d;

	if (hoja && ry >= 3 && ry < 3 + th) {
		ratonhoja();
		return;
	}
	if (b == 64 || b == 65) {
		if (b == 64)
			fy -= 3;
		else
			fy += 3;
		if (fy > nl - th)
			fy = nl - th;
		if (fy < 0)
			fy = 0;
		if (px < 0)
			px = colde(cy, cx);
		if (cy < fy)
			cy = fy;
		if (cy >= fy + th)
			cy = fy + th - 1;
		cx = bytede(cy, px);
		return;
	}
	if (rs) {
		arr = 0;
		return;
	}
	if (b == 32 && arr) {
		posiciona(ry, rx);
		sel = cy != ay || cx != ax;
		return;
	}
	if (b)
		return;
	d = doble_clic();
	if (ry == 1) {
		for (i = 0; i < NM && !(rx > xs[i] && rx < xs[i + 1]); i++)
			;
		if (i < NM)
			haz(menu(i));
		return;
	}
	if (barra && ry == filas - 2) {
		if (d)
			aviso[0] = 0;
		return;
	}
	if (ry >= 3 && ry < 3 + th) {
		posiciona(ry, rx);
		if (d) {
			for (ax = cx; ax && letra(cy, ax - 1); ax--)
				;
			while (cx < L[cy].n && letra(cy, cx))
				cx++;
			ay = cy;
			sel = ax != cx;
			arr = 0;
		} else {
			sel = 0;
			ay = cy;
			ax = cx;
			arr = 1;
		}
	}
}

static int igual(const char *a, const char *b, int n)
{
	while (n--)
		if (tolower((unsigned char)a[n]) != tolower((unsigned char)b[n]))
			return 0;
	return 1;
}

static int busca(int d, int vuelta)
{
	int n = strlen(qb), y = cy, x = cx, i, j, y1, x1, y2, x2;

	if (!n)
		return 0;
	if (rango(&y1, &x1, &y2, &x2)) {
		y = d > 0 ? y2 : y1;
		x = d > 0 ? x2 : x1;
	}
	for (i = 0; i <= nl; i++) {
		if (d > 0) {
			for (j = i ? 0 : x; j + n <= L[y].n; j++)
				if (igual(L[y].s + j, qb, n))
					goto hit;
			if (++y == nl) {
				if (!vuelta)
					return 0;
				y = 0;
			}
		} else {
			for (j = (i ? L[y].n : x) - 1; j >= 0; j--)
				if (j + n <= L[y].n && igual(L[y].s + j, qb, n))
					goto hit;
			if (--y < 0)
				y = nl - 1;
		}
	}
	return 0;
hit:
	sel = 1;
	ay = cy = y;
	ax = j;
	cx = j + n;
	px = -1;
	return 1;
}

static void reemplaza(void)
{
	int r, todos = 0, c = 0, n;

	if (!pedir(tx(T_BUSCARP), qb, sizeof qb, 0) || !qb[0] || !pedir(tx(T_REEMPLP), rp, sizeof rp, 0))
		return;
	n = strlen(rp);
	sel = cy = cx = 0;
	while (busca(1, 0)) {
		if (!todos) {
			medidas();
			ajusta();
			r = elegir(tx(T_REEMPLQ), ops(4, T_SI, T_NO, T_TODAS, T_CANCELAR));
			if (r < 0 || r == 3)
				break;
			if (r == 1)
				continue;
			todos = r == 2;
		}
		borrasel();
		ins(cy, cx, rp, n);
		c++;
		if (solo)
			break;
	}
	sel = 0;
	avisa(tx(T_REEMPLN), c);
}

static void vacias(void)
{
	int y1 = 0, x1, y2 = nl - 1, x2, i, j, r = 0, k = 0;
	Lin o = { 0 };

	if (rango(&y1, &x1, &y2, &x2) && x2 == 0 && y2 > y1)
		y2--;
	else if (y2 == nl - 1 && !L[y2].n && y2 > y1)
		y2--;
	for (i = y1; i <= y2; i++) {
		for (j = 0; j < L[i].n && (L[i].s[j] == ' ' || L[i].s[j] == '\t' || L[i].s[j] == '\r'); j++)
			;
		if (j == L[i].n) {
			r++;
			continue;
		}
		if (k++)
			add(&o, "\n", 1);
		add(&o, L[i].s, L[i].n);
	}
	if (!r)
		avisa("%s", tx(T_NOVACIAS));
	else if (!bloqueado()) {
		del(y1, 0, y2, L[y2].n);
		ins(y1, 0, o.s, o.n);
		cy = y1;
		cx = 0;
		sel = 0;
		avisa(tx(T_VACIASN), r);
	}
	free(o.s);
}

static void osc52(const char *s, int n)
{
	static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	Lin o = { 0 };
	unsigned v;
	int i, j;
	char q[4];

	adds(&o, getenv("TMUX") ? "\033Ptmux;\033\033]52;c;" : "\033]52;c;");
	for (i = 0; i < n; i += 3) {
		v = (unsigned char)s[i] << 16;
		if (i + 1 < n)
			v |= (unsigned char)s[i + 1] << 8;
		if (i + 2 < n)
			v |= (unsigned char)s[i + 2];
		for (j = 0; j < 4; j++)
			q[j] = i + j - 1 < n ? b64[v >> (18 - 6 * j) & 63] : '=';
		add(&o, q, 4);
	}
	adds(&o, getenv("TMUX") ? "\a\033\\" : "\a");
	salida(o.s, o.n);
	free(o.s);
}

static void portapapeles(char *s, int n)
{
	const char *c = getenv("WAYLAND_DISPLAY") ? "wl-copy >/dev/null 2>&1" :
		getenv("DISPLAY") ? "(xclip -selection clipboard || xsel -ib) >/dev/null 2>&1" : 0;
	FILE *p;

	free(clip.s);
	clip.s = s;
	clip.n = clip.c = n;
	if (n <= 1 << 20)
		osc52(s, n);
	if (c && (p = popen(c, "w"))) {
		fwrite(s, 1, n, p);
		pclose(p);
	}
}

static int copia(void)
{
	int y1, x1, y2, x2, n;
	char *s;

	if (!rango(&y1, &x1, &y2, &x2)) {
		avisa("%s", tx(T_NOSEL));
		return 0;
	}
	s = trozo(y1, x1, y2, x2, &n);
	portapapeles(s, n);
	return 1;
}

static void lineasde(int *y1, int *y2)
{
	int x1, x2;

	*y1 = 0;
	*y2 = nl - 1;
	if (rango(y1, &x1, y2, &x2) && !x2 && *y2 > *y1)
		--*y2;
}

static void iocs(int activar)
{
	int y1, y2, y, c = 0, k, sy = cy, sx = cx;
	Lin o = { 0 };

	if (bloqueado())
		return;
	lineasde(&y1, &y2);
	for (y = y1; y <= y2; y++)
		if ((k = cambia(L[y].s, L[y].n, &o, activar))) {
			c += k;
			del(y, 0, y, L[y].n);
			ins(y, 0, o.s, o.n);
		}
	free(o.s);
	cy = sy;
	cx = sx;
	sel = 0;
	if (c)
		avisa(tx(activar ? T_IOC_REACT : T_IOC_DESACT), c);
	else
		avisa("%s", tx(T_IOC_NADA));
}

static void listaiocs(void)
{
	Lin o = { 0 }, u = { 0 }, k = { 0 };
	int y1, y2, y, i, j, c = 0;

	lineasde(&y1, &y2);
	add(&o, "\n", 1);
	for (y = y1; y <= y2; y++)
		for (i = 0; i < L[y].n; i++) {
			if (!ioc(L[y].s, i, L[y].n, &j))
				continue;
			cambia(L[y].s + i, j - i, &u, 0);
			k.n = 0;
			add(&k, "\n", 1);
			add(&k, u.s, u.n);
			add(&k, "\n", 1);
			if (!memmem(o.s, o.n, k.s, k.n)) {
				add(&o, u.s, u.n);
				add(&o, "\n", 1);
				c++;
			}
			i = j - 1;
		}
	free(u.s);
	free(k.s);
	if (!c) {
		free(o.s);
		avisa("%s", tx(T_IOC_NADA));
		return;
	}
	o.n -= 2;
	memmove(o.s, o.s + 1, o.n);
	portapapeles(o.s, o.n);
	avisa(tx(T_IOC_COPIA), c);
}

static void autodesactiva(void)
{
	Lin o = { 0 };
	int a = cx, e = cx;

	if (!autoioc || solo)
		return;
	while (a > 0 && !strchr(" \t\"'<>`()", L[cy].s[a - 1]))
		a--;
	if (cambia(L[cy].s + a, e - a, &o, 0)) {
		del(cy, a, cy, e);
		ins(cy, a, o.s, o.n);
	}
	free(o.s);
}

static void pega(void)
{
	Lin t = { 0 };
	const char *c = getenv("WAYLAND_DISPLAY") ? "wl-paste -n </dev/null 2>/dev/null" :
		getenv("DISPLAY") ? "(xclip -selection clipboard -o || xsel -ob) </dev/null 2>/dev/null" : 0;
	FILE *p;
	char b[4096];
	size_t r;

	if (c && (p = popen(c, "r"))) {
		while ((r = fread(b, 1, sizeof b, p)) > 0)
			add(&t, b, r);
		if (pclose(p))
			t.n = 0;
	}
	if (!t.n)
		add(&t, clip.s, clip.n);
	if (t.n && hoja)
		pegahoja(t.s, t.n);
	else if (t.n)
		inserta(t.s, t.n);
	else
		avisa(tx(T_PEGAFUERA), tx(T_MAY));
	free(t.s);
}

typedef struct { int y1, x1, y2, x2, q; } Campo;
typedef struct { int r, t; double v; char *k; } Clave;

static int *hreg, hn, hcap, *hw, hk, hwc, *hvis, hnv, hfr, hfc, hcy, hcx, hf, hc, ho, hsy = -1, hsx;
static Campo *hcp;
static int hnc, hcc, orden;
static Lin hv, hv2;

static int partir(int r)
{
	int y = hreg[r], fin = hreg[r + 1], x = 0, q;
	char *p;
	Campo c;

	hnc = 0;
	for (;;) {
		c.y1 = y;
		c.x1 = x;
		c.q = q = x < L[y].n && L[y].s[x] == '"';
		x += q;
		for (;;) {
			if (x >= L[y].n) {
				if (q && y + 1 < fin) {
					y++;
					x = 0;
					continue;
				}
				break;
			}
			p = memchr(L[y].s + x, q ? '"' : hsep, L[y].n - x);
			x = p ? p - L[y].s : L[y].n;
			if (!q)
				break;
			if (!p)
				continue;
			if (x + 1 < L[y].n && L[y].s[x + 1] == '"')
				x += 2;
			else {
				q = 0;
				x++;
			}
		}
		c.y2 = y;
		c.x2 = x;
		if (hnc == hcc)
			hcp = re(hcp, (hcc = hcc * 2 + 16) * sizeof *hcp);
		hcp[hnc++] = c;
		if (x >= L[y].n)
			break;
		x++;
	}
	return hnc;
}

static int recorre(int j, Lin *o, int hy, int hx, int ho2, int *ry, int *rx)
{
	Campo *c = hcp + j;
	int y = c->y1, x = c->x1 + c->q, q = c->q, n = 0;

	if (o)
		o->n = 0;
	for (;;) {
		if (y == hy && x == hx)
			return n;
		if (n == ho2 && ry) {
			*ry = y;
			*rx = x;
			return n;
		}
		if (y > c->y2 || (y == c->y2 && x >= c->x2))
			break;
		if (x >= L[y].n) {
			if (o)
				add(o, "\n", 1);
			y++;
			x = 0;
			n++;
			continue;
		}
		if (q && L[y].s[x] == '"') {
			if (x + 1 < L[y].n && L[y].s[x + 1] == '"') {
				if (o)
					add(o, "\"", 1);
				x += 2;
				n++;
			} else {
				q = 0;
				x++;
			}
			continue;
		}
		if (o)
			add(o, L[y].s + x, 1);
		x++;
		n++;
	}
	if (ry) {
		*ry = c->y2;
		*rx = c->x2 - (c->q && c->x2 > c->x1 + 1 && L[c->y2].s[c->x2 - 1] == '"');
	}
	return n;
}

static const char *valor(int j, Lin *o)
{
	if (j >= hnc) {
		o->n = 0;
		crece(o, 1);
	} else
		recorre(j, o, -1, -1, -1, 0, 0);
	crece(o, o->n + 1);
	o->s[o->n] = 0;
	return o->s;
}

static int regv(int i)
{
	return hvis ? hvis[i] : i + 1;
}

static int cota(int r)
{
	int a = 0, b = hnv, m;

	if (!hvis)
		return r < 1 ? 0 : r - 1 > hnv ? hnv : r - 1;
	while (a < b) {
		m = (a + b) / 2;
		if (hvis[m] < r)
			a = m + 1;
		else
			b = m;
	}
	return a;
}

static int regde(int y)
{
	int a = 0, b = hn - 1, m;

	while (a < b) {
		m = (a + b + 1) / 2;
		if (hreg[m] <= y)
			a = m;
		else
			b = m - 1;
	}
	return a;
}

static int anchocampo(int j)
{
	Campo *c = hcp + j;
	const unsigned char *s;
	int w, n;

	if (c->y1 != c->y2)
		w = ancho(valor(j, &hv2));
	else {
		s = (const unsigned char *)L[c->y1].s + c->x1 + c->q;
		n = c->x2 - c->x1 - 2 * c->q;
		for (w = 0; w < n && w <= 40 && s[w] >= 32 && s[w] < 127; w++)
			;
		if (w < n && w <= 40)
			w = anchon((const char *)s, n);
	}
	return w < 1 ? 1 : w > 40 ? 40 : w;
}

static void filtra(int deja);
static void filtrar(void);
static int activo(int j);
static int filtros(void);
static int visible(int r);
static void desplaza_f(int a);
static void quitafiltros(void);
static void miles(long n, char *b);

static void rehace(void)
{
	int y, x, q, n = nl, a, r, j, w;
	char *p;

	if (n > 1 && !L[n - 1].n)
		n--;
	hn = 0;
	for (y = 0; y < n; y++) {
		if (hn + 2 > hcap)
			hreg = re(hreg, (hcap = hn * 3 / 2 + 64) * sizeof *hreg);
		hreg[hn++] = y;
		for (q = 0, a = y;; y++) {
			for (x = 0; x < L[y].n; x++) {
				if (!(p = memchr(L[y].s + x, '"', L[y].n - x)))
					break;
				x = p - L[y].s;
				if (q && x + 1 < L[y].n && L[y].s[x + 1] == '"')
					x++;
				else if (q)
					q = 0;
				else if (y == a ? !x || L[y].s[x - 1] == hsep : x && L[y].s[x - 1] == hsep)
					q = 1;
			}
			if (!q || y - a >= 200 || y + 1 >= n)
				break;
		}
	}
	if (!hn) {
		hreg = re(hreg, (hcap = 64) * sizeof *hreg);
		hreg[hn++] = 0;
		n = 1;
	}
	hreg[hn] = n;
	hk = 0;
	for (r = 0; r < hn; r++) {
		partir(r);
		if (hnc > hwc)
			hw = re(hw, (hwc = hnc * 2 + 8) * sizeof *hw);
		while (hk < hnc)
			hw[hk++] = 3;
		for (j = 0; j < hnc; j++)
			if ((w = anchocampo(j)) > hw[j])
				hw[j] = w;
	}
	if (hf >= hn)
		hf = hn - 1;
	filtra(hf);
}

static void ponraw(void)
{
	if (hf >= hn)
		hf = hn - 1;
	if (hf < 0)
		hf = 0;
	partir(hf);
	if (hc < hnc)
		recorre(hc, 0, -1, -1, ho, &cy, &cx);
	else {
		cy = hreg[hf + 1] - 1;
		cx = L[cy].n;
	}
	hsy = cy;
	hsx = cx;
	sel = 0;
	px = -1;
}

static void desderaw(void)
{
	int j;

	if (cy >= hreg[hn]) {
		cy = hreg[hn] - 1;
		cx = L[cy].n;
	}
	hf = regde(cy);
	partir(hf);
	for (j = 0; j < hnc - 1 && (cy > hcp[j].y2 || (cy == hcp[j].y2 && cx > hcp[j].x2)); j++)
		;
	hc = j;
	ho = cy == hcp[j].y1 && cx <= hcp[j].x1 + hcp[j].q ? 0 : recorre(j, 0, cy, cx, -1, 0, 0);
	hsy = cy;
	hsx = cx;
	sel = 0;
}

static void sincro(void)
{
	if (hoja && (cy != hsy || cx != hsx))
		desderaw();
}

static int sigue_v(int r, int d)
{
	int i;

	if (d > 0) {
		i = cota(r + 1);
		return i < hnv ? regv(i) : r;
	}
	i = cota(r) - 1;
	return i >= 0 ? regv(i) : 0;
}

static int largo(void)
{
	partir(hf);
	return (int)strlen(valor(hc, &hv));
}

static void pintaval(int y, int x, int w, const char *s, int a, int salto)
{
	int i = 0, j, n = strlen(s), col = 0, ww, c, ri = 0, rf = -1, k, at;

	while (i < n) {
		j = mira(s, i, n, col, &ww, &c);
		if (resalta && a == NOR && i >= rf) {
			for (rf = ri = i; rf < n && !ioc(s, rf, n, &k); rf++)
				;
			ri = rf;
			if (rf < n)
				rf = k;
		}
		at = a == NOR && resalta && i >= ri && i < rf ? IOC : a;
		if (col >= salto) {
			if (col - salto + ww > w) {
				pon(y, x + w - 1, "…", 3, 1, a == INV ? INV : NUM);
				return;
			}
			glifo(y, x + col - salto, c, s + i, j - i, ww, at);
		}
		col += ww;
		i = j;
	}
}

static int colx(int j)
{
	int x = numeros ? gw + 2 : 1, i;

	for (i = hfc; i < j; i++)
		x += hw[i] + 3;
	return x;
}

static void pintahoja(void)
{
	int f, i, r, j, x, w, a, fin = cols - 1, sal, cur;
	char nb[16];

	hcy = -1;
	for (f = 0; f < th; f++) {
		i = 3 + f;
		if (f == 1) {
			x = colx(hfc);
			pon(i, x - 1, "├", 3, 1, BOR);
			rellena(i, x, fin - x, "─", BOR);
			for (j = hfc; j < hk && (x += hw[j] + 2) < fin; j++, x++)
				pon(i, x, "┼", 3, 1, BOR);
			pon(i, fin, "┤", 3, 1, BOR);
			continue;
		}
		r = !f ? 0 : hfr + f - 2 < hnv ? regv(hfr + f - 2) : -1;
		if (r < 0)
			continue;
		if (numeros && f) {
			j = snprintf(nb, sizeof nb, "%d", r);
			texto(i, 1 + (gw - j + 1) / 2, nb, r == hf ? NUMA : NUM, gw);
		}
		partir(r);
		for (j = hfc, x = colx(hfc); j < hk && x < fin; j++) {
			w = hw[j];
			cur = r == hf && j == hc;
			a = cur ? INV : f ? NOR : TIT;
			rellena(i, x, w + 2 < fin - x ? w + 2 : fin - x, " ", a);
			valor(j, &hv);
			sal = 0;
			if (cur) {
				sal = anchon(hv.s, ho) - (w - 1);
				if (sal < 0)
					sal = 0;
				hcy = i;
				hcx = x + 1 + anchon(hv.s, ho) - sal;
			}
			pintaval(i, x + 1, w < fin - x - 1 ? w : fin - x - 1, hv.s, a, sal);
			if (!f && x + w + 1 < fin)
				pon(i, x + w + 1, activo(j) ? "▼" : "▾", 3, 1, cur ? INV : activo(j) ? IOC : NUM);
			x += w + 2;
			if (x < fin)
				pon(i, x, "│", 3, 1, BOR);
			x++;
		}
	}
	if (hcy < 0) {
		hcy = 3;
		hcx = colx(hfc) + 1;
	}
	if (hcx > cols - 2)
		hcx = cols - 2;
}

static void pistahoja(void)
{
	char b[8], t, f[160] = "", m1[32], m2[32];
	int i = 0, c = hc + 1, j;

	pista[0] = 0;
	if (!hoja)
		return;
	sincro();
	if (filtros()) {
		miles(hnv, m1);
		miles(hn - 1, m2);
		snprintf(f, sizeof f, tx(T_FILTRADAS), m1, m2);
		snprintf(pista, sizeof pista, "%s", f);
	}
	partir(hf);
	valor(hc, &hv);
	if (ancho(hv.s) <= hw[hc < hk ? hc : 0] && !strchr(hv.s, '\n'))
		return;
	while (c > 0 && i < 6) {
		c--;
		b[i++] = 'A' + c % 26;
		c /= 26;
	}
	b[i] = 0;
	for (j = 0, i--; j < i; j++, i--) {
		t = b[j];
		b[j] = b[i];
		b[i] = t;
	}
	snprintf(pista, sizeof pista, "%s%s%s%d · %s", f, f[0] ? " · " : "", b, hf, hv.s);
}

static void ajustahoja(void)
{
	int i, n = th - 2, x;

	sincro();
	if (n < 1)
		n = 1;
	if (hc >= hk)
		hc = hk - 1;
	if (hf > 0) {
		i = cota(hf);
		if (i < hfr)
			hfr = i;
		if (i >= hfr + n)
			hfr = i - n + 1;
	}
	if (hfr > hnv - n)
		hfr = hnv - n;
	if (hfr < 0)
		hfr = 0;
	if (hc < hfc)
		hfc = hc;
	for (;;) {
		for (x = colx(hfc), i = hfc; i <= hc; i++)
			x += hw[i] + 3;
		if (x <= cols - 1 || hfc >= hc)
			break;
		hfc++;
	}
}

static void codifica(const char *s, int n, int q, Lin *o)
{
	int i;

	o->n = 0;
	for (i = 0; i < n && !q; i++)
		q = s[i] == hsep || s[i] == '"' || s[i] == '\n' || s[i] == '\r';
	if (q)
		add(o, "\"", 1);
	for (i = 0; i < n; i++) {
		add(o, s + i, 1);
		if (q && s[i] == '"')
			add(o, "\"", 1);
	}
	if (q)
		add(o, "\"", 1);
}

static void editacelda(const char *t, int n, int borra)
{
	Lin e = { 0 }, s = { 0 };
	int y, x, i, j, o, q, w;

	if (bloqueado())
		return;
	partir(hf);
	if (hc >= hnc) {
		y = hreg[hf + 1] - 1;
		for (i = hnc; i <= hc; i++)
			add(&s, &hsep, 1);
		ins(y, L[y].n, s.s, s.n);
		partir(hf);
	}
	valor(hc, &hv);
	o = ho > hv.n ? hv.n : ho;
	i = borra == 2 ? 0 : o;
	j = borra == 2 ? hv.n : o;
	if (borra < 0 && o) {
		do
			i--;
		while (i && (hv.s[i] & 0xC0) == 0x80);
	}
	if (borra > 0 && o < hv.n) {
		do
			j++;
		while (j < hv.n && (hv.s[j] & 0xC0) == 0x80);
	}
	s.n = 0;
	add(&s, hv.s, i);
	add(&s, t, n);
	add(&s, hv.s + j, hv.n - j);
	q = hcp[hc].q;
	codifica(s.s, s.n, q, &e);
	if (e.n != hcp[hc].x2 - hcp[hc].x1 || hcp[hc].y1 != hcp[hc].y2 ||
		memcmp(e.s, L[hcp[hc].y1].s + hcp[hc].x1, e.n)) {
		y = hcp[hc].y1;
		x = hcp[hc].x1;
		del(y, x, hcp[hc].y2, hcp[hc].x2);
		ins(y, x, e.s, e.n);
	}
	ho = i + n;
	crece(&s, s.n + 1);
	s.s[s.n] = 0;
	if (hc < hk && (w = ancho(s.s)) > hw[hc])
		hw[hc] = w > 40 ? 40 : w;
	free(e.s);
	free(s.s);
	ponraw();
}

static void rawreg(int r, Lin *o)
{
	int y;

	for (y = hreg[r]; y < hreg[r + 1]; y++) {
		if (y > hreg[r])
			add(o, "\n", 1);
		add(o, L[y].s, L[y].n);
	}
}

static void sustituye(int r1, int r2, Lin *o)
{
	int y2 = hreg[r2 + 1] - 1;

	del(hreg[r1], 0, y2, L[y2].n);
	ins(hreg[r1], 0, o->s, o->n);
}

static int simbolo(const char *p, int pct)
{
	if (*p == ' ' || *p == '$' || (pct && *p == '%'))
		return 1;
	if (!strncmp(p, "€", 3))
		return 3;
	return strncmp(p, "£", 2) ? 0 : 2;
}

static int numero(const char *s, double *v)
{
	const char *p, *a, *ud = 0, *uc = 0, *q;
	double r = 0, d = 1;
	int neg = 0, dig = 0, coma = idiomas[idi].coma, k;
	char sd;

	for (p = s; (k = simbolo(p, 0)); p += k)
		;
	if (*p == '-' || *p == '+')
		neg = *p++ == '-';
	for (a = p; isdigit((unsigned char)*p) || *p == '.' || *p == ','; p++) {
		dig += isdigit((unsigned char)*p) != 0;
		if (*p == '.')
			ud = p;
		if (*p == ',')
			uc = p;
	}
	if (!dig)
		return 0;
	for (q = p; (k = simbolo(q, 1)); q += k)
		;
	if (*q)
		return 0;
	if (ud && uc)
		sd = ud > uc ? '.' : ',';
	else if (ud || uc) {
		q = ud ? ud : uc;
		for (k = 0; isdigit((unsigned char)q[k + 1]); k++)
			;
		sd = *q == (coma ? ',' : '.') || k != 3 || q + k + 1 != p ? *q : 0;
	} else
		sd = 0;
	for (q = a; q < p; q++) {
		if (*q == sd) {
			d = 0.1;
			continue;
		}
		if (!isdigit((unsigned char)*q))
			continue;
		if (d < 1) {
			r += (*q - '0') * d;
			d /= 10;
		} else
			r = r * 10 + *q - '0';
	}
	*v = neg ? -r : r;
	return 1;
}

static int esip(const char *s, double *v)
{
	unsigned a[4];
	int n = 0;

	if (!isdigit((unsigned char)*s) || s[strspn(s, "0123456789.")])
		return 0;
	if (sscanf(s, "%3u.%3u.%3u.%3u%n", a, a + 1, a + 2, a + 3, &n) != 4 || s[n] ||
		a[0] > 255 || a[1] > 255 || a[2] > 255 || a[3] > 255)
		return 0;
	*v = ((a[0] * 256.0 + a[1]) * 256 + a[2]) * 256 + a[3];
	return 1;
}

static int esfecha(const char *s, double *v)
{
	int d, m, a, h = 0, mi = 0, se = 0, n = 0, t, iso;
	double fr = 0, p = 0.1;

	if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]))
		return 0;
	iso = sscanf(s, "%4d-%2d-%2d%n", &a, &m, &d, &n) == 3 && n == 10;
	if (!iso && sscanf(s, "%2d/%2d/%4d%n", &d, &m, &a, &n) != 3)
		return 0;
	if ((s[n] == ' ' || (iso && s[n] == 'T')) && sscanf(s + n + 1, "%2d:%2d%n", &h, &mi, &t) == 2) {
		n += 1 + t;
		if (s[n] == ':' && sscanf(s + n + 1, "%2d%n", &se, &t) == 1)
			n += 1 + t;
		if ((s[n] == '.' || s[n] == ',') && isdigit((unsigned char)s[n + 1]))
			for (n++; isdigit((unsigned char)s[n]); n++, p /= 10)
				fr += (s[n] - '0') * p;
	}
	if (s[n] == 'Z')
		n++;
	else if (!strcmp(s + n, " UTC"))
		n += 4;
	if (s[n] || a < 100)
		return 0;
	if (!iso && (idi == 1 || m > 12)) {
		t = d;
		d = m;
		m = t;
	}
	if (m < 1 || m > 12 || d < 1 || d > 31 || h > 24 || mi > 59 || se > 60)
		return 0;
	*v = (((((a * 100.0 + m) * 100 + d) * 100 + h) * 100 + mi) * 100 + se) + fr;
	return 1;
}

static char *clavetexto(const char *s)
{
	Lin o = { 0 };
	int i = 0, n = strlen(s), c, k;
	char b;

	while (i < n) {
		k = dec(s + i, n - i, &c);
		if (c == 0xF1 || c == 0xD1)
			add(&o, "n\177", 2);
		else if (c < 128 || (c >= 0xC0 && c <= 0xFF && c != 0xD7 && c != 0xF7)) {
			b = pliega(c);
			add(&o, &b, 1);
		} else
			add(&o, s + i, k);
		i += k;
	}
	add(&o, "", 1);
	crece(&o, 1);
	o.s[o.n - 1] = 0;
	return o.s;
}

static int compara(const void *a, const void *b)
{
	const Clave *x = a, *y = b;
	int r;

	if (x->t == 4 || y->t == 4)
		r = (x->t == 4) - (y->t == 4);
	else if (x->t != y->t)
		r = (x->t - y->t) * orden;
	else if (x->t == 3)
		r = strcmp(x->k, y->k) * orden;
	else
		r = ((x->v > y->v) - (x->v < y->v)) * orden;
	return r ? r : x->r - y->r;
}

static void ordena(int d)
{
	Clave *k;
	Lin o = { 0 };
	int r, n = hn - 1, i;
	const char *s;

	if (bloqueado())
		return;
	if (n < 2) {
		avisa("%s", tx(T_POCASFILAS));
		return;
	}
	k = re(0, n * sizeof *k);
	for (r = 1; r < hn; r++) {
		partir(r);
		s = valor(hc, &hv);
		while (*s == ' ')
			s++;
		k[r - 1].r = r;
		k[r - 1].k = 0;
		if (!*s)
			k[r - 1].t = 4;
		else if (esip(s, &k[r - 1].v))
			k[r - 1].t = 2;
		else if (esfecha(s, &k[r - 1].v) || numero(s, &k[r - 1].v))
			k[r - 1].t = 1;
		else {
			k[r - 1].t = 3;
			k[r - 1].k = clavetexto(s);
		}
	}
	orden = d;
	qsort(k, n, sizeof *k, compara);
	for (i = 0; i < n; i++) {
		if (i)
			add(&o, "\n", 1);
		rawreg(k[i].r, &o);
		free(k[i].k);
	}
	free(k);
	partir(0);
	valor(hc, &hv2);
	sustituye(1, hn - 1, &o);
	free(o.s);
	hf = 0;
	ho = 0;
	rehace();
	ponraw();
	avisa(tx(d > 0 ? T_ORDENADAZ : T_ORDENADZA), hv2.s);
}

static void filahoja(int a)
{
	Lin o = { 0 };
	int y = hreg[hf + 1] - 1, i;

	if (bloqueado())
		return;
	if (a == A_QFILA || a == A_BORRALIN) {
		if (hn < 2) {
			avisa("%s", tx(T_UNAFILA));
			return;
		}
		if (hf + 1 < hn || hreg[hn] < nl)
			del(hreg[hf], 0, hreg[hf + 1], 0);
		else
			del(hreg[hf] - 1, L[hreg[hf] - 1].n, y, L[y].n);
		rehace();
		if (hf >= hn)
			hf = hn - 1;
	} else {
		add(&o, "\n", 1);
		if (a == A_DUP)
			rawreg(hf, &o);
		else
			for (i = 1; i < hk; i++)
				add(&o, &hsep, 1);
		ins(y, L[y].n, o.s, o.n);
		free(o.s);
		hf++;
		rehace();
	}
	ho = 0;
	ponraw();
}

static void rawcampo(int j, Lin *o)
{
	Campo *k = hcp + j;
	int y, a, b;

	for (y = k->y1; y <= k->y2; y++) {
		if (y > k->y1)
			add(o, "\n", 1);
		a = y == k->y1 ? k->x1 : 0;
		b = y == k->y2 ? k->x2 : L[y].n;
		add(o, L[y].s + a, b - a);
	}
}

static void columnas(int a)
{
	Lin o = { 0 };
	int r, j, n, p;

	if (bloqueado())
		return;
	if (a == A_QCOL && hk < 2) {
		avisa("%s", tx(T_UNACOL));
		return;
	}
	for (r = 0; r < hn; r++) {
		if (r)
			add(&o, "\n", 1);
		partir(r);
		n = a == A_COL && hnc < hc + 1 ? hc + 1 : hnc;
		for (j = p = 0; j < n; j++) {
			if (a == A_QCOL && j == hc)
				continue;
			if (p++)
				add(&o, &hsep, 1);
			if (j < hnc)
				rawcampo(j, &o);
			if (a == A_COL && j == hc)
				add(&o, &hsep, 1);
		}
	}
	sustituye(0, hn - 1, &o);
	free(o.s);
	desplaza_f(a);
	if (a == A_COL)
		hc++;
	else if (hc && hc >= hk - 1)
		hc--;
	ho = 0;
	rehace();
	ponraw();
}

static int byteval(const char *s, int want)
{
	int i = 0, j, n = strlen(s), w, c, col = 0;

	while (i < n) {
		j = mira(s, i, n, col, &w, &c);
		if (col + w > want)
			break;
		col += w;
		i = j;
	}
	return i;
}

static void ratonhoja(void)
{
	int b = rb & ~28, f = ry - 3, j, x, r, n;

	if (b == 64 || b == 65) {
		for (j = 0; j < 3; j++)
			hf = sigue_v(hf, b == 64 ? -1 : 1);
		n = largo();
		if (ho > n)
			ho = n;
		ponraw();
		return;
	}
	if (rs || b || f < 0 || f >= th || f == 1)
		return;
	r = !f ? 0 : hfr + f - 2 < hnv ? regv(hfr + f - 2) : -1;
	if (r < 0)
		return;
	for (j = hfc, x = colx(hfc); j < hk && rx >= x + hw[j] + 3; j++)
		x += hw[j] + 3;
	if (j >= hk)
		return;
	hf = r;
	hc = j;
	partir(hf);
	valor(hc, &hv);
	ho = byteval(hv.s, rx - x - 1 < 0 ? 0 : rx - x - 1);
	ponraw();
	if (!f && rx == x + hw[j] + 1)
		filtrar();
}

static void pegahoja(const char *s, int n)
{
	Lin t = { 0 };
	int i;
	char c;

	while (n && (s[n - 1] == '\n' || s[n - 1] == '\r'))
		n--;
	for (i = 0; i < n; i++) {
		if (s[i] == '\r' && i + 1 < n && s[i + 1] == '\n')
			continue;
		c = s[i] == '\r' || s[i] == '\n' ? ' ' : s[i];
		add(&t, &c, 1);
	}
	editacelda(t.s ? t.s : "", t.n, 0);
	free(t.s);
}

static void teclahoja(int k)
{
	int b = k & 0xFFFFFF, m = k & ~0xFFFFFF, n, r;
	char s[8];

	sincro();
	switch (k) {
	case K_ESC: aviso[0] = 0; return;
	case K_F10: haz(menu(0)); return;
	case K_F1: haz(A_ATAJOS); return;
	case K_F3: haz(A_SIG); return;
	case K_F3 | MAY: haz(A_ANT); return;
	}
	if (k < 32 && atajo[k]) {
		haz(atajo[k]);
		return;
	}
	if (b == K_ABA && (m & ALT)) {
		filtrar();
		return;
	}
	n = largo();
	switch (b) {
	case K_ARR:
	case K_ABA:
		hf = sigue_v(hf, b == K_ABA ? 1 : -1);
		break;
	case K_REP:
	case K_AVP:
		r = (hf ? cota(hf) : -1) + (b == K_AVP ? th - 3 : 3 - th);
		if (r >= hnv)
			r = hnv - 1;
		hf = r < 0 ? 0 : regv(r);
		break;
	case K_IZQ:
		if (!(m & CTR) && ho > 0) {
			do
				ho--;
			while (ho && (hv.s[ho] & 0xC0) == 0x80);
		} else if (hc > 0) {
			hc--;
			ho = m & CTR ? 0 : largo();
		}
		break;
	case K_DER:
		if (!(m & CTR) && ho < n) {
			do
				ho++;
			while (ho < n && (hv.s[ho] & 0xC0) == 0x80);
		} else if (hc < hk - 1) {
			hc++;
			ho = 0;
		}
		break;
	case K_INI:
		if (m & ALT)
			hc = 0;
		else
			hf = 0;
		ho = 0;
		break;
	case K_FIN:
		if (m & ALT)
			hc = hk - 1;
		else
			hf = hnv ? regv(hnv - 1) : 0;
		ho = largo();
		break;
	case 13:
		if ((r = sigue_v(hf, 1)) == hf) {
			filahoja(A_FILA);
			return;
		}
		hf = r;
		ho = largo();
		break;
	case 9:
		if (hc < hk - 1)
			hc++;
		else if ((r = sigue_v(hf, 1)) == hf) {
			hc = 0;
			filahoja(A_FILA);
			return;
		} else {
			hf = r;
			hc = 0;
		}
		ho = largo();
		break;
	case K_STAB:
		if (hc > 0)
			hc--;
		else if (hf > 0) {
			hf = sigue_v(hf, -1);
			hc = hk - 1;
		}
		ho = largo();
		break;
	case 127: editacelda("", 0, -1); return;
	case K_SUP: editacelda("", 0, 1); return;
	case K_PEGAR: pegahoja(pg.s ? pg.s : "", pg.n); return;
	default:
		if (k >= 32 && k < K_ESC && k != 127)
			editacelda(s, cod(k, s), 0);
		return;
	}
	n = largo();
	if (ho > n)
		ho = n;
	ponraw();
}

static void convierte(void)
{
	Tabla t = { 0 };
	Lin c = { 0 };
	int r, i, j, e;

	e = elegir(tx(T_CSV_ESTILO), ops(3, T_MARKDOWN, T_CUADRO, T_CANCELAR));
	if (e < 0 || e == 2)
		return;
	t.e = e + 1;
	for (i = -1; i < hnv; i++) {
		r = i < 0 ? 0 : regv(i);
		partir(r);
		filanueva(&t, t.f);
		for (j = 0; j < hnc && j < MAXC; j++) {
			valor(j, &hv);
			c.n = 0;
			add(&c, hv.s, hv.n);
			reserva(&t, t.f, j + 1);
			CE(&t, t.f - 1, j) = celdacsv(&c, t.e);
		}
		if (j > t.k)
			t.k = j;
	}
	if (sigue()) {
		vacio();
		ruta[0] = 0;
		cif = 0;
		crypto_wipe(clave, 32);
		hoja = 0;
		metetabla(&t);
		avisa(tx(T_CSV_OK), t.f);
	}
	suelta_t(&t);
	free(c.s);
}

static int escsv(const char *p)
{
	const char *e = strrchr(p, '.');
	char b[8];
	int i;

	if (!e || strlen(e) > 4)
		return 0;
	for (i = 0; e[i]; i++)
		b[i] = tolower((unsigned char)e[i]);
	b[i] = 0;
	return !strcmp(b, ".csv") || !strcmp(b, ".tsv");
}

static void iniciahoja(void)
{
	Lin b = { 0 };
	int y;
	const char *e = strrchr(ruta, '.');

	for (y = 0; y < nl && b.n < 65536; y++) {
		add(&b, L[y].s, L[y].n);
		add(&b, "\n", 1);
	}
	hsep = e && escsv(ruta) && tolower((unsigned char)e[1]) == 't' ? '\t' : b.n ? separador(b.s, b.n) : ',';
	free(b.s);
	hoja = 1;
	hf = hc = ho = hfr = hfc = 0;
	quitafiltros();
	rehace();
	ponraw();
	avisa(tx(T_HOJAFILAS), hn - 1, hk);
}

static int hojahaz(int a)
{
	char b[32];
	int i;

	switch (a) {
	case A_TODO: case A_VACIAS: case A_REEMPL: case A_TABLA:
		avisa("%s", tx(T_NOHOJA));
		return 1;
	case A_COPIAR:
	case A_CORTAR:
		partir(hf);
		valor(hc, &hv);
		portapapeles(strdup(hv.s), hv.n);
		if (a == A_CORTAR)
			editacelda("", 0, 2);
		return 1;
	case A_DUP: case A_BORRALIN: case A_FILA: case A_QFILA:
		filahoja(a);
		return 1;
	case A_COL: case A_QCOL:
		columnas(a);
		return 1;
	case A_ORDAZ: case A_ORDZA:
		ordena(a == A_ORDAZ ? 1 : -1);
		return 1;
	case A_IRA:
		b[0] = 0;
		if (pedir(tx(T_IRAFILA), b, 12, 0) && (i = atoi(b)) >= 0) {
			hf = i >= hn ? hn - 1 : i;
			ho = 0;
			ponraw();
		}
		return 1;
	case A_CSV:
		convierte();
		return 1;
	case A_FILTRAR:
		filtrar();
		return 1;
	case A_SINFILTROS:
		quitafiltros();
		filtra(hf);
		ponraw();
		avisa("%s", tx(T_FILTROSQ));
		return 1;
	}
	return 0;
}

typedef struct { int neg, op; double a, b; unsigned ip, mk; char *p; } Cond;
typedef struct { char *txt, **val; Cond *c; int nc, nval; } Filtro;
typedef struct { char *s, *k; int n, on, t; double v; } Val;

enum { F_CONT, F_GLOB, F_CIDR, F_RANGO, F_MAY, F_MAYI, F_MEN, F_MENI, F_IGUAL };

static Filtro *hfl;
static int hflc;
static Val *dv;
static int ndv, cdv, *dh, cdh, *lv, nlv, modlista;
static Lin hf1, hf2;

static void plega(const char *s, Lin *o)
{
	int i = 0, n = strlen(s), c, k;
	char b;

	o->n = 0;
	while (i < n) {
		k = dec(s + i, n - i, &c);
		if (c == 0xF1 || c == 0xD1)
			add(o, "n\177", 2);
		else if (c < 128 || (c >= 0xC0 && c <= 0xFF && c != 0xD7 && c != 0xF7)) {
			b = pliega(c);
			add(o, &b, 1);
		} else
			add(o, s + i, k);
		i += k;
	}
	crece(o, o->n + 1);
	o->s[o->n] = 0;
}

static int tipode(const char *s, double *v)
{
	while (*s == ' ')
		s++;
	if (!*s)
		return 4;
	if (esip(s, v))
		return 2;
	if (esfecha(s, v) || numero(s, v))
		return 1;
	return 3;
}

static int glob(const char *p, const char *s)
{
	const char *ap = 0, *as = 0;

	while (*s) {
		if (*p == '*') {
			ap = ++p;
			as = s;
		} else if (*p == '?' || *p == *s) {
			p++;
			s++;
		} else if (ap) {
			p = ap;
			s = ++as;
		} else
			return 0;
	}
	while (*p == '*')
		p++;
	return !*p;
}

static int solodia(const char *s)
{
	int n = strlen(s);

	while (n && s[n - 1] == ' ')
		n--;
	return (n == 10 && s[4] == '-') || (n <= 10 && strchr(s, '/') && !strchr(s, ':'));
}

static void suelta_f(Filtro *f)
{
	int i;

	for (i = 0; i < f->nc; i++)
		free(f->c[i].p);
	for (i = 0; i < f->nval; i++)
		free(f->val[i]);
	free(f->c);
	free(f->val);
	free(f->txt);
	memset(f, 0, sizeof *f);
}

static int cidr(const char *s, Cond *c)
{
	unsigned a[5];
	int n = 0;

	if (sscanf(s, "%3u.%3u.%3u.%3u/%2u%n", a, a + 1, a + 2, a + 3, a + 4, &n) != 5 || s[n] ||
		a[0] > 255 || a[1] > 255 || a[2] > 255 || a[3] > 255 || a[4] > 32)
		return 0;
	c->op = F_CIDR;
	c->mk = a[4] ? 0xFFFFFFFFu << (32 - a[4]) : 0;
	c->ip = (a[0] << 24 | a[1] << 16 | a[2] << 8 | a[3]) & c->mk;
	return 1;
}

static int intervalo(const char *s, Cond *c)
{
	const char *q = strstr(s, "..");
	char a[64];

	if (!q || q - s >= (int)sizeof a)
		return 0;
	memcpy(a, s, q - s);
	a[q - s] = 0;
	if (tipode(a, &c->a) > 2 || tipode(q + 2, &c->b) > 2)
		return 0;
	c->op = F_RANGO;
	if (solodia(q + 2))
		c->b += 235959.999;
	return 1;
}

static void compila(Filtro *f, const char *e)
{
	char *t = strdup(e), *a;
	Cond c;
	int n;

	f->txt = strdup(e);
	for (a = strtok(t, "|"); a; a = strtok(0, "|")) {
		while (*a == ' ')
			a++;
		for (n = strlen(a); n && a[n - 1] == ' '; n--)
			a[n - 1] = 0;
		memset(&c, 0, sizeof c);
		if (*a == '!') {
			c.neg = 1;
			a++;
		}
		if (!*a || cidr(a, &c) || intervalo(a, &c))
			;
		else if (strchr("<>=", *a)) {
			c.op = *a == '=' ? F_IGUAL : *a == '>' ? (a[1] == '=' ? F_MAYI : F_MAY) : (a[1] == '=' ? F_MENI : F_MEN);
			a += 1 + (a[1] == '=' && *a != '=');
			while (*a == ' ')
				a++;
			if (tipode(a, &c.a) > 2) {
				plega(a, &hf1);
				c.p = strdup(hf1.s);
			} else if (solodia(a) && (c.op == F_MAY || c.op == F_MENI))
				c.a += 235959.999;
		} else {
			plega(a, &hf1);
			c.op = strpbrk(a, "*?") ? F_GLOB : F_CONT;
			c.p = strdup(hf1.s);
		}
		if (!*a)
			continue;
		f->c = re(f->c, (f->nc + 1) * sizeof *f->c);
		f->c[f->nc++] = c;
	}
	free(t);
}

static int cumplec(Cond *c, const char *s)
{
	double v;
	int t, r;

	switch (c->op) {
	case F_CONT:
		plega(s, &hf2);
		return strstr(hf2.s, c->p) != 0;
	case F_GLOB:
		plega(s, &hf2);
		return glob(c->p, hf2.s);
	case F_CIDR:
		while (*s == ' ')
			s++;
		return esip(s, &v) && ((unsigned)v & c->mk) == c->ip;
	}
	if (c->p) {
		plega(s, &hf2);
		r = strcmp(hf2.s, c->p);
	} else {
		t = tipode(s, &v);
		if (t > 2)
			return 0;
		if (c->op == F_RANGO)
			return v >= c->a && v <= c->b;
		r = (v > c->a) - (v < c->a);
	}
	switch (c->op) {
	case F_MAY: return r > 0;
	case F_MAYI: return r >= 0;
	case F_MEN: return r < 0;
	case F_MENI: return r <= 0;
	}
	return !r;
}

static int pcmp(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static int cumple(Filtro *f, const char *s)
{
	int i, pos = 0, si = 0;

	if (f->val && !bsearch(&s, f->val, f->nval, sizeof *f->val, pcmp))
		return 0;
	for (i = 0; i < f->nc; i++) {
		if (f->c[i].neg) {
			if (cumplec(f->c + i, s))
				return 0;
		} else {
			pos = 1;
			si |= cumplec(f->c + i, s);
		}
	}
	return !pos || si;
}

static int activo(int j)
{
	return j < hflc && (hfl[j].nc || hfl[j].val);
}

static int filtros(void)
{
	int j, n = 0;

	for (j = 0; j < hflc; j++)
		n += activo(j);
	return n;
}

static int pasa(int r, int salvo)
{
	int j;

	partir(r);
	for (j = 0; j < hflc; j++)
		if (j != salvo && activo(j) && !cumple(hfl + j, valor(j, &hv)))
			return 0;
	return 1;
}

static void filtra(int deja)
{
	int r;

	free(hvis);
	hvis = 0;
	hnv = hn - 1;
	if (!filtros())
		return;
	hvis = re(0, hn * sizeof *hvis);
	for (hnv = 0, r = 1; r < hn; r++)
		if (r == deja || pasa(r, -1))
			hvis[hnv++] = r;
}

static int visible(int r)
{
	int i;

	if (!r || !hvis)
		return 1;
	i = cota(r);
	return i < hnv && hvis[i] == r;
}

static void ponfiltro(int j)
{
	if (j >= hflc) {
		hfl = re(hfl, (j + 1) * sizeof *hfl);
		memset(hfl + hflc, 0, (j + 1 - hflc) * sizeof *hfl);
		hflc = j + 1;
	}
}

static void desplaza_f(int a)
{
	if (a == A_COL) {
		if (hc + 1 < hflc) {
			ponfiltro(hflc);
			memmove(hfl + hc + 2, hfl + hc + 1, (hflc - hc - 2) * sizeof *hfl);
			memset(hfl + hc + 1, 0, sizeof *hfl);
		}
	} else if (hc < hflc) {
		suelta_f(hfl + hc);
		memmove(hfl + hc, hfl + hc + 1, (hflc - hc - 1) * sizeof *hfl);
		hflc--;
	}
}

static void quitafiltros(void)
{
	int j;

	for (j = 0; j < hflc; j++)
		suelta_f(hfl + j);
	free(hfl);
	hfl = 0;
	hflc = 0;
}

static void miles(long n, char *b)
{
	char t[32];
	int l, i, j = 0;

	l = snprintf(t, sizeof t, "%ld", n);
	for (i = 0; i < l; i++) {
		if (i && (l - i) % 3 == 0)
			b[j++] = idiomas[idi].coma ? '.' : ',';
		b[j++] = t[i];
	}
	b[j] = 0;
}

static unsigned hsh(const char *s)
{
	unsigned h = 2166136261u;

	while (*s)
		h = (h ^ (unsigned char)*s++) * 16777619u;
	return h;
}

static void cuenta(int col)
{
	int r, i, m;
	unsigned h;
	const char *s;

	for (i = 0; i < ndv; i++) {
		free(dv[i].s);
		free(dv[i].k);
	}
	ndv = 0;
	m = 1024;
	if (cdh < m) {
		free(dh);
		dh = re(0, (cdh = m) * sizeof *dh);
	}
	memset(dh, -1, cdh * sizeof *dh);
	for (r = 1; r < hn; r++) {
		if (!pasa(r, col))
			continue;
		s = valor(col, &hv);
		for (h = hsh(s) & (cdh - 1); dh[h] >= 0 && strcmp(dv[dh[h]].s, s); h = (h + 1) & (cdh - 1))
			;
		if (dh[h] >= 0) {
			dv[dh[h]].n++;
			continue;
		}
		if (ndv == cdv)
			dv = re(dv, (cdv = cdv * 2 + 256) * sizeof *dv);
		memset(dv + ndv, 0, sizeof *dv);
		dv[ndv].s = strdup(s);
		dv[ndv].n = 1;
		dv[ndv].on = !activo(col) || cumple(hfl + col, s);
		dh[h] = ndv++;
		if (ndv * 2 > cdh) {
			free(dh);
			dh = re(0, (cdh *= 4) * sizeof *dh);
			memset(dh, -1, cdh * sizeof *dh);
			for (i = 0; i < ndv; i++) {
				for (h = hsh(dv[i].s) & (cdh - 1); dh[h] >= 0; h = (h + 1) & (cdh - 1))
					;
				dh[h] = i;
			}
		}
	}
	for (i = 0; i < ndv; i++) {
		dv[i].t = tipode(dv[i].s, &dv[i].v);
		if (dv[i].t == 3) {
			plega(dv[i].s, &hf1);
			dv[i].k = strdup(hf1.s);
		}
	}
}

static int cmpval(const void *a, const void *b)
{
	const Val *x = dv + *(const int *)a, *y = dv + *(const int *)b;
	int r;

	if (modlista && x->n != y->n)
		return modlista == 1 ? x->n - y->n : y->n - x->n;
	if (x->t != y->t)
		return x->t - y->t;
	if (x->t == 3)
		r = strcmp(x->k, y->k);
	else
		r = (x->v > y->v) - (x->v < y->v);
	return r ? r : strcmp(x->s, y->s);
}

static void listavals(const char *e)
{
	Filtro f = { 0 };
	int i;

	if (*e)
		compila(&f, e);
	lv = re(lv, (ndv + 1) * sizeof *lv);
	for (nlv = i = 0; i < ndv; i++)
		if (!*e || cumple(&f, dv[i].s))
			lv[nlv++] = i;
	qsort(lv, nlv, sizeof *lv, cmpval);
	suelta_f(&f);
}

static void aplicafiltro(int col, const char *e)
{
	Filtro *f;
	int i, todos = 1, n = 0;

	ponfiltro(col);
	f = hfl + col;
	suelta_f(f);
	for (i = 0; i < nlv; i++)
		if (!dv[lv[i]].on)
			todos = 0;
	if (!todos || (!*e && nlv < ndv)) {
		f->val = re(0, (nlv + 1) * sizeof *f->val);
		for (i = 0; i < nlv; i++)
			if (dv[lv[i]].on)
				f->val[n++] = strdup(dv[lv[i]].s);
		f->nval = n;
		qsort(f->val, n, sizeof *f->val, pcmp);
		if (!n) {
			free(f->val);
			f->val = re(0, sizeof *f->val);
			f->val[0] = strdup("\001");
			f->nval = 1;
		}
	}
	if (*e)
		compila(f, e);
}

static int pendiente(void)
{
	struct pollfd p = { 0, POLLIN, 0 };

	return ebi < ebn || poll(&p, 1, 0) > 0;
}

static int filtrocol(int col)
{
	static const int ops2[] = { T_ORDAZ, T_ORDZA };
	char e[128] = "", b[32], c[200];
	int s = 2, top = 0, k, x = 0, y = 4, w = 44, h = 10, i, j, n = 1, lista = 1, a, mw = 0, cambio = 1, base = 7, r;

	if (activo(col) && hfl[col].txt)
		snprintf(e, sizeof e, "%s", hfl[col].txt);
	avisa("%s", tx(T_CONTANDO));
	pinta();
	vuelca(-1, -1);
	aviso[0] = 0;
	cuenta(col);
	for (;;) {
		if (!cambio || !pendiente()) {
			if (cambio) {
				listavals(e);
				for (mw = 0, i = 0; i < nlv && i < 2000; i++)
					if ((a = ancho(dv[lv[i]].s)) > mw)
						mw = a;
				cambio = 0;
			}
			w = mw + 16;
			if (w < 44)
				w = 44;
			if (w > cols - 4)
				w = cols - 4;
			x = colx(col);
			if (x + w > cols - 1)
				x = cols - 1 - w;
			y = 4;
			h = filas - 2 - y;
			lista = h - base - 5;
			if (lista < 1)
				lista = 1;
			n = nlv + 1;
			if (s >= 3 + n + 1)
				s = 3 + n;
			if (s >= 2 && s - 2 < top)
				top = s - 2;
			if (s >= 2 && s - 2 >= top + lista)
				top = s - 2 - lista + 1;
			if (s >= 2 + n)
				top = n > lista ? n - lista : 0;
			pinta();
			caja(y, x, h, w, 0);
			for (i = 0; i < 2; i++) {
				a = s == i ? INV : NOR;
				if (a == INV)
					rellena(y + 1 + i, x + 1, w - 2, " ", INV);
				texto(y + 1 + i, x + 2, tx(ops2[i]), a, w - 4);
			}
			raya(y + 3, x, w);
			j = texto(y + 4, x + 2, tx(T_FILTRO), MEN, w - 4);
			texto(y + 4, x + 2 + j, ": ", NOR, 2);
			rellena(y + 4, x + 4 + j, w - 6 - j, " ", CAM);
			if (e[0])
				texto(y + 4, x + 4 + j, e, CAM, w - 6 - j);
			else
				texto(y + 4, x + 4 + j, tx(T_FPISTA), NUM, w - 6 - j);
			texto(y + 5, x + 2, nlv || !e[0] ? "" : tx(T_FNADA), NUM, w - 4);
			raya(y + 6, x, w);
			for (i = 0; i < lista && top + i < n; i++) {
				j = top + i;
				a = s == 2 + j ? INV : NOR;
				if (a == INV)
					rellena(y + base + i, x + 1, w - 2, " ", INV);
				if (!j) {
					for (r = 1, k = 0; k < nlv; k++)
						r &= dv[lv[k]].on;
					snprintf(c, sizeof c, "[%s] %s", r ? "x" : " ", tx(T_TODOS));
					for (k = 0, r = 0; k < nlv; k++)
						r += dv[lv[k]].n;
					miles(r, b);
				} else {
					Val *v = dv + lv[j - 1];
					snprintf(c, sizeof c, "[%s] %s", v->on ? "x" : " ", v->s[0] ? v->s : tx(T_VACIAS2));
					miles(v->n, b);
				}
				texto(y + base + i, x + 2, c, a == INV ? INV : j && !dv[lv[j - 1]].s[0] ? NUM : NOR, w - 6 - (int)strlen(b));
				texto(y + base + i, x + w - 2 - (int)strlen(b), b, a == INV ? INV : NUM, strlen(b));
			}
			raya(y + h - 5, x, w);
			a = s == 2 + n ? INV : NOR;
			if (a == INV)
				rellena(y + h - 4, x + 1, w - 2, " ", INV);
			texto(y + h - 4, x + 2, tx(T_LISTA0 + modlista), a, w - 4);
			a = s == 3 + n ? INV : NOR;
			if (a == INV)
				rellena(y + h - 3, x + 1, w - 2, " ", INV);
			texto(y + h - 3, x + 2, tx(T_QUITARF), a, w - 4);
			texto(y + h - 2, x + 2, tx(T_FPIE), NUM, w - 4);
			j = ancho(tx(T_FILTRO));
			vuelca(y + 4, x + 4 + j + (ancho(e) < w - 7 - j ? ancho(e) : w - 7 - j));
		}
		k = tecla(-1);
		if (k == K_ESC)
			return 0;
		if (k == K_ARR && s > 0)
			s--;
		else if (k == K_ABA && s < 3 + n)
			s++;
		else if (k == K_REP)
			s = s - lista < 0 ? 0 : s - lista;
		else if (k == K_AVP)
			s = s + lista > 3 + n ? 3 + n : s + lista;
		else if (k == ' ' && s >= 2 && s < 2 + n) {
			if (s == 2) {
				for (r = 1, i = 0; i < nlv; i++)
					r &= dv[lv[i]].on;
				for (i = 0; i < nlv; i++)
					dv[lv[i]].on = !r;
			} else
				dv[lv[s - 3]].on ^= 1;
		} else if (k == 127) {
			for (i = strlen(e); i && (e[i - 1] & 0xC0) == 0x80; i--)
				;
			if (i)
				e[i - 1] = 0;
			cambio = 1;
		} else if (k == 13) {
			if (s < 2)
				return s ? A_ORDZA : A_ORDAZ;
			if (s == 2 + n) {
				modlista = (modlista + 1) % 3;
				cambio = 1;
				continue;
			}
			if (s == 3 + n) {
				if (col < hflc)
					suelta_f(hfl + col);
				return -1;
			}
			aplicafiltro(col, e);
			return -1;
		} else if (k == K_RATON && !rs && !(rb & ~28)) {
			if (rx <= x || rx >= x + w - 1 || ry <= y || ry >= y + h - 1)
				return 0;
			if (ry - y - 1 < 2)
				return ry - y - 1 ? A_ORDZA : A_ORDAZ;
			if (ry >= y + base && ry < y + base + lista && top + ry - y - base < n) {
				s = 2 + top + ry - y - base;
				k = ' ';
				if (s == 2) {
					for (r = 1, i = 0; i < nlv; i++)
						r &= dv[lv[i]].on;
					for (i = 0; i < nlv; i++)
						dv[lv[i]].on = !r;
				} else
					dv[lv[s - 3]].on ^= 1;
			} else if (ry == y + h - 4) {
				modlista = (modlista + 1) % 3;
				cambio = 1;
			} else if (ry == y + h - 3) {
				if (col < hflc)
					suelta_f(hfl + col);
				return -1;
			}
		} else if (k == K_RATON && !rs && (rb & ~28) == 64)
			s = s > 2 ? s - 1 : s;
		else if (k == K_RATON && !rs && (rb & ~28) == 65)
			s = s < 2 + n - 1 ? s + 1 : s;
		else if (k >= 32 && k < K_ESC && k != 127) {
			char u[8];
			int l = cod(k, u);

			if ((int)strlen(e) + l < (int)sizeof e - 1) {
				memcpy(e + strlen(e), u, l);
				e[strlen(e) + l] = 0;
			}
			if (s > 2)
				s = 2;
			cambio = 1;
		}
	}
}

static void filtrar(void)
{
	char b1[32], b2[32];
	int a;

	ajustahoja();
	a = filtrocol(hc);
	if (a > 0) {
		haz(a);
		return;
	}
	if (a < 0) {
		filtra(-1);
		hf = hnv ? (visible(hf) && hf ? hf : regv(0)) : 0;
		ho = 0;
		hfr = 0;
		ponraw();
		miles(hnv, b1);
		miles(hn - 1, b2);
		if (filtros())
			avisa(tx(T_FILTRADAS), b1, b2);
	}
}

static int confruta(char *b, int n, int crear)
{
	const char *x = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME");

	if (x && x[0] == '/')
		snprintf(b, n, "%s", x);
	else if (h && h[0] == '/')
		snprintf(b, n, "%s/.config", h);
	else
		return 0;
	if (crear)
		mkdir(b, 0700);
	strncat(b, "/isi", n - strlen(b) - 1);
	if (crear)
		mkdir(b, 0700);
	strncat(b, "/config", n - strlen(b) - 1);
	return 1;
}

static int porcodigo(const char *c, int n)
{
	int i;

	for (i = 0; i < (int)(sizeof idiomas / sizeof *idiomas); i++)
		if (!strncmp(idiomas[i].cod, c, n))
			return i;
	return 0;
}

static int idiomasistema(void)
{
	static const char *v[] = { "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG" };
	const char *l = 0;
	char p[3] = "";
	int i;

	for (i = 0; i < 4 && !(l && *l); i++)
		l = getenv(v[i]);
	if (!l || strlen(l) < 2)
		return 0;
	if (strncmp(l, "en", 2))
		return porcodigo(l, 2);
	if (l[2] == '_' && l[3] && l[4]) {
		p[0] = l[3];
		p[1] = l[4];
	}
	return porcodigo(p[0] && strstr("GB IE AU NZ ZA IN", p) ? "en_GB" : "en_US", 5);
}

static void cargaconf(void)
{
	char b[PATH_MAX], l[128];
	FILE *f;
	int i, hay = 0;

	if (confruta(b, sizeof b, 0) && (f = fopen(b, "r"))) {
		while (fgets(l, sizeof l, f)) {
			l[strcspn(l, "\r\n")] = 0;
			if (!strncmp(l, "idioma=", 7)) {
				for (i = 0; i < (int)(sizeof idiomas / sizeof *idiomas); i++)
					if (!strcmp(idiomas[i].cod, l + 7)) {
						idi = i;
						hay = 1;
					}
			} else if (!strncmp(l, "numeros=", 8))
				numeros = l[8] == '1';
			else if (!strncmp(l, "resaltar=", 9))
				resalta = l[9] == '1';
			else if (!strncmp(l, "auto=", 5))
				autoioc = l[5] == '1';
		}
		fclose(f);
	}
	if (!hay)
		idi = idiomasistema();
}

static void guardaconf(void)
{
	char b[PATH_MAX];
	FILE *f;

	if (!confruta(b, sizeof b, 1) || !(f = fopen(b, "w")))
		return;
	fprintf(f, "idioma=%s\nnumeros=%d\nresaltar=%d\nauto=%d\n", idiomas[idi].cod, numeros, resalta, autoioc);
	fclose(f);
}

static void eligeidioma(void)
{
	int n = sizeof idiomas / sizeof *idiomas, s = idi, top = 0, h, w, m = 0, y, x, i, k, d;
	char b[96];

	for (i = 0; i < n; i++)
		if (ancho(idiomas[i].nombre) > m)
			m = ancho(idiomas[i].nombre);
	for (;;) {
		pinta();
		h = n + 2 > th ? th : n + 2;
		if (h < 3)
			h = 3;
		w = m + 10 > cols - 4 ? cols - 4 : m + 10;
		if (s < top)
			top = s;
		if (s >= top + h - 2)
			top = s - (h - 3);
		y = 3 + (th - h) / 2;
		x = (cols - w) / 2;
		caja(y, x, h, w, tx(T_T_IDIOMA));
		for (i = 0; i < h - 2 && top + i < n; i++) {
			snprintf(b, sizeof b, "%s %s", top + i == idi ? "●" : " ", idiomas[top + i].nombre);
			if (top + i == s)
				rellena(y + 1 + i, x + 1, w - 2, " ", INV);
			texto(y + 1 + i, x + 3, b, top + i == s ? INV : NOR, w - 5);
		}
		vuelca(-1, -1);
		k = tecla(-1);
		if (k == K_ESC)
			return;
		if (k == K_ARR || k == K_STAB)
			s = (s + n - 1) % n;
		else if (k == K_ABA || k == 9)
			s = (s + 1) % n;
		else if (k == K_REP || k == K_INI)
			s = 0;
		else if (k == K_AVP || k == K_FIN)
			s = n - 1;
		else if (k == 13 || k == ' ')
			break;
		else if (k == K_RATON && !rs && (rb == 64 || rb == 65))
			s = rb == 64 ? (s ? s - 1 : 0) : (s < n - 1 ? s + 1 : s);
		else if (k == K_RATON && !rs && !(rb & ~28)) {
			d = doble_clic();
			i = ry - y - 1;
			if (rx <= x || rx >= x + w - 1 || i < 0 || i >= h - 2 || top + i >= n)
				return;
			if (top + i == s || d)
				break;
			s = top + i;
		}
	}
	idi = s;
	guardaconf();
}

static void haz(int a)
{
	char b[PATH_MAX];
	int i, sf = hf, sc = hc, so = ho;

	if (hoja && hojahaz(a))
		return;
	switch (a) {
	case A_NUEVO:
		if (sigue()) {
			hoja = 0;
			vacio();
			ruta[0] = 0;
			cif = 0;
			crypto_wipe(clave, 32);
		}
		break;
	case A_ABRIR:
		if (sigue() && explora(0, T_T_ABRIR, b))
			abre(b);
		break;
	case A_ABRIRCSV:
		if (sigue() && explora(0, T_T_ABRIRCSV, b)) {
			forzado = 1;
			abre(b);
			forzado = 0;
		}
		break;
	case A_ORDAZ:
	case A_ORDZA:
	case A_FILTRAR:
	case A_SINFILTROS:
		avisa("%s", tx(T_SOLOHOJA));
		break;
	case A_GUARDAR: guardar(); break;
	case A_COMO: guarda_como(); break;
	case A_CIFRAR: cifrar(); break;
	case A_DESCIFRAR:
		if (!cif)
			avisa("%s", tx(T_NOCIF));
		else if (!elegir(tx(T_QUITARQ), ops(2, T_SI, T_NO))) {
			cif = 0;
			crypto_wipe(clave, 32);
			mod = 1;
			guardar();
		}
		break;
	case A_SALIR:
		if (sigue()) {
			crypto_wipe(clave, 32);
			normal();
			exit(0);
		}
		break;
	case A_DESH:
		if (!deshaz(&pd, &nd, &pr, &nr, &crp, 1) && !solo)
			avisa("%s", tx(T_NODESH));
		break;
	case A_REH:
		if (!deshaz(&pr, &nr, &pd, &nd, &cdp, 0) && !solo)
			avisa("%s", tx(T_NOREH));
		break;
	case A_CORTAR:
		if (!bloqueado() && copia())
			borrasel();
		break;
	case A_COPIAR: copia(); break;
	case A_PEGAR: pega(); break;
	case A_TODO:
		sel = 1;
		ay = ax = 0;
		cy = nl - 1;
		cx = L[cy].n;
		break;
	case A_DUP:
		if (bloqueado())
			break;
		i = cx;
		{
			Lin t = { 0 };
			add(&t, "\n", 1);
			add(&t, L[cy].s, L[cy].n);
			ins(cy, L[cy].n, t.s, t.n);
			free(t.s);
		}
		cx = i;
		sel = 0;
		break;
	case A_BORRALIN:
		sel = 0;
		if (nl == 1)
			del(0, 0, 0, L[0].n);
		else if (cy < nl - 1)
			del(cy, 0, cy + 1, 0);
		else {
			del(cy - 1, L[cy - 1].n, cy, L[cy].n);
			cx = 0;
		}
		break;
	case A_VACIAS: vacias(); break;
	case A_BUSCAR:
		if (pedir(tx(T_BUSCARP), qb, sizeof qb, 0) && qb[0] && !busca(1, 1))
			avisa(tx(T_NOENC), qb);
		break;
	case A_SIG:
	case A_ANT:
		if (!qb[0])
			haz(A_BUSCAR);
		else if (!busca(a == A_SIG ? 1 : -1, 1))
			avisa(tx(T_NOENC), qb);
		break;
	case A_REEMPL:
		if (!bloqueado())
			reemplaza();
		break;
	case A_IRA:
		b[0] = 0;
		if (pedir(tx(T_IRAP), b, 12, 0) && (i = atoi(b)) > 0) {
			cy = i > nl ? nl - 1 : i - 1;
			cx = 0;
			sel = 0;
			fy = cy - th / 2 > 0 ? cy - th / 2 : 0;
		}
		break;
	case A_NUM: numeros ^= 1; guardaconf(); break;
	case A_TABLA: nuevatabla(); break;
	case A_CSV: importa(); break;
	case A_IDIOMA: eligeidioma(); break;
	case A_FILA: case A_COL: case A_QFILA: case A_QCOL:
		if (!bloqueado())
			topera(a);
		break;
	case A_RESALTA: resalta ^= 1; guardaconf(); break;
	case A_AUTO: autoioc ^= 1; guardaconf(); break;
	case A_DESACT: iocs(0); break;
	case A_REACT: iocs(1); break;
	case A_LISTA: listaiocs(); break;
	case A_ATAJOS: atajos(); break;
	case A_ACERCA: acercade(); break;
	}
	if (hoja && (a == A_DESH || a == A_REH || a == A_DESACT || a == A_REACT)) {
		rehace();
		desderaw();
	}
	if (hoja && (a == A_BUSCAR || a == A_SIG || a == A_ANT) && sel) {
		desderaw();
		for (i = 0; !visible(hf) && i <= hn && busca(a == A_ANT ? -1 : 1, 1); i++)
			desderaw();
		if (!visible(hf)) {
			avisa(tx(T_NOENC), qb);
			hf = sf;
			hc = sc;
			ho = so;
			ponraw();
		}
	}
}

static int tabsep(int e, int atras)
{
	if (atras)
		return e == 1 ? sepen(e, cy, cx - 1) : sepen(e, cy, cx - 3);
	return sepen(e, cy, cx);
}

static void edita(int k)
{
	int b = k & 0xFFFFFF, m = k & ~0xFFFFFF, t = filat(cy), e = estab(cy), i;
	char s[8];
	Lin l = { 0 };

	if (k == K_RATON) {
		raton();
		return;
	}
	if ((m & ALT) && b < 128) {
		if (letramenu(b) >= 0)
			haz(menu(letramenu(b)));
		return;
	}
	if (hoja) {
		teclahoja(k);
		return;
	}
	if (b >= K_ARR && b <= K_AVP) {
		mueve(b, m);
		return;
	}
	switch (k) {
	case K_ESC:
		if (sel)
			sel = 0;
		else
			aviso[0] = 0;
		return;
	case K_F10: haz(menu(0)); return;
	case K_F1: haz(A_ATAJOS); return;
	case K_F3: haz(A_SIG); return;
	case K_F3 | MAY: haz(A_ANT); return;
	}
	if (k < 32 && atajo[k]) {
		haz(atajo[k]);
		return;
	}
	if (e && !t && !(k == 13 && cx == L[cy].n))
		return;
	switch (k) {
	case 13:
		if (t) {
			topera(2);
			return;
		}
		borrasel();
		autodesactiva();
		add(&l, "\n", 1);
		for (i = 0; i < cx && (L[cy].s[i] == ' ' || L[cy].s[i] == '\t'); i++)
			;
		add(&l, L[cy].s, e ? 0 : i);
		ins(cy, cx, l.s, l.n);
		free(l.s);
		return;
	case 9:
		if (t) {
			topera(1);
			return;
		}
		borrasel();
		autodesactiva();
		ins(cy, cx, "\t", 1);
		return;
	case K_STAB:
		if (t)
			topera(-1);
		return;
	case 127:
		if (borrasel())
			break;
		if (t && (!cx || tabsep(e, 1)))
			return;
		if (cx) {
			i = cx;
			do
				i--;
			while (i && (L[cy].s[i] & 0xC0) == 0x80);
			del(cy, i, cy, cx);
		} else if (cy)
			del(cy - 1, L[cy - 1].n, cy, 0);
		break;
	case K_SUP:
		if (borrasel())
			break;
		if (t && (cx >= L[cy].n || tabsep(e, 0)))
			return;
		if (cx < L[cy].n) {
			i = cx;
			do
				i++;
			while (i < L[cy].n && (L[cy].s[i] & 0xC0) == 0x80);
			del(cy, cx, cy, i);
		} else if (cy < nl - 1)
			del(cy, cx, cy + 1, 0);
		break;
	case K_PEGAR:
		inserta(pg.s, pg.n);
		return;
	default:
		if (k < 32 || k >= K_ESC || k == 127 || (t && e == 1 && k == '|'))
			return;
		if (t && !sel)
			celda();
		borrasel();
		if (strchr(" ,;)]}\"'<>!?", k))
			autodesactiva();
		ins(cy, cx, s, cod(k, s));
	}
	if (t && filat(cy))
		tfix();
}

int main(int argc, char **argv)
{
	struct sigaction sa;
	char *f = 0;
	int i, k, ms, ya = 0;

	cargaconf();
	for (i = 1; i < argc; i++) {
		if (!ya && !strcmp(argv[i], "--"))
			ya = 1;
		else if (!ya && !strcmp(argv[i], "-r"))
			solo = 1;
		else if (!ya && (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version"))) {
			puts("isi " VERSION);
			return 0;
		} else if (!ya && (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))) {
			fputs(tx(T_USO), stdout);
			return 0;
		} else if (!ya && argv[i][0] == '-' && argv[i][1]) {
			fprintf(stderr, "isi: ");
			fprintf(stderr, tx(T_OPCION), argv[i]);
			fprintf(stderr, "\n\n%s", tx(T_USO));
			return 2;
		} else if (f) {
			fprintf(stderr, "isi: %s\n", tx(T_UNO));
			return 2;
		} else
			f = argv[i];
	}
	if (!isatty(0) || !isatty(1)) {
		fprintf(stderr, "isi: %s\n", tx(T_TERMINAL));
		return 1;
	}
	prctl(PR_SET_DUMPABLE, 0);
	if (mlock(clave, sizeof clave)) {}
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = avisotam;
	sigaction(SIGWINCH, &sa, 0);
	sa.sa_handler = corta;
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGHUP, &sa, 0);
	signal(SIGPIPE, SIG_IGN);
	vacio();
	crudo();
	tamano();
	if (f && abre(f) < 0) {
		normal();
		fprintf(stderr, "isi: %s\n", tx(T_CANCELADO));
		return 1;
	}
	if (solo)
		avisa("%s", tx(T_SOLOM));
	for (;;) {
		pistahoja();
		medidas();
		ajusta();
		pinta();
		vuelca(cols < 40 || filas < 8 ? -1 : hoja ? hcy : 3 + cy - fy, hoja ? hcx : x0 + colde(cy, cx) - fx);
		ms = -1;
		if (aviso[0]) {
			ms = (int)(avt + 60 - time(0)) * 1000;
			if (ms <= 0) {
				aviso[0] = 0;
				continue;
			}
		}
		k = tecla(ms);
		if (k == K_NADA || k == K_TAM)
			continue;
		i = k > ' ' && k < K_ESC;
		if (!i || !prev)
			grupo++;
		prev = i;
		edita(k);
	}
}
