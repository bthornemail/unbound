/*
 * zperm - a word's configurations and indices.
 *
 *	zperm word ...
 *
 * For each word of n distinct letters: the indices are its n positions,
 * counted mod n; the configurations are its n! orderings. Checks, failing on
 * the first that does not hold:
 *	the rotation (each letter to the next index, mod n) cycles in exactly n
 *	the configurations fall into exactly (n-1)! = n!/n rotation cycles
 *	rotation and reversal (rev) together reach 2n of them (n for n <= 2)
 *	adding one swap of two neighbours reaches all n!
 * Every configuration is reached by applying the operations, not listed.
 */
#include <u.h>
#include <libc.h>

enum { Max = 8, Most = 40320 };

static char seen[Most][Max+1];
static int nseen;

static int
has(char *s)
{
	int i;

	for(i = 0; i < nseen; i++)
		if(strcmp(seen[i], s) == 0)
			return 1;
	return 0;
}

static void
rot(char *s, char *t)
{
	int n;

	n = strlen(s);
	memmove(t, s + 1, n - 1);
	t[n-1] = s[0];
	t[n] = 0;
}

static void
rev(char *s, char *t)
{
	int n, i;

	n = strlen(s);
	for(i = 0; i < n; i++)
		t[i] = s[n-1-i];
	t[n] = 0;
}

static void
swap01(char *s, char *t)
{
	strcpy(t, s);
	t[0] = s[1];
	t[1] = s[0];
}

/* everything reachable from w by the operations allowed */
static int
closure(char *w, int userev, int useswap)
{
	char t[Max+1];
	int i;

	nseen = 0;
	strcpy(seen[nseen++], w);
	for(i = 0; i < nseen; i++){
		rot(seen[i], t);
		if(!has(t)) strcpy(seen[nseen++], t);
		if(userev){
			rev(seen[i], t);
			if(!has(t)) strcpy(seen[nseen++], t);
		}
		if(useswap){
			swap01(seen[i], t);
			if(!has(t)) strcpy(seen[nseen++], t);
		}
	}
	return nseen;
}

static int
fact(int n)
{
	return n <= 1 ? 1 : n * fact(n - 1);
}

static void
fail(char *w, char *s)
{
	fprint(2, "zperm: %s: FAIL %s\n", w, s);
	exits("fail");
}

void
main(int argc, char *argv[])
{
	char all[Most][Max+1], t[Max+1];
	int k, n, i, j, nall, ncyc, len, dihedral;
	char *w;

	for(k = 1; k < argc; k++){
		w = argv[k];
		n = strlen(w);
		if(n > Max)
			sysfatal("%s is longer than %d letters", w, Max);
		for(i = 0; i < n; i++)
			for(j = i + 1; j < n; j++)
				if(w[i] == w[j])
					sysfatal("%s repeats a letter", w);
		/* all configurations: rotation, reversal and one swap */
		nall = closure(w, 1, 1);
		memmove(all, seen, nall * sizeof all[0]);
		if(nall != fact(n))
			fail(w, smprint("rotation, rev and a swap reach %d, not %d! = %d", nall, n, fact(n)));
		/* the rotation cycles, each of length n */
		for(ncyc = 0, i = 0; i < nall; i++){
			if(all[i][0] == 0)
				continue;
			len = closure(all[i], 0, 0);
			if(len != n)
				fail(w, smprint("a rotation cycle has length %d, not %d", len, n));
			ncyc++;
			for(j = 0; j < nall; j++)
				if(all[j][0] && has(all[j]))
					all[j][0] = 0;
		}
		if(ncyc != fact(n - 1))
			fail(w, smprint("%d rotation cycles, not (n-1)! = %d", ncyc, fact(n - 1)));
		/* rotation and reversal alone */
		dihedral = closure(w, 1, 0);
		if(dihedral != (n <= 2 ? n : 2 * n))
			fail(w, smprint("rotation and rev reach %d, not %d", dihedral, n <= 2 ? n : 2 * n));
		rev(w, t);
		print("%-5s %d indices (mod %d)  %d! = %d configurations  %d rotation cycles of %d  rotation+rev reach %d  +swap reach %d  rev: %s\n",
			w, n, n, n, fact(n), ncyc, n, dihedral, nall, t);
	}
	exits(nil);
}
