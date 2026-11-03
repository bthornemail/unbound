/*
 * ztranslate - the second Unicode address (the author: "that is just a decoder example, like a translator for all
 * bytes", "all it does its like unicode for all unicode"), after unbound/HTML/slide_ruler.html, the 16-Bit Kernel
 * Slide Ruler: a code point is plane << 16 | offset; its address the Aegean sign U+10107 + plane (the 17 planes, the
 * signs ONE to NINE, then TEN to EIGHTY, in Aegean's own order: the plane's place, not its value), the Braille cell
 * U+2800 + the offset's high byte, and the cell of its low byte. The page shows the low byte's cell alone; with both,
 * every one of the 1,114,112 code points has one address of three glyphs, each a code point itself.
 *	ztranslate		every code point's address, in order, a line each
 *	ztranslate -n		the falsifier, the page's one cell as the address: the high byte read as the blank cell
 *	ztranslate -d file	each line read back: lines, those at their own place (line n + 1 the code point n),
 *				those elsewhere, those refused (not three glyphs, a sign past the 17th, a cell outside the block)
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { Aegean = 0x10107, Braille = 0x2800, Planes = 17 };

static Biobuf out;

static void
put(Rune r)
{
	char b[UTFmax];

	Bwrite(&out, b, runetochar(b, &r));
}

static void
encode(int page)
{
	long n;

	for(n = 0; n < Planes << 16; n++){
		put(Aegean + (n >> 16));
		put(Braille + (page ? 0 : n >> 8 & 255));
		put(Braille + (n & 255));
		Bputc(&out, '\n');
	}
}

static void
decode(char *file)
{
	Biobuf *in;
	char *l, *s;
	Rune r[4];
	int k, n;
	long lines, own, other, refused, v;

	if((in = Bopen(file, OREAD)) == nil)
		sysfatal("open %s: %r", file);
	lines = own = other = refused = 0;
	while((l = Brdstr(in, '\n', 1)) != nil){
		for(k = 0, s = l; *s && k < 4; k++){
			n = chartorune(&r[k], s);
			s += n;
		}
		if(*s || k != 3 || r[0] < Aegean || r[0] >= Aegean + Planes || r[1] < Braille || r[1] > Braille + 255
		|| r[2] < Braille || r[2] > Braille + 255)
			refused++;
		else{
			v = (long)(r[0] - Aegean) << 16 | (r[1] - Braille) << 8 | (r[2] - Braille);
			if(v == lines)
				own++;
			else
				other++;
		}
		lines++;
		free(l);
	}
	Bprint(&out, "%ld lines, %ld at their own place, %ld elsewhere, %ld refused\n", lines, own, other, refused);
}

void
main(int argc, char **argv)
{
	int page;
	char *d;

	page = 0;
	d = nil;
	ARGBEGIN{
	case 'n':
		page = 1;
		break;
	case 'd':
		d = ARGF();
		break;
	default:
		fprint(2, "usage: ztranslate [-n | -d file]\n");
		exits("usage");
	}ARGEND
	Binit(&out, 1, OWRITE);
	if(d != nil)
		decode(d);
	else
		encode(page);
	Bterm(&out);
	exits(nil);
}
