/*
 * svgswitch - toggle the text payload of an SVG, never its frame; a port of
 * boxedPINE scripts/svg-switch.
 *
 *	svgswitch [--rev] [--enc] [--tr A B] [file]
 *
 * Only the content of <text> and <tspan> changes: --enc rot13s its letters,
 * --tr maps A to B rune for rune, --rev reverses it. Tags, attributes and
 * everything outside text pass through as they are. Each switch is its own
 * inverse.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static int dorev, doenc;
static Rune *s1, *s2;
static Biobuf out;

static Rune
map(Rune c)
{
	int i, n;

	if(doenc){
		if(c >= 'a' && c <= 'z')
			c = 'a' + (c - 'a' + 13) % 26;
		else if(c >= 'A' && c <= 'Z')
			c = 'A' + (c - 'A' + 13) % 26;
	}
	if(s1 != nil){
		n = runestrlen(s2);
		for(i = 0; s1[i]; i++)
			if(s1[i] == c)
				return i < n ? s2[i] : s2[n-1];
	}
	return c;
}

static void
xform(Rune *t, int n)
{
	int i;

	for(i = 0; i < n; i++)
		t[i] = map(t[i]);
	if(dorev)
		for(i = n - 1; i >= 0; i--)
			Bputrune(&out, t[i]);
	else
		for(i = 0; i < n; i++)
			Bputrune(&out, t[i]);
}

/* the tag t (from < to >) opens or closes a text run */
static int
istext(Rune *t, int n, int close)
{
	static char *names[] = { "text", "tspan" };
	int i, k, j;

	j = close ? 2 : 1;
	if(close && (n < 2 || t[1] != '/'))
		return 0;
	for(k = 0; k < nelem(names); k++){
		for(i = 0; names[k][i] && j + i < n && t[j+i] == names[k][i]; i++)
			;
		if(names[k][i] == 0 && j + i < n && strchr(" \t\n>", t[j+i]) != nil)
			return close || !(n >= 2 && t[n-2] == '/');
	}
	return 0;
}

void
main(int argc, char *argv[])
{
	Biobuf *in, bin;
	Rune *buf, *tag, *txt;
	long c, n, cap, nt, nx, i;
	int intag, intext;

	argc--; argv++;	/* not ARGBEGIN: it would read --rev as the flags - r e v */
	for(; argc > 0 && strncmp(argv[0], "--", 2) == 0; argc--, argv++){
		if(strcmp(argv[0], "--rev") == 0)
			dorev = 1;
		else if(strcmp(argv[0], "--enc") == 0)
			doenc = 1;
		else if(strcmp(argv[0], "--tr") == 0 && argc >= 3){
			s1 = runesmprint("%s", argv[1]);
			s2 = runesmprint("%s", argv[2]);
			argc -= 2; argv += 2;
		}else
			sysfatal("unknown option %s", argv[0]);
	}
	if(argc > 0){
		if((in = Bopen(argv[0], OREAD)) == nil)
			sysfatal("open %s: %r", argv[0]);
	}else{
		Binit(&bin, 0, OREAD);
		in = &bin;
	}
	Binit(&out, 1, OWRITE);
	cap = 4096;
	buf = malloc(cap * sizeof buf[0]);
	n = 0;
	while((c = Bgetrune(in)) >= 0){
		if(n == cap && (buf = realloc(buf, (cap *= 2) * sizeof buf[0])) == nil)
			sysfatal("out of memory");
		buf[n++] = c;
	}
	tag = malloc((n + 1) * sizeof tag[0]);
	txt = malloc((n + 1) * sizeof txt[0]);
	intag = intext = 0;
	nt = nx = 0;
	for(i = 0; i < n; i++){
		c = buf[i];
		if(intag){
			tag[nt++] = c;
			if(c == '>'){
				if(istext(tag, nt, 1))
					intext = 0;
				else if(istext(tag, nt, 0))
					intext = 1;
				for(c = 0; c < nt; c++)
					Bputrune(&out, tag[c]);
				intag = 0;
				nt = 0;
			}
			continue;
		}
		if(c == '<'){
			if(intext && nx > 0){
				xform(txt, nx);
				nx = 0;
			}
			intag = 1;
			tag[nt++] = c;
			continue;
		}
		if(intext)
			txt[nx++] = c;
		else
			Bputrune(&out, c);
	}
	if(nx > 0)
		xform(txt, nx);
	Bterm(&out);
	exits(nil);
}
