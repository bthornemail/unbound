/*
 * hello - the sentence the way Kernighan's B tutorial (1972) printed
 * "hello, world": external words of four characters each, put out in order.
 *
 *	main( ) {
 *	    extrn a, b, c;
 *	    putchar(a); putchar(b); putchar(c); putchar('!*n');
 *	}
 *	a 'hell';  b 'o, w';  c 'orld';
 *
 * Here a, b, c are R0, R1, R2, each a 32-bit word packed with four
 * characters, and FLAG declares which constant b holds: IS or OF.
 */
#include <u.h>
#include <libc.h>

static u32int a, b, c;

/* four characters packed into a word, first character in the high byte */
static u32int
word(char *s)
{
	u32int w;
	int i;

	for(w = 0, i = 0; i < 4; i++)
		w = w << 8 | (s[i] ? (uchar)s[i] : 0);
	return w;
}

/* B's putchar: the word's characters, high byte first, stopping at a zero byte */
static void
putchar(u32int w)
{
	int i;
	char ch;

	for(i = 3; i >= 0; i--)
		if((ch = w >> 8*i) != 0)
			print("%c", ch);
}

static void
run(char *sa, char *sIS, char *sOF, char *sc, int FLAG)
{
	a = word(sa);
	b = word(FLAG ? sIS : sOF);
	c = word(sc);
	putchar(a); putchar(b); putchar(c); print("\n");
}

void
main(void)
{
	u32int x;

	run("GOD ", "IS W", "OF W", "ORD", 1);
	run("GOD ", "IS W", "OF W", "ORD", 0);
	run("WORD", " IS ", " OF ", "GOD", 1);
	run("WORD", " IS ", " OF ", "GOD", 0);

	x = word("IS W") ^ word("OF W");
	print("b XOR: %.8ux   b XNOR: %.8ux   a and c XOR: %.8ux %.8ux\n", x, ~x, word("GOD ") ^ word("GOD "), word("ORD") ^ word("ORD"));
	if(x != 0x06150000)
		sysfatal("the two b words do not differ by 06 15 00 00");
	x = word(" IS ") ^ word(" OF ");
	if(x != 0x00061500)
		sysfatal("the two b words of WORD IS|OF GOD do not differ by 00 06 15 00");
	print("hello: FLAG chooses b; the whole IS/OF difference is in b, 06 15 (00 06 15 00 in WORD IS|OF GOD)\n");
	exits(nil);
}
