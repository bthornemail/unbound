/*
 * zcue - a Wirth syntax notation file as WebVTT, to fd 1: one cue per
 * LITERAL, in order, one animation frame each: its second is the frame, not a
 * clock (SPEC-07 I-79). The cue's identifier is
 * the source path and the literal's ordinal; its class names the production
 * that holds it; its text is the LITERAL as written in WSN (a quote inside
 * written twice).
 *	zcue file.wsn source-path
 * No position is written: where a cue sits is the literal's offset in the
 * source buffer, found when the track is read (JS/cue-position.js).
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static void
stamp(Biobuf *o, long s)
{
	Bprint(o, "%02ld:%02ld:%02ld.000", s/3600, s/60%60, s%60);
}

/* cue text: & < > escaped */
static void
text(Biobuf *o, char *s)
{
	for(; *s; s++)
		switch(*s){
		case '&': Bprint(o, "&amp;"); break;
		case '<': Bprint(o, "&lt;"); break;
		case '>': Bprint(o, "&gt;"); break;
		default: Bputc(o, *s);
		}
}

void
main(int argc, char **argv)
{
	Biobuf *b, o;
	char *l, *e, *prod;
	long n;

	if(argc != 3){
		fprint(2, "usage: zcue file.wsn source-path\n");
		exits("usage");
	}
	if((b = Bopen(argv[1], OREAD)) == nil)
		sysfatal("%s: %r", argv[1]);
	Binit(&o, 1, OWRITE);
	Bprint(&o, "WEBVTT - %s\n\n", argv[2]);
	Bprint(&o, "NOTE\nWritten by zcue in Plan 9 from %s.wsn: one cue per LITERAL, one animation frame each.\n", argv[2]);
	Bprint(&o, "No position is written: a cue sits at its literal's offset in the source\n");
	Bprint(&o, "buffer, found when the track is read (exact: row and column of the 65,536;\n");
	Bprint(&o, "or the hypotenuse of the two).\n\n");
	prod = strdup("FILE");
	n = 0;
	while((l = Brdstr(b, '\n', 1)) != nil){
		if(strncmp(l, "  \"", 3) == 0){
			Bprint(&o, "%s#%ld\n", argv[2], n + 1);
			stamp(&o, n);
			Bprint(&o, " --> ");
			stamp(&o, n + 1);
			Bprint(&o, "\n<c.%s>", prod);
			text(&o, l + 2);
			Bprint(&o, "</c>\n\n");
			n++;
		}else if((e = strstr(l, " =")) != nil && e[2] == 0){
			free(prod);
			*e = 0;
			prod = strdup(l);
		}
		free(l);
	}
	Bterm(b);
	Bterm(&o);
	exits(nil);
}
