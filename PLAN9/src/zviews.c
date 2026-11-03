/*
 * zviews - a page's tree by wsn/html.rules, in Plan 9 C beside omi-types' OMI.View (the same reading, a line of the
 * outline a piece): its runes, its pieces (rules 1 and 2), its tree (rule 3), written as OMI.View's outline, two
 * spaces a level, the production, a space, the piece's bytes (a line feed written \n, a backslash \\), an element
 * left open marked "OPEN" at its children's level. The goldens are OMI.View's outlines (in/views/gen.mjs).
 *	zviews page		the page's outline
 *	zviews -w page		the second reading set beside it: the tree's words as Plan 9's libhtml renders them (a
 *				word a line, split at white space and U+00A0, entities read), a textarea's raw text
 *				as text, none within a table, a bullet before each list item; -T, -B, -A each leave
 *				one of the three out, the falsifier
 *	zviews -s file		any text's words, the same split (libhtml's text, as htmlfs serves it)
 * A rune is the bytes its lead byte calls for, as OMI.View reads: under 0x80 alone, 110xxxxx one more, 1110xxxx
 * two, 11110xxx three, any other alone; its cast in html.wsn's alphabet a letter's lower case, a digit and each of
 * < > / = " ' - _ : . # & ; ! itself, white space a space, any other rune "~".
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { Comment, Doctype, Starttag, Endtag, Text, Rawtext };
static char *prod[] = { "COMMENT", "DOCTYPE", "STARTTAG", "ENDTAG", "TEXT", "RAWTEXT" };
static char *rawnames[] = { "script", "style", "title", "textarea", nil };
static char *voidnames[] = { "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param",
	"source", "track", "wbr", nil };
static char *linames[] = { "li", nil };
static char *tablenames[] = { "table", nil };
static char *textareanames[] = { "textarea", nil };

typedef struct Piece Piece;
struct Piece
{
	int	kind;
	long	a, b;		/* its runes */
	long	na, nb;		/* a tag's name, cast */
};

static uchar *buf;
static long len, nr, *start;
static uchar *castv;
static Piece *ps;
static long np, cap, pi;
static long *opened, nopen;
static Biobuf out;

static int
cast(uchar c)
{
	if(c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	if(c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || strchr("<>/=\"'-_:.#&;!", c) != nil && c != 0)
		return c;
	if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f')
		return ' ';
	return '~';
}

static int
namechar(int c)
{
	return c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c == '-' || c == '_' || c == ':' || c == '.';
}

/* rune k is the one byte c */
static int
is(long k, int c)
{
	return k < nr && start[k+1] - start[k] == 1 && buf[start[k]] == c;
}

/* the runes from k begin with these symbols */
static int
begins(char *pat, long k)
{
	for(; *pat; pat++, k++)
		if(k >= nr || castv[k] != (uchar)*pat)
			return 0;
	return 1;
}

static int
named(long na, long nb, char **names)
{
	for(; *names; names++)
		if(strlen(*names) == nb - na && memcmp(castv + na, *names, nb - na) == 0)
			return 1;
	return 0;
}

static int
samename(Piece *p, Piece *q)
{
	return p->nb - p->na == q->nb - q->na && memcmp(castv + p->na, castv + q->na, p->nb - p->na) == 0;
}

static int
selfclosed(long a, long b)
{
	return b - a >= 2 && is(b - 2, '/');
}

/* a tag at k: its kind, the rune after its ">", its name; a quote opens a value when the nearest rune before it,
 * spaces aside, is "=" */
static int
tagat(long k, int *kind, long *end, long *na, long *nb)
{
	long c, j;
	int aftereq, q;

	if(!is(k, '<') || k + 1 >= nr)
		return 0;
	if(is(k + 1, '/')){
		*kind = Endtag;
		c = k + 2;
	}else{
		*kind = Starttag;
		c = k + 1;
	}
	if(c >= nr || !(castv[c] >= 'a' && castv[c] <= 'z'))
		return 0;
	for(j = c; j < nr && namechar(castv[j]); j++)
		;
	*na = c;
	*nb = j;
	aftereq = 0;
	for(;;){
		if(j >= nr)
			return 0;
		if(is(j, '>')){
			*end = j + 1;
			return 1;
		}
		if(is(j, '"') || is(j, '\'')){
			if(aftereq){
				q = castv[j];
				for(j++; j < nr && castv[j] != q; j++)
					;
				if(j >= nr)
					return 0;
			}
			j++;
			aftereq = 0;
			continue;
		}
		if(castv[j] != ' ')
			aftereq = is(j, '=');
		j++;
	}
}

static int
pieceat(long k)
{
	int kind;
	long e, na, nb;

	return begins("<!--", k) || begins("<!doctype", k) || tagat(k, &kind, &e, &na, &nb);
}

static void
add(int kind, long a, long b, long na, long nb)
{
	if(np == cap){
		cap = cap ? 2 * cap : 1024;
		ps = realloc(ps, cap * sizeof ps[0]);
		if(ps == nil)
			sysfatal("realloc: %r");
	}
	ps[np].kind = kind;
	ps[np].a = a;
	ps[np].b = b;
	ps[np].na = na;
	ps[np].nb = nb;
	np++;
}

/* the first rune from k where the symbols begin, past them; -1 when they are not found */
static long
upto(char *pat, long k)
{
	for(; k < nr; k++)
		if(begins(pat, k))
			return k + strlen(pat);
	return -1;
}

static void
pieces(void)
{
	long k, e, j, na, nb, i;
	int kind;

	k = 0;
	while(k < nr){
		if(begins("<!--", k)){
			if((e = upto("-->", k + 4)) < 0){
				add(Text, k, nr, 0, 0);
				break;
			}
			add(Comment, k, e, 0, 0);
			k = e;
		}else if(begins("<!doctype", k)){
			if((e = upto(">", k)) < 0){
				add(Doctype, k, nr, 0, 0);
				break;
			}
			add(Doctype, k, e, 0, 0);
			k = e;
		}else if(tagat(k, &kind, &e, &na, &nb)){
			add(kind, k, e, na, nb);
			if(kind == Starttag && named(na, nb, rawnames) && !selfclosed(k, e)){
				/* raw text, to the first "</" and its name, any case, or the end */
				for(j = e; j < nr; j++){
					if(!begins("</", j) || j + 2 + (nb - na) > nr)
						continue;
					for(i = 0; i < nb - na && castv[j+2+i] == castv[na+i]; i++)
						;
					if(i == nb - na)
						break;
				}
				if(j > e)
					add(Rawtext, e, j, 0, 0);
				e = j;
			}
			k = e;
		}else{
			for(j = k + 1; j < nr && !(is(j, '<') && pieceat(j)); j++)
				;
			add(Text, k, j, 0, 0);
			k = j;
		}
	}
}

static void
line(int d, char *s, Piece *p)
{
	long i;

	for(i = 0; i < d; i++)
		Bprint(&out, "  ");
	Bprint(&out, "%s", s);
	if(p == nil){
		Bputc(&out, '\n');
		return;
	}
	Bputc(&out, ' ');
	for(i = start[p->a]; i < start[p->b]; i++)
		if(buf[i] == '\n')
			Bprint(&out, "\\n");
		else if(buf[i] == '\\')
			Bprint(&out, "\\\\");
		else
			Bputc(&out, buf[i]);
	Bputc(&out, '\n');
}

static int
member(Piece *p)
{
	long i;

	for(i = 0; i < nopen; i++)
		if(samename(p, &ps[opened[i]]))
			return 1;
	return 0;
}

/* the pieces from pi at depth d, to the end or an end tag of an open element's name */
static void
forest(int d)
{
	Piece *p;
	long e;

	while(pi < np){
		p = &ps[pi];
		if(p->kind == Endtag && member(p))
			return;
		if(p->kind == Starttag && !named(p->na, p->nb, voidnames) && !selfclosed(p->a, p->b)){
			e = pi++;
			line(d, prod[p->kind], p);
			opened[nopen++] = e;
			forest(d + 1);
			nopen--;
			if(pi < np && ps[pi].kind == Endtag && samename(&ps[pi], &ps[e]))
				line(d, prod[Endtag], &ps[pi++]);
			else
				line(d + 1, "OPEN", nil);
			continue;
		}
		line(d, prod[p->kind], p);
		pi++;
	}
}

/* words: split at white space and U+00A0, a word a line */
static int inword;

static void
wordbyte(int c)
{
	if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'){
		if(inword)
			Bputc(&out, '\n');
		inword = 0;
		return;
	}
	Bputc(&out, c);
	inword = 1;
}

static void
split(uchar *s, long n)
{
	long i;

	for(i = 0; i < n; i++){
		if(s[i] == 0xC2 && i + 1 < n && s[i+1] == 0xA0){
			if(inword)
				Bputc(&out, '\n');
			inword = 0;
			i++;
			continue;
		}
		wordbyte(s[i]);
	}
}

static void
endword(void)
{
	if(inword)
		Bputc(&out, '\n');
	inword = 0;
}

/* a text piece's words, its entities read: &#n; and &gt; &lt; &amp; &nbsp; &times; &copy;, any other as written */
static char *ents[] = { "gt", ">", "lt", "<", "amp", "&", "nbsp", "\xC2\xA0", "times", "\xC3\x97", "copy", "\xC2\xA9", nil };

static void
textwords(long a, long b)
{
	uchar *s, *e, *t;
	char r[UTFmax];
	char **x;
	Rune v;
	long m;

	s = buf + a;
	e = buf + b;
	while(s < e){
		if(*s == '&'){
			t = s + 1;
			if(t < e && *t == '#'){
				for(t++, v = 0; t < e && *t >= '0' && *t <= '9' && v <= Runemax; t++)
					v = v * 10 + *t - '0';
				if(t > s + 2 && t < e && *t == ';' && v <= Runemax){
					m = runetochar(r, &v);
					split((uchar*)r, m);
					s = t + 1;
					continue;
				}
			}else{
				for(; t < e && *t >= 'a' && *t <= 'z'; t++)
					;
				if(t > s + 1 && t < e && *t == ';')
					for(x = ents; *x; x += 2)
						if(strlen(x[0]) == t - s - 1 && memcmp(x[0], s + 1, t - s - 1) == 0){
							split((uchar*)x[1], strlen(x[1]));
							s = t + 1;
							goto next;
						}
			}
		}
		split(s, 1);
		s++;
	next:;
	}
	endword();
}

/* the tree's words as libhtml renders them: a text piece's words, a textarea's raw text as text, none within a
 * table (libhtml's table item holds its cells), and a bullet "•" before each list item's */
static int notable, nobullet, notextarea;

static void
words(void)
{
	Piece *p;
	long i, k, t;

	for(i = 0; i < np; i++){
		p = &ps[i];
		if(p->kind == Starttag){
			if(!nobullet && named(p->na, p->nb, linames))
				Bprint(&out, "\xE2\x80\xA2\n");
			if(!named(p->na, p->nb, voidnames) && !selfclosed(p->a, p->b))
				opened[nopen++] = i;
		}else if(p->kind == Endtag){
			for(k = nopen - 1; k >= 0 && !samename(p, &ps[opened[k]]); k--)
				;
			if(k >= 0)
				nopen = k;
		}else if(p->kind == Text || p->kind == Rawtext && !notextarea && named(ps[i-1].na, ps[i-1].nb, textareanames)){
			for(k = 0, t = 0; !notable && k < nopen; k++)
				if(named(ps[opened[k]].na, ps[opened[k]].nb, tablenames))
					t = 1;
			if(!t)
				textwords(start[p->a], start[p->b]);
		}
	}
}

void
main(int argc, char **argv)
{
	int fd, n, w, s;
	long k, i;

	w = s = 0;
	ARGBEGIN{
	case 'w':
		w = 1;
		break;
	case 's':
		s = 1;
		break;
	case 'T':
		notable = 1;
		break;
	case 'B':
		nobullet = 1;
		break;
	case 'A':
		notextarea = 1;
		break;
	default:
		fprint(2, "usage: zviews [-w [-TBA] | -s] file\n");
		exits("usage");
	}ARGEND
	if(argc != 1){
		fprint(2, "usage: zviews [-w [-TBA] | -s] file\n");
		exits("usage");
	}
	if((fd = open(argv[0], OREAD)) < 0)
		sysfatal("open: %r");
	cap = 0;
	for(;;){
		if(len == cap){
			cap = cap ? 2 * cap : 65536;
			if((buf = realloc(buf, cap)) == nil)
				sysfatal("realloc: %r");
		}
		if((n = read(fd, buf + len, cap - len)) <= 0)
			break;
		len += n;
	}
	close(fd);
	Binit(&out, 1, OWRITE);
	if(s){
		split(buf, len);
		endword();
		Bterm(&out);
		exits(nil);
	}
	start = malloc((len + 1) * sizeof start[0]);
	castv = malloc(len + 1);
	if(start == nil || castv == nil)
		sysfatal("malloc: %r");
	for(k = 0, i = 0; i < len; k++){
		start[k] = i;
		castv[k] = '~';
		if(buf[i] < 0x80 || buf[i] >= 0x80 && buf[i] < 0xC0 || buf[i] >= 0xF8){
			castv[k] = cast(buf[i]);
			n = 1;
		}else if(buf[i] < 0xE0)
			n = 2;
		else if(buf[i] < 0xF0)
			n = 3;
		else
			n = 4;
		i += n;
		if(i > len)
			i = len;
	}
	nr = k;
	start[nr] = len;
	cap = 0;
	pieces();
	opened = malloc((np + 1) * sizeof opened[0]);
	if(opened == nil)
		sysfatal("malloc: %r");
	pi = 0;
	if(w)
		words();
	else
		forest(0);
	Bterm(&out);
	exits(nil);
}
