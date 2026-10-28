/*
 * zhand - a parity handshake between two nodes holding the same blocks in
 * possibly different configurations.
 *
 *	zhand dirA dirB n
 *
 * Each node is a directory (a 9P server) holding blocks 0 .. n-1, one file
 * each. The handshake:
 *	1  set parity: XOR of all blocks on each side; order does not matter, so
 *	   unequal set parities mean different blocks, and there is no sync
 *	2  positional parity: block i XOR block i, for each index mod n; all zero
 *	   means in sync
 *	3  the shortest sequence of rot (each block to the next index), rev and
 *	   swap (blocks 0 and 1) that makes B's positional parity zero; it is
 *	   applied to B's files on B's server, and the parity checked again
 * Exit status "" when the nodes end in sync.
 */
#include <u.h>
#include <libc.h>

enum { Max = 8, Blk = 256, Most = 40320 };

static char A[Max][Blk], B[Max][Blk];
static int n;

static void
rd(char *dir, int i, char *buf)
{
	int fd, m;
	char *p;

	p = smprint("%s/%d", dir, i);
	if((fd = open(p, OREAD)) < 0)
		sysfatal("open %s: %r", p);
	memset(buf, 0, Blk);
	if((m = read(fd, buf, Blk - 1)) < 0)
		sysfatal("read %s: %r", p);
	close(fd);
	free(p);
}

static void
wr(char *dir, int i, char *buf)
{
	int fd;
	char *p;

	p = smprint("%s/%d", dir, i);
	if((fd = create(p, OWRITE, 0644)) < 0)
		sysfatal("create %s: %r", p);
	write(fd, buf, strlen(buf));
	close(fd);
	free(p);
}

/* XOR of all blocks: the set parity */
static void
setparity(char blk[Max][Blk], uchar *p)
{
	int i, j;

	memset(p, 0, Blk);
	for(i = 0; i < n; i++)
		for(j = 0; j < Blk; j++)
			p[j] ^= blk[i][j];
}

/* the indices where A and B (in order perm) differ */
static int
positional(int *perm)
{
	int i, d;

	for(d = 0, i = 0; i < n; i++)
		if(memcmp(A[i], B[perm[i]], Blk) != 0)
			d |= 1 << i;
	return d;
}

/* breadth first over B's configurations, as orders of B's blocks */
static int q[Most][Max], from[Most], op[Most], nq;
static char *opname[] = { "rot", "rev", "swap" };

static int
known(int *p)
{
	int i;

	for(i = 0; i < nq; i++)
		if(memcmp(q[i], p, n * sizeof(int)) == 0)
			return 1;
	return 0;
}

static void
apply(int o, int *p, int *t)
{
	int i;

	for(i = 0; i < n; i++)
		t[i] = o == 0 ? p[(i + 1) % n] : o == 1 ? p[n - 1 - i] : p[i];
	if(o == 2){
		t[0] = p[1];
		t[1] = p[0];
	}
}

void
main(int argc, char *argv[])
{
	uchar pa[Blk], pb[Blk];
	int i, k, o, t[Max], path[64], np;
	char C[Max][Blk];

	if(argc != 4)
		sysfatal("usage: zhand dirA dirB n");
	if(atoi(argv[3]) < 1 || atoi(argv[3]) > Max)
		sysfatal("n must be 1 to %d, not %s", Max, argv[3]);
	n = atoi(argv[3]);
	for(i = 0; i < n; i++){
		rd(argv[1], i, A[i]);
		rd(argv[2], i, B[i]);
	}
	setparity(A, pa);
	setparity(B, pb);
	if(memcmp(pa, pb, Blk) != 0){
		print("1 set parity differs: the nodes hold different blocks; no sync\n");
		exits("different");
	}
	print("1 set parity equal: the same blocks\n");
	for(i = 0; i < n; i++)
		q[0][i] = i;
	nq = 1;
	from[0] = -1;
	for(k = 0; k < nq; k++){
		if(positional(q[k]) == 0)
			break;
		for(o = 0; o < 3; o++){
			if(o == 2 && n < 2)
				continue;
			apply(o, q[k], t);
			if(!known(t)){
				memmove(q[nq], t, sizeof t);
				from[nq] = k;
				op[nq++] = o;
			}
		}
	}
	print("2 positional parity: differs at mask %ux\n", positional(q[0]));
	if(k == nq)
		sysfatal("no sequence of rot, rev and swap syncs the nodes");
	for(np = 0, i = k; from[i] >= 0; i = from[i])
		path[np++] = op[i];
	print("3 handshake:");
	if(np == 0)
		print(" none, already in sync");
	while(np > 0)
		print(" %s", opname[path[--np]]);
	print("\n");
	/* B's server takes the configuration */
	for(i = 0; i < n; i++)
		memmove(C[i], B[q[k][i]], Blk);
	for(i = 0; i < n; i++)
		wr(argv[2], i, C[i]);
	for(i = 0; i < n; i++)
		rd(argv[2], i, B[i]);
	for(i = 0; i < n; i++)
		t[i] = i;
	if(positional(t) != 0)
		sysfatal("after the handshake the positional parity is not zero");
	print("  positional parity now zero: in sync\n");
	exits(nil);
}
