/*
 * zblob - a text punched into cards: Blobs of 65,536 bits (8,192 bytes), its
 * bytes in order, one braille cell per byte (8 dots, U+2800 + byte), the
 * unused tail of the last card blank (0).
 *
 *	zblob text outdir
 *
 * For card NN (00, 01, ...) writes:
 *	NN.bin		the 8,192 bytes, the Blob
 *	NN.braille	the same, 8,192 braille cells, 64 to a line
 *	NN.0b		the same as stars and bars, one 1 or 0 per dot, high bit
 *			first, 512 to a line
 * and checks that both forms read back to NN.bin, and that the cards in
 * order, without their blank tail, are the text byte for byte.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { Card = 8192 };

static uchar *text;
static long ntext;

static void
slurp(char *name)
{
	int fd;
	Dir *d;

	if((fd = open(name, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", name);
	ntext = d->length;
	text = malloc(ntext);
	if(readn(fd, text, ntext) != ntext)
		sysfatal("read %s: %r", name);
	close(fd);
	free(d);
}

static Biobuf*
mk(char *dir, int k, char *ext)
{
	Biobuf *b;
	char *p;

	p = smprint("%s/%.2d.%s", dir, k, ext);
	if((b = Bopen(p, OWRITE)) == nil)
		sysfatal("create %s: %r", p);
	free(p);
	return b;
}

static Biobuf*
op(char *dir, int k, char *ext)
{
	Biobuf *b;
	char *p;

	p = smprint("%s/%.2d.%s", dir, k, ext);
	if((b = Bopen(p, OREAD)) == nil)
		sysfatal("open %s: %r", p);
	free(p);
	return b;
}

void
main(int argc, char *argv[])
{
	uchar card[Card], back[Card];
	Biobuf *b;
	long off, i, j, n;
	int k, ncards, bit;
	long c;

	if(argc != 3)
		sysfatal("usage: zblob text outdir");
	slurp(argv[1]);
	ncards = (ntext + Card - 1) / Card;
	for(k = 0; k < ncards; k++){
		off = (long)k * Card;
		n = ntext - off < Card ? ntext - off : Card;
		memset(card, 0, Card);
		memmove(card, text + off, n);
		b = mk(argv[2], k, "bin");
		Bwrite(b, card, Card);
		Bterm(b);
		b = mk(argv[2], k, "braille");
		for(i = 0; i < Card; i++){
			Bputrune(b, 0x2800 + card[i]);
			if(i % 64 == 63)
				Bputc(b, '\n');
		}
		Bterm(b);
		b = mk(argv[2], k, "0b");
		for(i = 0; i < Card; i++){
			for(bit = 7; bit >= 0; bit--)
				Bputc(b, card[i] >> bit & 1 ? '1' : '0');
			if(i % 64 == 63)
				Bputc(b, '\n');
		}
		Bterm(b);

		/* the braille reads back */
		b = op(argv[2], k, "braille");
		for(i = 0; i < Card && (c = Bgetrune(b)) >= 0; )
			if(c != '\n')
				back[i++] = c - 0x2800;
		Bterm(b);
		if(i != Card || memcmp(back, card, Card) != 0)
			sysfatal("card %.2d: the braille does not read back to the Blob", k);
		/* the stars and bars read back */
		b = op(argv[2], k, "0b");
		memset(back, 0, Card);
		for(j = 0; j < Card * 8 && (c = Bgetc(b)) >= 0; )
			if(c == '0' || c == '1'){
				if(c == '1')
					back[j / 8] |= 0x80 >> (j % 8);
				j++;
			}
		Bterm(b);
		if(j != Card * 8 || memcmp(back, card, Card) != 0)
			sysfatal("card %.2d: the stars and bars do not read back to the Blob", k);
		/* the card holds the text's bytes in order */
		if(memcmp(card, text + off, n) != 0)
			sysfatal("card %.2d: not the text's bytes", k);
		print("card %.2d: %ld bytes of text, %ld blank; braille and stars-and-bars read back\n", k, n, Card - n);
	}
	print("zblob: %ld bytes in %d cards of 65,536 bits; joined in order they are the text\n", ntext, ncards);
	exits(nil);
}
