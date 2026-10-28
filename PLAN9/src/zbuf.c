/*
 * zbuf - frame a body as a ZIGUAROTH buffer, in Plan 9 C.
 *
 *	zbuf path content-type description <body >buffer
 *
 * The same buffer ZIGUAROTH's _ARCHIVE/Expressions/buffer.js wraps: the
 * 12 bytes "ZIGUA-ROTH: ", the path of provenance, the MIME fields of
 * rule B3, CR LF CR LF, then the body byte for byte. The header is ASCII;
 * a rune of U+0080 or above in the path or description is written U+XXXX (B2).
 */
#include <u.h>
#include <libc.h>

static void
ascii(char *s)
{
	Rune r;

	while(*s){
		s += chartorune(&r, s);
		if(r < 0x80)
			print("%c", r);
		else
			print("U+%04uX", r);
	}
}

void
main(int argc, char *argv[])
{
	char *body;
	long n, m, cap;

	if(argc != 4){
		fprint(2, "usage: zbuf path content-type description <body >buffer\n");
		exits("usage");
	}
	cap = 8192;
	body = malloc(cap);
	n = 0;
	while((m = read(0, body+n, cap-n)) > 0){
		n += m;
		if(n == cap){
			cap *= 2;
			body = realloc(body, cap);
		}
		if(body == nil)
			sysfatal("out of memory");
	}
	if(m < 0)
		sysfatal("read: %r");
	print("ZIGUA-ROTH: ");
	ascii(argv[1]);
	print("\r\nMIME-Version: 1.0\r\nContent-Type: %s\r\nContent-Transfer-Encoding: binary\r\nContent-Length: %ld\r\nContent-Description: ", argv[2], n);
	ascii(argv[3]);
	print("\r\n\r\n");
	if(write(1, body, n) != n)
		sysfatal("write: %r");
	exits(nil);
}
