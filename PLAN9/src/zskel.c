/*
 * zskel - the converted Wirth syntax notation reduced to its skeleton, a
 * regular expression (wsn/skeleton.re, one alternative a line: the top
 * production, a blank line, a production's name, a literal with "" for a quote
 * inside, the closing dot), applied with regexp(2) to every line of every file.
 *	zskel skeleton.re file...	each file with lines off the skeleton, and the count
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>

enum { MAXRE = 32 };

void
main(int argc, char *argv[])
{
	Reprog *re[MAXRE];
	Biobuf *b;
	char *l;
	int nre, i, k, off, bad, files;

	if(argc < 3)
		sysfatal("usage: zskel skeleton.re file...");
	if((b = Bopen(argv[1], OREAD)) == nil)
		sysfatal("open %s: %r", argv[1]);
	for(nre = 0; nre < MAXRE && (l = Brdstr(b, '\n', 1)) != nil; nre++)
		if((re[nre] = regcomp(l)) == nil)
			sysfatal("regcomp %s", l);
	Bterm(b);
	for(bad = 0, files = 0, i = 2; i < argc; i++, files++){
		if((b = Bopen(argv[i], OREAD)) == nil)
			sysfatal("open %s: %r", argv[i]);
		for(off = 0; (l = Brdstr(b, '\n', 1)) != nil; free(l)){
			for(k = 0; k < nre; k++)
				if(regexec(re[k], l, nil, 0))
					break;
			off += k == nre;
		}
		Bterm(b);
		if(off){
			print("%s: %d lines off the skeleton\n", argv[i], off);
			bad++;
		}
	}
	print("%d files, %d with lines off the skeleton\n", files, bad);
	exits(bad ? "off" : nil);
}
