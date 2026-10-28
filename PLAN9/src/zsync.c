/*
 * zsync - XOR files together, byte for byte; a byte past the end of a
 * shorter file is 0.
 *
 *	zsync out in ...
 *
 * With the four vertex files as input it writes their parity P; with P and
 * the three files that remain it writes the fourth back.
 */
#include <u.h>
#include <libc.h>

enum { Max = 1<<20 };

static uchar acc[Max];

void
main(int argc, char *argv[])
{
	uchar buf[8192];
	long n, off, len, i;
	int fd, k;

	if(argc < 3){
		fprint(2, "usage: zsync out in ...\n");
		exits("usage");
	}
	len = 0;
	for(k = 2; k < argc; k++){
		if((fd = open(argv[k], OREAD)) < 0)
			sysfatal("open %s: %r", argv[k]);
		for(off = 0; (n = read(fd, buf, sizeof buf)) > 0; off += n){
			if(off + n > Max)
				sysfatal("%s is longer than %d bytes", argv[k], Max);
			for(i = 0; i < n; i++)
				acc[off + i] ^= buf[i];
		}
		if(n < 0)
			sysfatal("read %s: %r", argv[k]);
		close(fd);
		if(off > len)
			len = off;
	}
	if((fd = create(argv[1], OWRITE, 0644)) < 0)
		sysfatal("create %s: %r", argv[1]);
	if(write(fd, acc, len) != len)
		sysfatal("write %s: %r", argv[1]);
	close(fd);
	exits(nil);
}
