/*
 * zconv - wsn.awk, back.awk and split.awk in Plan 9 C (plan9-runtime: ports are
 * Plan 9 C programs, not awk), each giving what the awk gave, byte for byte.
 *	zconv wsn TOP file	one file to Wirth syntax notation, meaning kept line
 *				for line: every non-blank line one LITERAL, in order
 *				("" for a quote inside); a line "// ── title ──" starts
 *				a production named by the title's letters, upper case
 *				(SECTION if none, a suffix A, B ... if taken); the lines
 *				before the first are HEAD; TOP is the sequence of all
 *	zconv back file		the LITERALs of such a file, one per line: the
 *				source's non-blank lines, if nothing was added or lost
 *	zconv lines file	the file's non-blank lines, as awk '!/^[ \t]*$/' gives them:
 *				what back must give
 *	zconv split DIR file	the roll-up (I-122.ts) cut into its records: the
 *				comment lines just above a line record("I-...", through
 *				the next line that starts with }, each to DIR/I-....ts;
 *				every other line, in order, to DIR/I-122.ts
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static char **line;
static int nline;

static void
readlines(char *file)
{
	Biobuf *b;
	char *l;
	int max;

	if((b = Bopen(file, OREAD)) == nil)
		sysfatal("open %s: %r", file);
	max = 0;
	while((l = Brdstr(b, '\n', 1)) != nil){
		if(nline == max){
			max = max ? 2*max : 1024;
			line = realloc(line, max * sizeof *line);
		}
		line[nline++] = l;
	}
	Bterm(b);
}

static int
blank(char *s)
{
	for(; *s; s++)
		if(*s != ' ' && *s != '\t')
			return 0;
	return 1;
}

static int
istitle(char *s)
{
	return strncmp(s, "// ── ", strlen("// ── ")) == 0;
}

/* ---- wsn ---- */
static char *top;
static char **used;
static int nused;

static int
isused(char *s)
{
	int i;

	for(i = 0; i < nused; i++)
		if(strcmp(used[i], s) == 0)
			return 1;
	return 0;
}

static char*
suffix(int k)
{
	char r[16], *p;

	p = r + sizeof r - 1;
	*p = 0;
	while(k > 0){
		*--p = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[(k - 1) % 26];
		k = (k - 1) / 26;
	}
	return strdup(p);
}

static char*
ident(char *t)
{
	char *s, *p, *t2;
	int k;

	s = malloc(strlen(t) + 1);
	for(p = s; *t; t++)
		if((*t >= 'A' && *t <= 'Z') || (*t >= 'a' && *t <= 'z'))
			*p++ = *t >= 'a' ? *t - 32 : *t;
	*p = 0;
	if(*s == 0)
		s = strdup("SECTION");
	if(strcmp(s, top) == 0 || isused(s)){
		k = 0;
		do
			t2 = smprint("%s%s", s, suffix(++k));
		while(isused(t2) || strcmp(t2, top) == 0);
		s = t2;
	}
	used = realloc(used, (nused + 1) * sizeof *used);
	used[nused++] = s;
	return s;
}

static char*
quote(char *s)
{
	char *q, *p;

	q = malloc(2*strlen(s) + 1);
	for(p = q; *s; s++){
		*p++ = *s;
		if(*s == '"')
			*p++ = '"';
	}
	*p = 0;
	return q;
}

static int
wsn(void)
{
	char **name;
	int *first, n, i, j;
	Biobuf o;

	name = malloc((nline + 1) * sizeof *name);
	first = malloc((nline + 2) * sizeof *first);
	for(n = 0, i = 0; i < nline; i++){
		if(blank(line[i]))
			continue;
		if(istitle(line[i]) || n == 0){
			name[n] = istitle(line[i]) ? ident(line[i]) : ident("HEAD");
			first[n++] = i;
		}
	}
	Binit(&o, 1, OWRITE);
	if(n == 0){
		Bprint(&o, "%s = \"\" .\n", top);
		Bterm(&o);
		return 1;
	}
	first[n] = nline;
	Bprint(&o, "%s =", top);
	for(i = 0; i < n; i++)
		Bprint(&o, " %s", name[i]);
	Bprint(&o, " .\n");
	for(i = 0; i < n; i++){
		Bprint(&o, "\n%s =\n", name[i]);
		for(j = first[i]; j < first[i+1]; j++){
			if(blank(line[j]))
				continue;
			Bprint(&o, "  \"%s\"\n", quote(line[j]));
		}
		Bprint(&o, "  .\n");
	}
	Bterm(&o);
	return 0;
}

/* ---- back ---- */
static void
back(void)
{
	Biobuf o;
	char *s, *p, *q;
	int i, n;

	Binit(&o, 1, OWRITE);
	for(i = 0; i < nline; i++){
		if(strncmp(line[i], "  \"", 3) != 0)
			continue;
		n = strlen(line[i]);
		s = malloc(n);
		memmove(s, line[i] + 3, n - 4 > 0 ? n - 4 : 0);
		s[n - 4 > 0 ? n - 4 : 0] = 0;
		for(p = q = s; *p; p++){	/* "" to " */
			*q++ = *p;
			if(p[0] == '"' && p[1] == '"')
				p++;
		}
		*q = 0;
		Bprint(&o, "%s\n", s);
		free(s);
	}
	Bterm(&o);
}

/* ---- split ---- */
static void
split(char *dir)
{
	char **who, *id, *f, *last;
	int i, j, s, e;
	int fd;

	who = mallocz(nline * sizeof *who, 1);
	for(i = 0; i < nline; i++){
		if(strncmp(line[i], "record(\"", 8) != 0)
			continue;
		id = strdup(line[i] + 8);
		if((f = strchr(id, '"')) != nil)
			*f = 0;
		for(s = i; s > 0 && strncmp(line[s-1], "//", 2) == 0; s--)
			;
		for(e = i; e < nline - 1 && line[e][0] != '}'; e++)
			;
		for(j = s; j <= e; j++)
			who[j] = id;
	}
	last = nil;
	fd = -1;
	for(i = 0; i < nline; i++){
		f = who[i] ? smprint("%s/%s.ts", dir, who[i]) : smprint("%s/I-122.ts", dir);
		if(last == nil || strcmp(f, last) != 0){
			if(fd >= 0)
				close(fd);
			if((fd = open(f, OWRITE)) < 0 && (fd = create(f, OWRITE, 0644)) < 0)
				sysfatal("create %s: %r", f);
			seek(fd, 0, 2);	/* >> */
			last = f;
		}
		fprint(fd, "%s\n", line[i]);
	}
	if(fd >= 0)
		close(fd);
}

void
main(int argc, char *argv[])
{
	if(argc == 4 && strcmp(argv[1], "wsn") == 0){
		top = argv[2];
		readlines(argv[3]);
		exits(wsn() ? "empty" : nil);
	}
	if(argc == 3 && strcmp(argv[1], "back") == 0){
		readlines(argv[2]);
		back();
		exits(nil);
	}
	if(argc == 3 && strcmp(argv[1], "lines") == 0){
		Biobuf o;
		int i;

		readlines(argv[2]);
		Binit(&o, 1, OWRITE);
		for(i = 0; i < nline; i++)
			if(!blank(line[i]))
				Bprint(&o, "%s\n", line[i]);
		Bterm(&o);
		exits(nil);
	}
	if(argc == 4 && strcmp(argv[1], "split") == 0){
		readlines(argv[3]);
		split(argv[2]);
		exits(nil);
	}
	sysfatal("usage: zconv wsn TOP file | back file | lines file | split DIR file");
}
