/*
 * zmvpoly - Metaverse Kit's poly tools (~/devops/metaverse-kit tools/mv-poly-basis, mv-poly-closed-config,
 * mv-poly-decompose, mv-poly-signal-project, and their tools/wave28-poly-lib.js) in Plan 9 C, literal (the
 * author's choice): the tools stamp each artifact with a sha256 of itself and name each input by its sha256;
 * here an artifact carries no digest, holds its small inputs whole (a closed config its basis, the
 * constraints' rows, the carrier's bits and the matrix; a decomposition its basis and closed config) and a
 * large one by its path (a signal's source), and validate recomputes what the tools only compared by digest.
 *	zmvpoly basis emit --out F | basis validate --in F
 *	zmvpoly closed build --basis B --constraints C --carrier K --out F | closed validate --in F --basis B
 *	zmvpoly decompose run --basis B --closed C --poly P --out F | decompose validate --in F --basis B --closed C
 *	zmvpoly signal project (--pointer-trace T | --txrx X) [--poly-decomp D] --out F [--out-log L] | signal validate --in F
 *	zmvpoly -n ...	the falsifier: the closed config's matrix rows not rotated by the carrier
 * Each check in the library's order and words; a refusal is ERROR: why, exit 2. Strings are compared and
 * written as their raw JSON literals; a pointer's p_after read as a JSON number or a string of digits.
 */
#include <u.h>
#include <libc.h>

enum { Null, True, False, Num, Str, Arr, Obj };

typedef struct J J;
struct J {
	int	t;
	char	*s;
	J	**v;
	char	**k;
	int	n;
};

static char *p;
static int norotate;
static J*
mk(int t)
{
	J *j;

	j = mallocz(sizeof *j, 1);
	j->t = t;
	return j;
}

static void
ws(void)
{
	while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
}

static char*
lit(void)
{
	char *b;

	b = p++;
	while(*p && *p != '"'){
		if(*p == '\\' && p[1])
			p++;
		p++;
	}
	p++;
	return smprint("%.*s", (int)(p - b), b);
}

static void
add(J *j, char *k, J *v)
{
	j->v = realloc(j->v, (j->n+1) * sizeof(J*));
	j->k = realloc(j->k, (j->n+1) * sizeof(char*));
	j->k[j->n] = k;
	j->v[j->n++] = v;
}

static J*
val(void)
{
	J *j;
	char *b, *k;

	ws();
	switch(*p){
	case '{':
		j = mk(Obj);
		p++; ws();
		if(*p == '}'){ p++; return j; }
		for(;;){
			ws(); k = lit(); ws(); p++;
			add(j, k, val());
			ws();
			if(*p++ == '}')
				return j;
		}
	case '[':
		j = mk(Arr);
		p++; ws();
		if(*p == ']'){ p++; return j; }
		for(;;){
			add(j, nil, val());
			ws();
			if(*p++ == ']')
				return j;
		}
	case '"':
		j = mk(Str);
		j->s = lit();
		return j;
	case 't': p += 4; return mk(True);
	case 'f': p += 5; return mk(False);
	case 'n': p += 4; return mk(Null);
	}
	j = mk(Num);
	b = p;
	while(*p && strchr("-+.0123456789eE", *p))
		p++;
	j->s = smprint("%.*s", (int)(p - b), b);
	return j;
}

static J*
get(J *o, char *k)
{
	int i, n;

	if(o == nil || o->t != Obj)
		return nil;
	n = strlen(k);
	for(i = 0; i < o->n; i++)
		if(strlen(o->k[i]) == n+2 && memcmp(o->k[i]+1, k, n) == 0)
			return o->v[i];
	return nil;
}

static void
put(J *o, char *k, J *v)
{
	add(o, smprint("\"%s\"", k), v);
}

static char *bare(J *j) { return smprint("%.*s", (int)strlen(j->s)-2, j->s+1); }

static J*
str(char *s)
{
	J *j;

	j = mk(Str);
	j->s = smprint("\"%s\"", s);
	return j;
}

static int
keycmp(void *a, void *b)
{
	char *x, *y;
	int m, n, c;

	x = *(char**)a; y = *(char**)b;
	m = strlen(x) - 2; n = strlen(y) - 2;
	c = memcmp(x+1, y+1, m < n ? m : n);
	return c ? c : m - n;
}

typedef struct Buf Buf;
struct Buf { char *s; long n, cap; };

static void
bput(Buf *b, char *s)
{
	long m;

	m = strlen(s);
	if(b->n + m + 1 > b->cap){
		b->cap = (b->n + m + 1) * 2;
		b->s = realloc(b->s, b->cap);
	}
	memmove(b->s + b->n, s, m+1);
	b->n += m;
}

static void
canon(Buf *b, J *j)
{
	char **ks;
	int i, h;

	switch(j->t){
	case Null: bput(b, "null"); return;
	case True: bput(b, "true"); return;
	case False: bput(b, "false"); return;
	case Num: case Str: bput(b, j->s); return;
	case Arr:
		bput(b, "[");
		for(i = 0; i < j->n; i++){
			if(i) bput(b, ",");
			canon(b, j->v[i]);
		}
		bput(b, "]");
		return;
	}
	ks = malloc((j->n+1) * sizeof(char*));
	memmove(ks, j->k, j->n * sizeof(char*));
	qsort(ks, j->n, sizeof(char*), keycmp);
	bput(b, "{");
	for(i = 0; i < j->n; i++){
		if(i) bput(b, ",");
		for(h = 0; j->k[h] != ks[i]; h++)
			;
		bput(b, ks[i]);
		bput(b, ":");
		canon(b, j->v[h]);
	}
	bput(b, "}");
	free(ks);
}

static char*
canonical(J *j)
{
	Buf b;

	memset(&b, 0, sizeof b);
	bput(&b, "");
	canon(&b, j);
	return b.s;
}

static void
fail(char *why)
{
	fprint(2, "%s\n", why);
	exits("2");
}

static char*
slurp(char *file)
{
	int fd;
	Dir *d;
	char *s;

	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		return nil;
	s = mallocz(d->length + 1, 1);
	if(readn(fd, s, d->length) != d->length)
		sysfatal("read %s: %r", file);
	close(fd);
	free(d);
	return s;
}

static void
die(char *msg)
{
	fprint(2, "ERROR: %s\n", msg);
	exits("2");
}

static char *LAYOUT = "wave28.matrix_layout.core6.v0";
static char *DECOMPOSE_ID = "wave28.decompose.gauss_f2.v0";
static char *NORM_ID = "wave28.poly_norm.bitmask_lex.v0";

static J*
readjson(char *file)
{
	char *s;

	if((s = slurp(file)) == nil)
		die(smprint("ENOENT: no such file or directory, open '%s'", file));
	p = s;
	return val();
}

static void
spit(char *file, char *s)
{
	int fd;

	if((fd = create(file, OWRITE, 0666)) < 0)
		sysfatal("create %s: %r", file);
	write(fd, s, strlen(s));
	close(fd);
}

static int isstr(J *j) { return j != nil && j->t == Str; }
static int isarr(J *j) { return j != nil && j->t == Arr; }
static int streq(J *j, char *s) { return isstr(j) && strlen(j->s) == strlen(s) + 2 && memcmp(j->s + 1, s, strlen(s)) == 0; }

/* keyset(obj, expected, ctx): the keys, sorted, as the expected sorted */
static void
keyset(J *o, char **want, int n, char *ctx)
{
	char **got, **w;
	int i;

	if(o == nil || o->t != Obj || o->n != n)
		die(smprint("%s keyset mismatch", ctx));
	got = malloc(n * sizeof(char*));
	w = malloc(n * sizeof(char*));
	for(i = 0; i < n; i++){
		got[i] = o->k[i];
		w[i] = smprint("\"%s\"", want[i]);
	}
	qsort(got, n, sizeof(char*), keycmp);
	qsort(w, n, sizeof(char*), keycmp);
	for(i = 0; i < n; i++)
		if(strcmp(got[i], w[i]) != 0)
			die(smprint("%s keyset mismatch", ctx));
}

static void
membrane(J *v, char *ctx)
{
	int i;

	if(v->t == Arr){
		for(i = 0; i < v->n; i++)
			membrane(v->v[i], smprint("%s[%d]", ctx, i));
		return;
	}
	if(v->t == Obj){
		for(i = 0; i < v->n; i++)
			membrane(v->v[i], smprint("%s.%.*s", ctx, (int)strlen(v->k[i]) - 2, v->k[i] + 1));
		return;
	}
	if(v->t != Str)
		die(smprint("%s violates string membrane", ctx));
}

static int
bits6(J *v)
{
	char *s;
	int i;

	if(!isstr(v) || strlen(v->s) != 8)
		return 0;
	s = v->s + 1;
	for(i = 0; i < 6; i++)
		if(s[i] != '0' && s[i] != '1')
			return 0;
	return 1;
}

static int bin6(char *s) { int v, i; for(v = 0, i = 0; i < 6; i++) v = v * 2 + (s[i] - '0'); return v; }
static char *to6(int m) { char b[7]; int i; for(i = 0; i < 6; i++) b[i] = (m >> (5 - i)) & 1 ? '1' : '0'; b[6] = 0; return strdup(b); }

static int
maskcmp(void *a, void *b)	/* by the mask's 6-bit string, lexicographic */
{
	return strcmp(to6(*(int*)a), to6(*(int*)b));
}

/* parseMonomial and parsePolyCanonical, in the library's checks and words */
static int
monomial(char *t, char *ctx)
{
	char *f, *e;
	int prev, mask, i, idx;

	if(strcmp(t, "1") == 0)
		return 0;
	prev = 0;
	mask = 0;
	for(f = t, i = 0; ; i++){
		e = strchr(f, '*');
		if(e) *e = 0;
		if(!(f[0] == 'x' && f[1] >= '1' && f[1] <= '6' && f[2] == 0))
			die(smprint("%s.factor[%d] invalid variable token", ctx, i));
		idx = f[1] - '0';
		if(idx <= prev)
			die(smprint("%s factors must be strictly increasing", ctx));
		prev = idx;
		mask |= 1 << (idx - 1);
		if(!e)
			break;
		f = e + 1;
	}
	return mask;
}

static int*
polymasks(J *poly, char *ctx, int *n)
{
	char *s, *t, *e;
	int *m, *sorted, i, j;

	if(!isstr(poly) || strlen(poly->s) == 2)
		die(smprint("%s must be non-empty string", ctx));
	s = bare(poly);
	*n = 0;
	m = malloc((strlen(s) + 1) * sizeof(int));
	if(strcmp(s, "0") == 0)
		return m;
	for(t = s; ; t = e + 1){
		e = strchr(t, '+');
		if(e) *e = 0;
		if(*t == 0)
			die(smprint("%s invalid plus separator", ctx));
		if(!e)
			break;
	}
	s = bare(poly);
	for(t = s, i = 0; ; t = e + 1, i++){
		e = strchr(t, '+');
		if(e) *e = 0;
		m[(*n)++] = monomial(t, smprint("%s.term[%d]", ctx, i));
		if(!e)
			break;
	}
	for(i = 0; i < *n; i++)
		for(j = 0; j < i; j++)
			if(m[i] == m[j])
				die(smprint("%s duplicate/canceling term not canonical", ctx));
	sorted = malloc(*n * sizeof(int) + 1);
	memmove(sorted, m, *n * sizeof(int));
	qsort(sorted, *n, sizeof(int), maskcmp);
	for(i = 0; i < *n; i++)
		if(m[i] != sorted[i])
			die(smprint("%s terms not canonical order", ctx));
	return m;
}

static char*
monotext(int m)
{
	char b[32];
	int i, k;

	if(m == 0)
		return "1";
	b[0] = 0;
	for(i = 1, k = 0; i <= 6; i++)
		if(m & (1 << (i - 1))){
			if(k++)
				strcat(b, "*");
			snprint(b + strlen(b), sizeof b - strlen(b), "x%d", i);
		}
	return strdup(b);
}

static char*
polytext(int *m, int n)
{
	int *s, i;
	char *out;

	if(n == 0)
		return "0";
	s = malloc(n * sizeof(int));
	memmove(s, m, n * sizeof(int));
	qsort(s, n, sizeof(int), maskcmp);
	out = strdup("");
	for(i = 0; i < n; i++)
		out = smprint("%s%s%s", out, i ? "+" : "", monotext(s[i]));
	return out;
}

/* validateBasisArtifact, the digest's checks aside; the basis masks */
static int*
basischeck(J *b, int *nm)
{
	static char *keys[] = { "authority", "basis", "basis_order", "field", "v", "variables" };
	J *vars, *bas, *ord;
	int *bm, *om, *sorted, i, j, k, no;

	keyset(b, keys, nelem(keys), "basis");
	if(!streq(get(b, "v"), "wave28.zero_poly_basis.v0")) die("basis version mismatch");
	if(!streq(get(b, "authority"), "advisory")) die("basis authority must be advisory");
	if(!streq(get(b, "field"), "F2")) die("basis field must be F2");
	vars = get(b, "variables"); bas = get(b, "basis"); ord = get(b, "basis_order");
	if(!isarr(vars) || vars->n == 0) die("variables must be non-empty array");
	if(!isarr(bas) || bas->n == 0) die("basis must be non-empty array");
	if(!isarr(ord) || ord->n == 0) die("basis_order must be non-empty array");
	for(i = 0; i < vars->n; i++){
		J *v = vars->v[i];
		if(!isstr(v) || !(strlen(v->s) == 4 && v->s[1] == 'x' && v->s[2] >= '1' && v->s[2] <= '6'))
			die(smprint("variables[%d] invalid", i));
	}
	for(i = 0; i < vars->n; i++)
		for(j = 0; j < i; j++)
			if(strcmp(vars->v[i]->s, vars->v[j]->s) == 0)
				die("variables duplicate");
	bm = malloc((bas->n + 1) * sizeof(int));
	for(i = 0; i < bas->n; i++){
		int *m = polymasks(bas->v[i], smprint("basis[%d]", i), &k);
		if(k != 1) die(smprint("basis[%d] must be monomial in v0", i));
		bm[i] = m[0];
	}
	om = malloc((ord->n + 1) * sizeof(int));
	for(i = 0; i < ord->n; i++){
		int *m = polymasks(ord->v[i], smprint("basis_order[%d]", i), &k);
		if(k != 1) die(smprint("basis_order[%d] must be monomial in v0", i));
		om[i] = m[0];
	}
	no = ord->n;
	if(bas->n != no) die("basis and basis_order length mismatch");
	for(i = 0; i < bas->n; i++)
		for(j = 0; j < i; j++)
			if(bm[i] == bm[j]) die("basis terms duplicate");
	if(strcmp(canonical(bas), canonical(ord)) != 0) die("basis_order must equal basis in v0");
	sorted = malloc(bas->n * sizeof(int));
	memmove(sorted, bm, bas->n * sizeof(int));
	qsort(sorted, bas->n, sizeof(int), maskcmp);
	for(i = 0; i < bas->n; i++)
		if(sorted[i] != bm[i]) die("basis must be sorted by monomial bitmask lexicographic order");
	membrane(b, "basis");
	*nm = bas->n;
	return bm;
}

static void
constraintscheck(J *c)
{
	static char *keys[] = { "authority", "matrix_layout_id", "row_masks", "v" };
	J *r;
	int i;

	keyset(c, keys, nelem(keys), "constraints");
	if(!streq(get(c, "v"), "wave28.constraints.v0")) die("constraints version mismatch");
	if(!streq(get(c, "authority"), "advisory")) die("constraints authority must be advisory");
	if(!streq(get(c, "matrix_layout_id"), LAYOUT)) die("constraints matrix_layout_id mismatch");
	r = get(c, "row_masks");
	if(!isarr(r) || r->n != 6) die("constraints.row_masks must be array[6]");
	for(i = 0; i < 6; i++)
		if(!bits6(r->v[i])) die(smprint("constraints.row_masks[%d] must be 6-bit string", i));
	membrane(c, "constraints");
}

static void
carriercheck(J *c)
{
	static char *keys[] = { "authority", "carrier_bits", "v" };

	keyset(c, keys, nelem(keys), "carrier_state");
	if(!streq(get(c, "v"), "wave28.carrier_state.v0")) die("carrier_state version mismatch");
	if(!streq(get(c, "authority"), "advisory")) die("carrier_state authority must be advisory");
	if(!bits6(get(c, "carrier_bits"))) die("carrier_state.carrier_bits must be 6-bit string");
	membrane(c, "carrier_state");
}

/* rotateBits6, xorBits6, determinantNonZeroF2 */
static int rot6(int n, int s) { s %= 6; return ((n << s) | (n >> (6 - s))) & 63; }

static int
fullrank(int *rows)
{
	int r[6], rank, col, i, pivot, t;

	memmove(r, rows, sizeof r);
	rank = 0;
	for(col = 5; col >= 0; col--){
		for(pivot = -1, i = rank; i < 6; i++)
			if((r[i] >> col) & 1){
				pivot = i;
				break;
			}
		if(pivot < 0)
			continue;
		t = r[rank]; r[rank] = r[pivot]; r[pivot] = t;
		for(i = 0; i < 6; i++)
			if(i != rank && ((r[i] >> col) & 1))
				r[i] ^= r[rank];
		rank++;
	}
	return rank == 6;
}

static J*
arr(void)
{
	return mk(Arr);
}

static void push(J *a, J *v) { add(a, nil, v); }

/* the matrix, from the constraints' rows and the carrier's bits */
static J*
matrix(J *rowmasks, J *carrier, int *nondeg)
{
	J *m, *cols, *rows;
	int r[6], i;

	m = mk(Obj);
	cols = arr();
	for(i = 1; i <= 6; i++)
		push(cols, str(smprint("x%d", i)));
	rows = arr();
	for(i = 0; i < 6; i++){
		r[i] = bin6(rowmasks->v[i]->s + 1) ^ (norotate ? bin6(carrier->s + 1) : rot6(bin6(carrier->s + 1), i));
		push(rows, str(to6(r[i])));
	}
	put(m, "columns", cols);
	put(m, "layout", str(LAYOUT));
	put(m, "rows", rows);
	*nondeg = fullrank(r);
	return m;
}

/* the closed config, literal: its inputs whole and the matrix recomputed */
static void
closedcheck(J *x, J *basis, char *ctx)
{
	static char *keys[] = { "authority", "basis", "carrier_bits", "matrix", "matrix_layout_id", "non_degenerate", "row_masks", "v" };
	J *nd, *rm, *cb;
	int i, nondeg;

	keyset(x, keys, nelem(keys), ctx);
	if(!streq(get(x, "v"), "wave28.closed_config.v0")) die(smprint("%s version mismatch", ctx));
	if(!streq(get(x, "authority"), "advisory")) die(smprint("%s authority must be advisory", ctx));
	if(strcmp(canonical(get(x, "basis")), canonical(get(basis, "basis"))) != 0) die(smprint("%s basis mismatch", ctx));
	if(!streq(get(x, "matrix_layout_id"), LAYOUT)) die(smprint("%s matrix_layout_id mismatch", ctx));
	nd = get(x, "non_degenerate");
	if(!isstr(nd) || !(strcmp(nd->s, "\"0\"") == 0 || strcmp(nd->s, "\"1\"") == 0)) die(smprint("%s.non_degenerate must be bit string 0|1", ctx));
	membrane(x, ctx);
	rm = get(x, "row_masks");
	cb = get(x, "carrier_bits");
	if(!isarr(rm) || rm->n != 6 || !bits6(cb)) die(smprint("%s inputs invalid", ctx));
	for(i = 0; i < 6; i++)
		if(!bits6(rm->v[i])) die(smprint("%s inputs invalid", ctx));
	if(strcmp(canonical(matrix(rm, cb, &nondeg)), canonical(get(x, "matrix"))) != 0) die(smprint("%s matrix mismatch", ctx));
	if(!streq(nd, nondeg ? "1" : "0")) die(smprint("%s non_degenerate mismatch", ctx));
}

/* decompose: the basis terms present, and the rest */
static void
decompose(J *poly, int *bm, int nb, J **coeff, char **residual)
{
	int *im, ni, i, j, *res, nr, in;

	im = polymasks(poly, "input_poly", &ni);
	*coeff = arr();
	for(i = 0; i < nb; i++){
		for(in = 0, j = 0; j < ni; j++)
			if(im[j] == bm[i]) in = 1;
		push(*coeff, str(in ? "1" : "0"));
	}
	res = malloc((ni + 1) * sizeof(int));
	for(nr = 0, j = 0; j < ni; j++){
		for(in = 0, i = 0; i < nb; i++)
			if(im[j] == bm[i]) in = 1;
		if(!in) res[nr++] = im[j];
	}
	*residual = polytext(res, nr);
}

static void
decompcheck(J *x, J *basis, J *closed, int *bm, int nb)
{
	static char *keys[] = { "authority", "basis", "closed_config", "coeff_vector", "input_poly", "norm_id", "residual_poly", "v" };
	J *cv, *coeff, *in;
	char *residual;
	int i, k;

	keyset(x, keys, nelem(keys), "poly_decomp");
	if(!streq(get(x, "v"), "wave28.poly_decomp.v0")) die("poly_decomp version mismatch");
	if(!streq(get(x, "authority"), "advisory")) die("poly_decomp authority must be advisory");
	if(strcmp(canonical(get(x, "basis")), canonical(get(basis, "basis"))) != 0) die("poly_decomp basis mismatch");
	if(strcmp(canonical(get(x, "closed_config")), canonical(closed)) != 0) die("poly_decomp closed_config mismatch");
	if(!streq(get(x, "norm_id"), NORM_ID)) die("poly_decomp norm_id mismatch");
	in = get(x, "input_poly");
	polymasks(in, "poly_decomp.input_poly", &k);
	polymasks(get(x, "residual_poly"), "poly_decomp.residual_poly", &k);
	cv = get(x, "coeff_vector");
	if(!isarr(cv) || cv->n != nb) die("poly_decomp.coeff_vector length mismatch");
	for(i = 0; i < cv->n; i++)
		if(!streq(cv->v[i], "0") && !streq(cv->v[i], "1")) die(smprint("poly_decomp.coeff_vector[%d] invalid", i));
	decompose(in, bm, nb, &coeff, &residual);
	if(strcmp(canonical(coeff), canonical(cv)) != 0) die(smprint("poly_decomp replay mismatch (%s): coeff_vector", DECOMPOSE_ID));
	if(!streq(get(x, "residual_poly"), residual)) die(smprint("poly_decomp replay mismatch (%s): residual_poly", DECOMPOSE_ID));
	membrane(x, "poly_decomp");
}

/* a JS Number() of a p_after: a JSON number, or a string of digits */
static long
number(J *v, int *ok)
{
	char *s, *e;
	long n;

	*ok = 0;
	if(v == nil || (v->t != Num && v->t != Str))
		return 0;
	s = v->t == Str ? bare(v) : v->s;
	if(*s == 0)
		return *ok = 1, 0;
	n = strtol(s, &e, 10);
	if(*e == '.'){
		for(e++; *e == '0'; e++)
			;
	}
	if(*e != 0)
		return 0;
	*ok = 1;
	return n;
}

/* the parity set of a signal's masks, as its polynomial */
static char*
signalpoly(char *file, int pointer, char **err)
{
	char *raw, *l, *e;
	int *masks, n, i, j, ok, cnt, *odd, no;
	long v;
	J *row, *b;

	*err = nil;
	if((raw = slurp(file)) == nil)
		die(smprint("ENOENT: no such file or directory, open '%s'", file));
	if(*raw == 0 || raw[strlen(raw)-1] != '\n'){
		*err = "input NDJSON must end with newline";
		return nil;
	}
	while(*raw && strchr(" \t\r\n", raw[strlen(raw)-1]))
		raw[strlen(raw)-1] = 0;
	masks = malloc((strlen(raw) + 2) * sizeof(int));
	n = 0;
	for(l = raw, i = 1; ; l = e + 1, i++){
		e = strchr(l, '\n');
		if(e) *e = 0;
		p = l;
		ws();
		if(*p != '{' && *p != '['){
			*err = smprint("invalid NDJSON line %d", i);
			return nil;
		}
		row = val();
		if(pointer){
			v = number(get(row, "p_after"), &ok);
			if(!ok || v < 0 || v > 239){
				*err = "pointer trace p_after out of range";
				return nil;
			}
			masks[n++] = v % 64;
		}else{
			b = get(row, "bits");
			if(!bits6(b)){
				*err = "txrx bits must be 6-bit string";
				return nil;
			}
			masks[n++] = bin6(b->s + 1);
		}
		if(!e)
			break;
	}
	odd = malloc((n + 1) * sizeof(int));
	for(no = 0, i = 0; i < n; i++){
		for(cnt = 0, j = 0; j < n; j++)
			if(masks[j] == masks[i]) cnt++;
		for(j = 0; j < no; j++)
			if(odd[j] == masks[i]) break;
		if(cnt % 2 && j == no)
			odd[no++] = masks[i];
	}
	return polytext(odd, no);
}

static void
signalcheck(J *x, int recompute)
{
	static char *keys[] = { "authority", "input_poly", "norm_id", "poly_decomp", "source", "source_type", "v" };
	J *st;
	char *poly, *err;
	int k;

	keyset(x, keys, nelem(keys), "signal_poly_projection");
	if(!streq(get(x, "v"), "wave28.signal_poly_projection.v0")) die("signal_poly_projection version mismatch");
	if(!streq(get(x, "authority"), "advisory")) die("signal_poly_projection authority must be advisory");
	st = get(x, "source_type");
	if(!streq(st, "pointer_sync") && !streq(st, "txrx")) die("signal_poly_projection source_type invalid");
	if(!streq(get(x, "norm_id"), NORM_ID)) die("signal_poly_projection norm_id mismatch");
	polymasks(get(x, "input_poly"), "signal_poly_projection.input_poly", &k);
	membrane(x, "signal_poly_projection");
	if(recompute && isstr(get(x, "source")) && access(bare(get(x, "source")), AEXIST) == 0){
		poly = signalpoly(bare(get(x, "source")), streq(st, "pointer_sync"), &err);
		if(poly == nil || !streq(get(x, "input_poly"), poly)) die("signal_poly_projection input_poly mismatch");
	}
}

/* ── the four tools: their arguments as their parseArgs take them ── */
static char *a_mode, *a_in, *a_out, *a_basis, *a_cons, *a_car, *a_closed, *a_poly, *a_ptrace, *a_txrx, *a_pdec, *a_log;
static int a_help;

static char *flagset;

static void
args(int argc, char **argv, char **modes, int nmodes)
{
	int i, j;

	for(i = 0; i < argc; i++){
		char *a = argv[i];
		for(j = 0; j < nmodes; j++)
			if(strcmp(a, modes[j]) == 0)
				break;
		if(j < nmodes){ a_mode = modes[j]; continue; }
#define F(name, var) if(strstr(flagset, " " name " ") && strcmp(a, name) == 0 && i + 1 < argc && argv[i+1][0]){ var = argv[++i]; continue; }
		F("--in", a_in) F("--out", a_out) F("--basis", a_basis) F("--constraints", a_cons) F("--carrier", a_car) F("--closed", a_closed)
		F("--poly", a_poly) F("--pointer-trace", a_ptrace) F("--txrx", a_txrx) F("--poly-decomp", a_pdec) F("--out-log", a_log)
		if(strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0){ a_help = 1; continue; }
		die(smprint("unknown arg: %s", a));
	}
}

static void
tool_basis(int argc, char **argv)
{
	static char *modes[] = { "emit", "validate" };
	J *b, *l;
	int i, n;

	flagset = " --in --out ";
	args(argc, argv, modes, 2);
	if(a_help || a_mode == nil){
		print("mv-poly-basis emit --out <zero-poly-basis.v0.json>\nmv-poly-basis validate --in <zero-poly-basis.v0.json>\n");
		exits(nil);
	}
	if(strcmp(a_mode, "emit") == 0){
		if(a_out == nil) die("emit requires --out");
		b = mk(Obj);
		put(b, "authority", str("advisory"));
		l = arr();
		for(i = 0; i < 6; i++) push(l, str(monotext(1 << i)));
		put(b, "basis", l);
		put(b, "basis_order", l);
		put(b, "field", str("F2"));
		put(b, "v", str("wave28.zero_poly_basis.v0"));
		l = arr();
		for(i = 1; i <= 6; i++) push(l, str(smprint("x%d", i)));
		put(b, "variables", l);
		basischeck(b, &n);
		spit(a_out, smprint("%s\n", canonical(b)));
		print("ok mv-poly-basis emit\n");
		exits(nil);
	}
	if(a_in == nil) die("validate requires --in");
	basischeck(readjson(a_in), &n);
	print("ok mv-poly-basis validate\n");
	exits(nil);
}

static void
tool_closed(int argc, char **argv)
{
	static char *modes[] = { "build", "validate" };
	J *b, *c, *k, *x;
	int n, nondeg;

	flagset = " --basis --constraints --carrier --in --out ";
	args(argc, argv, modes, 2);
	if(a_help || a_mode == nil){
		print("mv-poly-closed-config build --basis <basis.json> --constraints <constraints.json> --carrier <carrier.json> --out <closed.json>\nmv-poly-closed-config validate --in <closed.json> --basis <basis.json>\n");
		exits(nil);
	}
	if(strcmp(a_mode, "build") == 0){
		if(!a_basis || !a_cons || !a_car || !a_out) die("build requires --basis --constraints --carrier --out");
		b = readjson(a_basis); c = readjson(a_cons); k = readjson(a_car);
		basischeck(b, &n);
		constraintscheck(c);
		carriercheck(k);
		x = mk(Obj);
		put(x, "authority", str("advisory"));
		put(x, "basis", get(b, "basis"));
		put(x, "carrier_bits", get(k, "carrier_bits"));
		put(x, "matrix", matrix(get(c, "row_masks"), get(k, "carrier_bits"), &nondeg));
		put(x, "matrix_layout_id", str(LAYOUT));
		put(x, "non_degenerate", str(nondeg ? "1" : "0"));
		put(x, "row_masks", get(c, "row_masks"));
		put(x, "v", str("wave28.closed_config.v0"));
		closedcheck(x, b, "closed_config");
		spit(a_out, smprint("%s\n", canonical(x)));
		print("ok mv-poly-closed-config build non_degenerate=%d\n", nondeg);
		exits(nil);
	}
	if(!a_in || !a_basis) die("validate requires --in --basis");
	b = readjson(a_basis); x = readjson(a_in);
	basischeck(b, &n);
	closedcheck(x, b, "closed_config");
	print("ok mv-poly-closed-config validate\n");
	exits(nil);
}

static void
tool_decompose(int argc, char **argv)
{
	static char *modes[] = { "run", "validate" };
	J *b, *cl, *x, *coeff;
	char *residual;
	int *bm, nb;

	flagset = " --basis --closed --poly --in --out ";
	args(argc, argv, modes, 2);
	if(a_help || a_mode == nil){
		print("mv-poly-decompose run --basis <basis.json> --closed <closed.json> --poly '<canonical-poly>' --out <poly-decomp.json>\nmv-poly-decompose validate --in <poly-decomp.json> --basis <basis.json> --closed <closed.json>\n");
		exits(nil);
	}
	if(strcmp(a_mode, "run") == 0){
		if(!a_basis || !a_closed || !a_poly || !a_out) die("run requires --basis --closed --poly --out");
		b = readjson(a_basis); cl = readjson(a_closed);
		bm = basischeck(b, &nb);
		closedcheck(cl, b, "closed_config_ref");
		decompose(str(a_poly), bm, nb, &coeff, &residual);
		x = mk(Obj);
		put(x, "authority", str("advisory"));
		put(x, "basis", get(b, "basis"));
		put(x, "closed_config", cl);
		put(x, "coeff_vector", coeff);
		put(x, "input_poly", str(a_poly));
		put(x, "norm_id", str(NORM_ID));
		put(x, "residual_poly", str(residual));
		put(x, "v", str("wave28.poly_decomp.v0"));
		decompcheck(x, b, cl, bm, nb);
		spit(a_out, smprint("%s\n", canonical(x)));
		print("ok mv-poly-decompose run decompose_id=%s\n", DECOMPOSE_ID);
		exits(nil);
	}
	if(!a_in || !a_basis || !a_closed) die("validate requires --in --basis --closed");
	b = readjson(a_basis); cl = readjson(a_closed); x = readjson(a_in);
	bm = basischeck(b, &nb);
	closedcheck(cl, b, "closed_config_ref");
	decompcheck(x, b, cl, bm, nb);
	print("ok mv-poly-decompose validate decompose_id=%s\n", DECOMPOSE_ID);
	exits(nil);
}

static void
tool_signal(int argc, char **argv)
{
	static char *modes[] = { "project", "validate" };
	J *x;
	char *poly, *err, *src;

	flagset = " --pointer-trace --txrx --poly-decomp --out-log --in --out ";
	args(argc, argv, modes, 2);
	if(a_help || a_mode == nil){
		print("mv-poly-signal-project project (--pointer-trace <trace.ndjson> | --txrx <txrx.ndjson>) [--poly-decomp <poly-decomp.json>] --out <signal-poly.json> [--out-log <signal-poly.ndjson>]\nmv-poly-signal-project validate --in <signal-poly.json>\n");
		exits(nil);
	}
	if(strcmp(a_mode, "project") == 0){
		if(!a_out) die("project requires --out");
		if((a_ptrace != nil) + (a_txrx != nil) != 1) die("project requires exactly one source: --pointer-trace or --txrx");
		src = a_ptrace ? a_ptrace : a_txrx;
		if((poly = signalpoly(src, a_ptrace != nil, &err)) == nil) die(err);
		x = mk(Obj);
		put(x, "authority", str("advisory"));
		put(x, "input_poly", str(poly));
		put(x, "norm_id", str(NORM_ID));
		put(x, "poly_decomp", a_pdec ? readjson(a_pdec) : str(""));
		put(x, "source", str(src));
		put(x, "source_type", str(a_ptrace ? "pointer_sync" : "txrx"));
		put(x, "v", str("wave28.signal_poly_projection.v0"));
		signalcheck(x, 0);
		spit(a_out, smprint("%s\n", canonical(x)));
		if(a_log) spit(a_log, smprint("%s\n", canonical(x)));
		print("ok mv-poly-signal-project project\n");
		exits(nil);
	}
	if(!a_in) die("validate requires --in");
	signalcheck(readjson(a_in), 1);
	print("ok mv-poly-signal-project validate\n");
	exits(nil);
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'n':
		norotate = 1;
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zmvpoly [-n] basis|closed|decompose|signal args");
	if(strcmp(argv[0], "basis") == 0) tool_basis(argc-1, argv+1);
	if(strcmp(argv[0], "closed") == 0) tool_closed(argc-1, argv+1);
	if(strcmp(argv[0], "decompose") == 0) tool_decompose(argc-1, argv+1);
	if(strcmp(argv[0], "signal") == 0) tool_signal(argc-1, argv+1);
	sysfatal("unknown tool %s", argv[0]);
}
