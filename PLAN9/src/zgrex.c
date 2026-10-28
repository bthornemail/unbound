/*
 * zgrex - the six constraints of omi-dom's shared/regex-constraints.js that
 * use backreferences, in Plan 9 C: Plan 9's regexp(2) has none, and DEFLECT,
 * REFLECT, INFLECT and MNEMONIC repeat an unbounded group (the copy language,
 * not context-free), so no WSN grammar can hold them. AXIS and PALINDROME
 * repeat only two-digit groups: finite, but tens of thousands of alternatives.
 *	zgrex <tokens	for each token, six bits in G's order:
 *			DEFLECT REFLECT INFLECT AXIS MNEMONIC PALINDROME
 * Each backreference allows one split only (the middle colon, or the colons),
 * so each is checked directly:
 *	DEFLECT	^([^".]+):\1$		REFLECT	^([".]+):\1$
 *	INFLECT	^([".]+):([".]+):\2:\1$
 *	AXIS	^(\d\d)[A-Za-z_](\d\d):\2[0-9+\-]\1$
 *	MNEMONIC	^(\d\d)([A-Z_]?[a-z_]+)(\d\d):\3\2\1$
 *	PALINDROME	^(\d\d)[A-Za-z_\-](\d\d):\2[0-9_\-]\1$
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static int
all(char *s, int n, char *set, int in)
{
	int i;

	for(i = 0; i < n; i++)
		if((strchr(set, s[i]) != nil) != in)
			return 0;
	return 1;
}

static int
digit(int c)
{
	return c >= '0' && c <= '9';
}

static int
letter(int c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* ^(X):\1$ with X over set (in) or outside it (!in), nonempty */
static int
twice(char *s, char *set, int in)
{
	int n, k;

	n = strlen(s);
	if(n < 3 || n % 2 == 0)
		return 0;
	k = n / 2;
	return s[k] == ':' && all(s, k, set, in) && memcmp(s, s+k+1, k) == 0;
}

static int
inflect(char *s)
{
	char *p[5], *c;
	int n;

	c = strdup(s);
	n = getfields(c, p, nelem(p), 0, ":");
	n = n == 4 && p[0][0] && p[1][0]
		&& all(p[0], strlen(p[0]), "\".", 1) && all(p[1], strlen(p[1]), "\".", 1)
		&& strcmp(p[2], p[1]) == 0 && strcmp(p[3], p[0]) == 0;
	free(c);
	return n;
}

/* dd m dd : dd e dd, the right pair repeating the left as \2 then \1 */
static int
mirror(char *s, int (*mid)(int), char *end)
{
	if(strlen(s) != 11 || s[5] != ':')
		return 0;
	if(!digit(s[0]) || !digit(s[1]) || !digit(s[3]) || !digit(s[4]))
		return 0;
	return mid(s[2]) && strchr(end, s[8]) != nil && s[8] != 0
		&& s[6] == s[3] && s[7] == s[4] && s[9] == s[0] && s[10] == s[1];
}

static int axismid(int c) { return letter(c) || c == '_'; }
static int palmid(int c) { return letter(c) || c == '_' || c == '-'; }

static int
mnemonic(char *s)
{
	char *colon;
	int n, w, i;

	n = strlen(s);
	if((colon = strchr(s, ':')) == nil)
		return 0;
	w = colon - s - 4;			/* the word between the two digit pairs */
	if(w < 1 || n != 2*(w + 4) + 1)
		return 0;
	if(!digit(s[0]) || !digit(s[1]) || !digit(colon[-2]) || !digit(colon[-1]))
		return 0;
	/* [A-Z_]? then [a-z_]+ */
	i = 2;
	if(s[i] >= 'A' && s[i] <= 'Z')
		i++;
	else if(s[i] == '_' && w > 1)
		i++;		/* an optional leading _ ; a lone _ is the [a-z_]+ part */
	for(; i < 2 + w; i++)
		if(!((s[i] >= 'a' && s[i] <= 'z') || s[i] == '_'))
			return 0;
	if(s[2] >= 'A' && s[2] <= 'Z' && w < 2)
		return 0;
	/* the right side: \3 \2 \1 */
	return memcmp(colon+1, colon-2, 2) == 0 && memcmp(colon+3, s+2, w) == 0 && memcmp(colon+3+w, s, 2) == 0;
}

void
main(void)
{
	Biobuf in, out;
	char *l;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	while((l = Brdstr(&in, '\n', 1)) != nil){
		Bprint(&out, "%d%d%d%d%d%d\n",
			twice(l, "\".", 0), twice(l, "\".", 1), inflect(l),
			mirror(l, axismid, "0123456789+-"), mnemonic(l),
			mirror(l, palmid, "0123456789_-"));
		free(l);
	}
	Bterm(&out);
	exits(nil);
}
