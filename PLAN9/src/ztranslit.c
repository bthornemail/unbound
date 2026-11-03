/*
 * ztranslit - ZIGUAROTH's _ARCHIVE/Expressions/transliterate.js in Plan 9 C (ported to unbound with the author's
 * yes), the first of the wiki template bindings: a template's code points mapped through a static mapping, nothing
 * else, under the codex's rules T1 to T6 (CONFIGURATION.yaml). A mapping is a file of lines "U+XXXX": "U+XXXX" (4 to
 * 6 upper-case hex digits), blank lines and lines beginning "#" skipped; one naming ASCII (U+0000 to U+007F, the
 * template's syntax) on either side, past U+10FFFF, or mapping to a surrogate, or naming a code point twice, is
 * refused, in transliterate.js's words. Every other code point is copied unless the mapping names it; then the check,
 * before anything is written: the same length in code points, ASCII untouched, every change a mapped one.
 *	ztranslit [-n] template mapping [output]	the output to [output] or standard output; the message on
 *							standard error, as transliterate.js writes it
 * -n, the falsifier: the ASCII guard on the mapping dropped, so the check after must refuse.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { Max = 0x110000 };

static int noguard;
static long *map;	/* Max entries, -1 where unmapped */
static long nmap;

static void
refuse(char *fmt, ...)
{
	va_list a;
	char buf[512];

	va_start(a, fmt);
	vsnprint(buf, sizeof buf, fmt, a);
	va_end(a);
	fprint(2, "%s\n", buf);
	exits("refused");
}

static int
space(int c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v' || c == '\n';
}

/* "U+" and 4 to 6 upper-case hex digits, the whole string; its value, or -1 */
static long
cp(char *s, char *e)
{
	long v;
	char *p;

	if(e - s < 6 || e - s > 8 || s[0] != 'U' || s[1] != '+')
		return -1;
	v = 0;
	for(p = s + 2; p < e; p++)
		if(*p >= '0' && *p <= '9')
			v = v * 16 + *p - '0';
		else if(*p >= 'A' && *p <= 'F')
			v = v * 16 + *p - 'A' + 10;
		else
			return -1;
	return v;
}

/* a line ^"([^"]+)":\s*"([^"]+)"\s*$ ; the two quoted runs, or 0 */
static int
pairof(char *l, char **a, char **ae, char **b, char **be)
{
	char *p;

	if(*l != '"')
		return 0;
	*a = l + 1;
	for(p = *a; *p && *p != '"'; p++)
		;
	if(*p != '"' || p == *a)
		return 0;
	*ae = p++;
	if(*p++ != ':')
		return 0;
	while(space(*p))
		p++;
	if(*p++ != '"')
		return 0;
	*b = p;
	for(; *p && *p != '"'; p++)
		;
	if(*p != '"' || p == *b)
		return 0;
	*be = p++;
	while(space(*p))
		p++;
	return *p == 0;
}

static void
loadmap(char *file)
{
	Biobuf *in;
	char *l, *t, *a, *ae, *b, *be;
	long i, from, to;

	if((in = Bopen(file, OREAD)) == nil)
		sysfatal("open %s: %r", file);
	if((map = malloc(Max * sizeof map[0])) == nil)
		sysfatal("malloc: %r");
	for(i = 0; i < Max; i++)
		map[i] = -1;
	for(i = 1; (l = Brdstr(in, '\n', 1)) != nil; i++){
		for(t = l; space(*t); t++)
			;
		if(*t == 0 || *t == '#'){
			free(l);
			continue;
		}
		if(!pairof(l, &a, &ae, &b, &be) || (from = cp(a, ae)) < 0 || (to = cp(b, be)) < 0)
			refuse("%s:%ld: not \"U+XXXX\": \"U+XXXX\"", file, i);
		if(!noguard && (from < 0x80 || to < 0x80))
			refuse("%s:%ld: ASCII is the template's syntax and is never mapped", file, i);
		if(from > 0x10FFFF || to > 0x10FFFF || to >= 0xD800 && to <= 0xDFFF)
			refuse("%s:%ld: not a code point", file, i);
		if(map[from] >= 0)
			refuse("%s:%ld: U+%s is mapped twice", file, i, (*ae = 0, a + 2));
		map[from] = to;
		nmap++;
		free(l);
	}
	Bterm(in);
}

void
main(int argc, char **argv)
{
	int fd, n, k;
	char *buf, *s, *e, b[UTFmax];
	long len, cap, nr, i, changed, outlen;
	Rune *src, r, o;
	char *out;

	ARGBEGIN{
	case 'n':
		noguard = 1;
		break;
	default:
		fprint(2, "usage: ztranslit [-n] template mapping [output]\n");
		exits("usage");
	}ARGEND
	if(argc != 2 && argc != 3){
		fprint(2, "usage: ztranslit [-n] template mapping [output]\n");
		exits("usage");
	}
	if((fd = open(argv[0], OREAD)) < 0)
		sysfatal("open %s: %r", argv[0]);
	buf = nil;
	len = cap = 0;
	for(;;){
		if(len == cap){
			cap = cap ? 2 * cap : 65536;
			if((buf = realloc(buf, cap + 1)) == nil)
				sysfatal("realloc: %r");
		}
		if((n = read(fd, buf + len, cap - len)) <= 0)
			break;
		len += n;
	}
	close(fd);
	buf[len] = 0;
	loadmap(argv[1]);
	if((src = malloc((len + 1) * sizeof src[0])) == nil || (out = malloc(len * UTFmax + 1)) == nil)
		sysfatal("malloc: %r");
	for(nr = 0, s = buf, e = buf + len; s < e; nr++)
		s += chartorune(&src[nr], s);
	/* map, then check before writing: the same length, ASCII untouched, every change a mapped one */
	changed = 0;
	outlen = 0;
	for(i = 0; i < nr; i++){
		r = src[i];
		o = map[r] >= 0 ? map[r] : r;
		if(o != r){
			if(r < 0x80)
				refuse("position %ld: an unmapped change", i);
			changed++;
		}
		k = runetochar(b, &o);
		memmove(out + outlen, b, k);
		outlen += k;
	}
	if(argc == 3){
		if((fd = create(argv[2], OWRITE, 0664)) < 0)
			sysfatal("create %s: %r", argv[2]);
	}else
		fd = 1;
	if(write(fd, out, outlen) != outlen)
		sysfatal("write: %r");
	if(argc == 3)
		close(fd);
	fprint(2, "%s: %ld code points, %ld mapped (%ld in the mapping), ASCII untouched\n", argv[0], nr, changed, nmap);
	exits(nil);
}
