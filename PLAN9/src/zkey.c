/*
 * zkey - files carried under the six zeroth, in an order both sides agree.
 *
 *	zkey
 *
 * Each zeroth (YHWH, YAHWEH, GOD IS WORD, GOD OF WORD, WORD IS GOD, WORD OF
 * GOD) is reversed and XNORed: key j = the complement of rev(zeroth j). Two
 * sides agree an order of the six keys; file i is carried as its bytes XOR
 * key[order[i mod 6]], the key repeated along the file (a declared choice).
 * The files themselves are never changed. Checks, failing on the first that
 * does not hold: the agreed order brings every file back; then, over the 719
 * other orders, reports how many files each brings back.
 */
#include <u.h>
#include <libc.h>

enum { Nz = 6, Nf = 8, Blk = 64 };

static char *zeroth[Nz] = { "YHWH", "YAHWEH", "GOD IS WORD", "GOD OF WORD", "WORD IS GOD", "WORD OF GOD" };
static char *file[Nf] = { "METATRON", "SOLOMON", "SOLON", "ASABIYAH", "ENOCH", "SPEAKER", "GENESIS", "OBSERVER" };
static uchar key[Nz][Blk];
static int klen[Nz];

static void
carry(char *f, int k, uchar *out)
{
	int i, n;

	n = strlen(f);
	for(i = 0; i < n; i++)
		out[i] = f[i] ^ key[k][i % klen[k]];
}

/* how many files the order o brings back from what the order a sent */
static int
back(int *a, int *o)
{
	uchar sent[Blk], got[Blk];
	int i, j, n, ok;

	for(ok = 0, i = 0; i < Nf; i++){
		n = strlen(file[i]);
		carry(file[i], a[i % Nz], sent);
		for(j = 0; j < n; j++)
			got[j] = sent[j] ^ key[o[i % Nz]][j % klen[o[i % Nz]]];
		if(memcmp(got, file[i], n) == 0)
			ok++;
	}
	return ok;
}

static int perm[720][Nz], np;

static void
gen(int *p, int k)
{
	int i, t;

	if(k == Nz){
		memmove(perm[np++], p, sizeof perm[0]);
		return;
	}
	for(i = k; i < Nz; i++){
		t = p[k]; p[k] = p[i]; p[i] = t;
		gen(p, k + 1);
		t = p[k]; p[k] = p[i]; p[i] = t;
	}
}

void
main(void)
{
	int j, n, i, agreed[Nz] = { 0, 1, 2, 3, 4, 5 }, p[Nz] = { 0, 1, 2, 3, 4, 5 }, hist[Nf + 1];
	char copy[Nf][Blk];

	for(j = 0; j < Nz; j++){
		n = klen[j] = strlen(zeroth[j]);
		for(i = 0; i < n; i++)
			key[j][i] = ~zeroth[j][n - 1 - i];	/* XNOR of the reversed zeroth */
	}
	for(i = 0; i < Nf; i++)
		strcpy(copy[i], file[i]);
	if(back(agreed, agreed) != Nf)
		sysfatal("the agreed order does not bring every file back");
	gen(p, 0);
	memset(hist, 0, sizeof hist);
	for(i = 0; i < np; i++)
		if(memcmp(perm[i], agreed, sizeof agreed) != 0)
			hist[back(agreed, perm[i])]++;
	for(i = 0; i < Nf; i++)
		if(strcmp(copy[i], file[i]) != 0)
			sysfatal("a file was changed");
	print("zkey: the agreed order brings all %d files back; the files are unchanged\n", Nf);
	print("over the %d other orders, files brought back:\n", np - 1);
	for(i = 0; i <= Nf; i++)
		if(hist[i])
			print("  %d of %d: %d orders\n", i, Nf, hist[i]);
	exits(nil);
}
