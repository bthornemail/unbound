/*
 * zborder - boxedPINE's space/src/border.ts and trace.ts in Plan 9 C: the
 * proof of PIXELATE, the author's "the ability to change a svg from the
 * margins". A name of up to 8 capitals or digits (an /A-Z0-9/ tag) and up to
 * 4 cues (each 0 to 15) are written as invisible one-unit cells in an SVG's
 * border, present = 1: each side carries two letters of the name in row 1
 * (16 cells) and a cue in row 4 (4 cells). The image as drawn does not change;
 * XOR 0x20 reads the same name in lowercase. The trace XORs each file's place
 * (its first cue) into a 65,536-byte buffer at its name's pairs, and the
 * place's complement into a second: an intact set of 16 traces to zero, and a
 * changed margin shows at exactly its pairs.
 *	zborder write NAME cues file	the file with that border (cues: 1,11,15 or -)
 *	zborder read [key] file		the name and cues read back, the name XOR key
 *	zborder clear file		the image as drawn, its border removed
 *	zborder trace file...		the indices the files mark, and the bytes there
 */
#include <u.h>
#include <libc.h>

static char*
slurp(char *file)
{
	int fd;
	long n, m;
	char *s;
	Dir *d;

	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", file);
	n = d->length;
	free(d);
	s = malloc(n + 1);
	if((m = readn(fd, s, n)) != n)
		sysfatal("read %s: %ld of %ld", file, m, n);
	s[n] = 0;
	close(fd);
	return s;
}

static int
space(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* where /<\/svg>\s*$/ matches, or -1 */
static long
svgend(char *s)
{
	long n;

	n = strlen(s);
	while(n > 0 && space(s[n-1]))
		n--;
	if(n < 6 || strncmp(s + n - 6, "</svg>", 6) != 0)
		return -1;
	return n - 6;
}

/* clearBorder: /<g id="border">.*?<\/g><\/svg>\s*$/s to </svg>\n */
static char*
clear(char *s)
{
	char *g, *out;
	long e, n;

	n = strlen(s);
	while(n > 0 && space(s[n-1]))
		n--;
	g = strstr(s, "<g id=\"border\">");
	if(g == nil || n < 10 || strncmp(s + n - 10, "</g></svg>", 10) != 0 || g - s > n - 10)
		return strdup(s);
	e = g - s;
	out = malloc(e + 8);
	memmove(out, s, e);
	strcpy(out + e, "</svg>\n");
	return out;
}

/* toFixed(2): the nearer hundredth, the larger on a tie */
static char*
fixed2(double v)
{
	double f;
	vlong n;

	f = floor(v * 100);
	n = f;
	if(v * 100 - f >= 0.5)
		n++;
	return smprint("%s%lld.%.2lld", n < 0 ? "-" : "", (n < 0 ? -n : n) / 100, (n < 0 ? -n : n) % 100);
}

/* the first /\sNAME="([\d.]+)"/ in s */
static int
attr(char *s, char *name, double *v)
{
	char *p, *q;
	int n;

	n = strlen(name);
	for(p = s; (p = strstr(p, name)) != nil; p++){
		if(p == s || !space(p[-1]) || p[n] != '=' || p[n+1] != '"')
			continue;
		for(q = p + n + 2; (*q >= '0' && *q <= '9') || *q == '.'; q++)
			;
		if(q > p + n + 2 && *q == '"'){
			*v = atof(p + n + 2);
			return 1;
		}
	}
	return 0;
}

static void
viewbox(char *s, double *w, double *h)
{
	char *p, *f[8], buf[256], *e;
	int n;

	if((p = strstr(s, "viewBox=\"")) != nil){
		p += 9;
		if((e = strchr(p, '"')) == nil || e - p >= sizeof buf)
			sysfatal("viewBox");
		memmove(buf, p, e - p);
		buf[e - p] = 0;
		if((n = tokenize(buf, f, nelem(f))) == 4){
			*w = atof(f[2]);
			*h = atof(f[3]);
			return;
		}
	}
	if(attr(s, "width", w) && attr(s, "height", h))
		return;
	sysfatal("the SVG has no viewBox or width and height");
}

static void
cell(Fmt *o, double W, double H, int side, int row, int i, int n, char *kind)
{
	double along, at, x, y;

	along = side % 2 == 0 ? W : H;
	at = 12 + (i * (along - 24)) / n;
	switch(side){
	case 0: x = at; y = row; break;
	case 1: x = W - 1 - row; y = at; break;
	case 2: x = at; y = H - 1 - row; break;
	default: x = row; y = at; break;
	}
	fmtprint(o, "<rect data-%s=\"%d.%d\" x=\"%s\" y=\"%s\" width=\"1\" height=\"1\" fill=\"#000\" fill-opacity=\"0\"/>",
		kind, side, i, fixed2(x), fixed2(y));
}

static int
istag(char *s)
{
	int n;

	for(n = 0; s[n]; n++)
		if(!((s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= '0' && s[n] <= '9')))
			return 0;
	return n >= 1 && n <= 8;
}

static char*
writeborder(char *svg, char *name, int *cues, int ncues)
{
	Fmt o;
	char *c, bytes[8];
	double W, H;
	long e;
	int side, i, word;

	if(!istag(name))
		sysfatal("the name is an /A-Z0-9/ tag of 1 to 8: %s", name);
	viewbox(svg, &W, &H);
	memset(bytes, 0, sizeof bytes);
	memmove(bytes, name, strlen(name));
	c = clear(svg);
	if((e = svgend(c)) < 0)
		sysfatal("no </svg>");
	fmtstrinit(&o);
	fmtprint(&o, "%.*s<g id=\"border\">", (int)e, c);
	for(side = 0; side < 4; side++){
		word = (uchar)bytes[side*2] << 8 | (uchar)bytes[side*2+1];
		for(i = 0; i < 16; i++)
			if(word >> (15 - i) & 1)
				cell(&o, W, H, side, 1, i, 16, "name");
		if(side < ncues){
			fmtprint(&o, "<rect data-cued=\"%d\" width=\"0\" height=\"0\" fill-opacity=\"0\"/>", side);
			for(i = 0; i < 4; i++)
				if(cues[side] >> (3 - i) & 1)
					cell(&o, W, H, side, 4, i, 4, "cue");
		}
	}
	fmtprint(&o, "</g></svg>\n");
	return fmtstrflush(&o);
}

/* readBorder: every data-name, data-cue and data-cued cell, wherever it is */
static void
readborder(char *svg, int key, char *name, int *cues, int *ncues)
{
	int words[4], bits[4], cued[4], s, i, n, b;
	char *p;

	memset(words, 0, sizeof words);
	memset(bits, 0, sizeof bits);
	memset(cued, 0, sizeof cued);
	for(p = svg; (p = strstr(p, "data-name=\"")) != nil; p++)
		if(p[11] >= '0' && p[11] <= '9' && p[12] == '.'){
			s = p[11] - '0';
			i = atoi(p + 13);
			if(s < 4)
				words[s] |= 1 << (15 - i);
		}
	for(p = svg; (p = strstr(p, "data-cue=\"")) != nil; p++)
		if(p[10] >= '0' && p[10] <= '9' && p[11] == '.' && p[12] >= '0' && p[12] <= '9' && p[13] == '"'){
			s = p[10] - '0';
			if(s < 4)
				bits[s] |= 1 << (3 - (p[12] - '0'));
		}
	for(p = svg; (p = strstr(p, "data-cued=\"")) != nil; p++)
		if(p[11] >= '0' && p[11] <= '9' && p[12] == '"' && p[11] - '0' < 4)
			cued[p[11] - '0'] = 1;
	for(n = 0, s = 0; s < 4; s++){
		b = words[s] >> 8;
		if(b != 0)
			name[n++] = b ^ key;
		b = words[s] & 0xFF;
		if(b != 0)
			name[n++] = b ^ key;
	}
	name[n] = 0;
	for(*ncues = 0, s = 0; s < 4; s++)
		if(cued[s])
			cues[(*ncues)++] = bits[s];
}

static uchar txor[65536], txnor[65536];

void
main(int argc, char *argv[])
{
	char name[16], *s, *f[8];
	int cues[4], ncues, key, i, j, n, place, p, first;

	if(argc < 3)
		sysfatal("usage: zborder write NAME cues file | read [key] file | clear file | trace file...");
	if(strcmp(argv[1], "write") == 0 && argc == 5){
		ncues = 0;
		if(strcmp(argv[3], "-") != 0){
			s = strdup(argv[3]);
			ncues = getfields(s, f, 4, 0, ",");
			for(i = 0; i < ncues; i++)
				if((cues[i] = atoi(f[i])) < 0 || cues[i] > 15)
					sysfatal("up to 4 cues, each 0 to 15");
		}
		print("%s", writeborder(slurp(argv[4]), argv[2], cues, ncues));
	}else if(strcmp(argv[1], "read") == 0){
		key = argc == 4 ? strtol(argv[2], nil, 0) : 0;
		readborder(slurp(argv[argc-1]), key, name, cues, &ncues);
		print("%s ", name);
		for(i = 0; i < ncues; i++)
			print("%s%d", i ? "," : "", cues[i]);
		print("\n");
	}else if(strcmp(argv[1], "clear") == 0)
		print("%s", clear(slurp(argv[2])));
	else if(strcmp(argv[1], "trace") == 0){
		for(i = 2; i < argc; i++){
			readborder(slurp(argv[i]), 0, name, cues, &ncues);
			place = ncues > 0 ? cues[0] : 0;
			n = strlen(name);
			for(j = 0; j < n; j += 2){
				p = (uchar)name[j] << 8 | (j+1 < n ? (uchar)name[j+1] : 0);	/* a last odd letter pairs with 0 */
				txor[p] ^= place;
				txnor[p] ^= 15 - place;
			}
		}
		for(first = 1, p = 0; p < 65536; p++)
			if(txor[p] || txnor[p]){
				print("%s%x:%d:%d", first ? "" : ",", p, txor[p], txnor[p]);
				first = 0;
			}
		print("%s\n", first ? "none" : "");
	}else
		sysfatal("usage: zborder write NAME cues file | read [key] file | clear file | trace file...");
	exits(nil);
}
