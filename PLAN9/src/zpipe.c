/*
 * zpipe - omi-dom's shared/constraint-pipeline.js in Plan 9 C: the -5D to -1D
 * layers (global, color, delimiter, non-alphanumeric, alphanumeric) over an
 * input's bytes, read as text when they are UTF-8 and as binary when not.
 *	zpipe file...		for each file, as the module gives it: name, kind,
 *				bytes, the five layers' match counts, tokens, admissible
 *	zpipe -b file...	the binary path forced on every file, text or not
 * Text is what the host's TextDecoder (fatal) takes: strict UTF-8, a leading
 * byte order mark dropped. On text the layers count characters in UTF-16
 * units, as JavaScript does (a character past U+FFFF is two), over every
 * line, a line ending at CR LF, LF or CR; on binary they count runs of bytes
 * in the first line only, a line ending at CR LF if any is present, else LF,
 * else CR. The two paths give different answers for the same bytes, and
 * admissible is true whatever the input: the module checks matchCount >= 0.
 */
#include <u.h>
#include <libc.h>

static int
alnum(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* strict UTF-8: the code points of b into cp, their count, or -1 if not text */
static int
utf8(uchar *b, int n, long *cp)
{
	int i, k, m, lo, hi;
	long c;

	for(i = 0, m = 0; i < n; m++){
		c = b[i++];
		lo = 0x80, hi = 0xbf;
		if(c < 0x80)
			k = 0;
		else if(c >= 0xc2 && c <= 0xdf)
			k = 1, c &= 0x1f;
		else if(c >= 0xe0 && c <= 0xef){
			k = 2;
			if(c == 0xe0) lo = 0xa0;
			if(c == 0xed) hi = 0x9f;
			c &= 0x0f;
		}else if(c >= 0xf0 && c <= 0xf4){
			k = 3;
			if(c == 0xf0) lo = 0x90;
			if(c == 0xf4) hi = 0x8f;
			c &= 0x07;
		}else
			return -1;
		for(; k > 0; k--, lo = 0x80, hi = 0xbf){
			if(i >= n || b[i] < lo || b[i] > hi)
				return -1;
			c = c << 6 | (b[i++] & 0x3f);
		}
		cp[m] = c;
	}
	return m;
}

static int
lower(long c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

typedef struct Count Count;
struct Count {
	int	global, color, delim, nonalnum, alnum;
};

static void
text(long *cp, int m, Count *k)
{
	int i, j;

	if(m > 0 && cp[0] == 0xfeff)		/* TextDecoder drops a leading BOM */
		cp++, m--;
	memset(k, 0, sizeof *k);
	k->global = 1;
	for(i = 0; i + 5 <= m; ){		/* /color/gi, matches not overlapping */
		for(j = 0; j < 5 && lower(cp[i+j]) == "color"[j]; j++)
			;
		if(j == 5)
			k->color++, i += 5;
		else
			i++;
	}
	/* split on \r\n|\n|\r, joined again by \n: each line ending is one unit */
	k->delim = 1;
	for(i = 0; i < m; i++){
		if(cp[i] == '\r' || cp[i] == '\n'){
			k->delim++;
			k->nonalnum++;
			if(cp[i] == '\r' && i+1 < m && cp[i+1] == '\n')
				i++;
		}else if(alnum(cp[i]))
			k->alnum++;
		else
			k->nonalnum += cp[i] > 0xffff ? 2 : 1;	/* UTF-16 units */
	}
}

/* splitBytes: the number of parts, and the length of the first */
static int
split(uchar *b, int n, char *d, int dn, int *first)
{
	int i, parts;

	*first = n;
	for(parts = 1, i = 0; i <= n - dn; i++)
		if(memcmp(b+i, d, dn) == 0){
			if(parts == 1)
				*first = i;
			parts++;
			i += dn - 1;
		}
	return parts;
}

static int
runs(uchar *b, int n, int want)
{
	int i, r, in;

	for(r = 0, in = 0, i = 0; i < n; i++){
		if(alnum(b[i]) == want){
			if(!in)
				r++;
			in = 1;
		}else
			in = 0;
	}
	return r;
}

static void
binary(uchar *b, int n, Count *k)
{
	int first;

	memset(k, 0, sizeof *k);
	k->global = 1;
	k->color = 0;		/* binary-skip */
	k->delim = split(b, n, "\r\n", 2, &first);
	if(k->delim == 1)
		k->delim = split(b, n, "\n", 1, &first);
	if(k->delim == 1)
		k->delim = split(b, n, "\r", 1, &first);
	k->nonalnum = runs(b, first, 0);
	k->alnum = runs(b, first, 1);
}

void
main(int argc, char *argv[])
{
	int i, fd, n, m, forced;
	uchar *b;
	long *cp;
	char *name;
	Dir *d;
	Count k;

	forced = 0;
	ARGBEGIN{
	case 'b':
		forced = 1;
		break;
	default:
		sysfatal("usage: zpipe [-b] file...");
	}ARGEND
	for(i = 0; i < argc; i++){
		if((fd = open(argv[i], OREAD)) < 0 || (d = dirfstat(fd)) == nil)
			sysfatal("open %s: %r", argv[i]);
		n = d->length;
		free(d);
		b = malloc(n + 1);
		cp = malloc((n + 1) * sizeof *cp);
		if(readn(fd, b, n) != n)
			sysfatal("read %s: %r", argv[i]);
		close(fd);
		m = forced ? -1 : utf8(b, n, cp);
		if(m >= 0)
			text(cp, m, &k);
		else
			binary(b, n, &k);
		name = strrchr(argv[i], '/');
		print("%s %s %d %d %d %d %d %d %d true\n", name ? name+1 : argv[i], m >= 0 ? "text" : "binary",
			n, k.global, k.color, k.delim, k.nonalnum, k.alnum, k.alnum);
		free(b);
		free(cp);
	}
	exits(nil);
}
