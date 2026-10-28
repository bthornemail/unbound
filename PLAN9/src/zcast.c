/*
 * zcast - a zeroth's type cast, as Wirth syntax notation, to fd 1.
 *	zcast ZEROTH
 * The feature pipeline of boxedPINE's src/terms.mjs (classify): each letter
 * A-Z of the zeroth is a consonant, kept upper, or a vowel (AEIOU), lowered
 * 0x20; a digit stays. Each class that holds a character becomes a production
 * of its characters, in the order they first appear; a class that holds none
 * gets no production, so it matches nothing: Wirth syntax notation leaves letter
 * and character undefined, for the language defined to settle
 * (https://en.wikipedia.org/wiki/Wirth_syntax_notation).
 * With each class, the stack element's held step (wsn/stack.wsn): a character
 * taken in is the one given out, then the element is empty again.
 *	YHWH:	CONSONANT = "Y" | "H" | "W" .	no VOWEL
 *	YAHWEH:	the same, and VOWEL = "a" | "e" .
 */
#include <u.h>
#include <libc.h>

static char *name[] = { "CONSONANT", "VOWEL", "DIGIT" };
static char held[3][64];
static int nheld[3];

static int
classof(int *c)
{
	if(*c >= '0' && *c <= '9')
		return 2;
	if(*c >= 'a' && *c <= 'z')
		*c -= 0x20;
	if(*c < 'A' || *c > 'Z')
		return -1;
	if(strchr("AEIOU", *c) != nil){
		*c += 0x20;
		return 1;
	}
	return 0;
}

void
main(int argc, char **argv)
{
	char *s;
	int c, k, i;

	if(argc != 2){
		fprint(2, "usage: zcast ZEROTH\n");
		exits("usage");
	}
	for(s = argv[1]; *s; s++){
		c = *s;
		if((k = classof(&c)) < 0)
			continue;
		for(i = 0; i < nheld[k]; i++)
			if(held[k][i] == c)
				break;
		if(i == nheld[k] && nheld[k] < sizeof held[k])
			held[k][nheld[k]++] = c;
	}
	for(k = 0; k < 3; k++){
		if(nheld[k] == 0)
			continue;
		print("%s =", name[k]);
		for(i = 0; i < nheld[k]; i++)
			print("%s \"%c\"", i ? " |" : "", held[k][i]);
		print(" .\n");
	}
	print("LETTER = CONSONANT | VOWEL .\n");
	for(k = 0; k < 3; k++){
		if(nheld[k] == 0)
			continue;
		print("HELD%s =", name[k]);
		for(i = 0; i < nheld[k]; i++)
			print("%s \"%c\" [ \"out\" \"!\" \"%c\" EMPTY ]", i ? " |" : "", held[k][i], held[k][i]);
		print(" .\n");
	}
	exits(nil);
}
