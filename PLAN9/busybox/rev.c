/*
 * rev - reverse each line, by runes.
 *
 *	rev [file ...]
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static void
rev(Biobuf *in, Biobuf *out)
{
	Rune *r;
	long c, n, cap;

	cap = 256;
	r = malloc(cap * sizeof r[0]);
	n = 0;
	while((c = Bgetrune(in)) >= 0){
		if(c == '\n'){
			while(n > 0)
				Bputrune(out, r[--n]);
			Bputrune(out, '\n');
			continue;
		}
		if(n == cap){
			cap *= 2;
			if((r = realloc(r, cap * sizeof r[0])) == nil)
				sysfatal("out of memory");
		}
		r[n++] = c;
	}
	while(n > 0)
		Bputrune(out, r[--n]);
	free(r);
}

void
main(int argc, char *argv[])
{
	Biobuf in, out, *f;
	int i;

	Binit(&out, 1, OWRITE);
	if(argc == 1){
		Binit(&in, 0, OREAD);
		rev(&in, &out);
	}
	for(i = 1; i < argc; i++){
		if((f = Bopen(argv[i], OREAD)) == nil)
			sysfatal("open %s: %r", argv[i]);
		rev(f, &out);
		Bterm(f);
	}
	Bterm(&out);
	exits(nil);
}
