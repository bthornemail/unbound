/*
 * zver - prolog-resolve's compatibleVersion in Plan 9 C, since rc has no
 * arithmetic: for each line of the file, yes when the version is within
 * 1.36.0 to 2.0.0, no when not.
 *	zver file
 * The version is read as the module reads it: everything from the first - on
 * dropped, the rest split at each dot, the first three fields read by
 * parseInt (leading white space, an optional +, the digits that follow, none
 * is 0; a - is already gone), missing fields 0, compared field by field.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static void
parse(char *s, int v[3])
{
	char *p, *f[64];
	int n, i, x;

	if((p = strchr(s, '-')) != nil)
		*p = 0;
	n = getfields(s, f, nelem(f), 0, ".");
	for(i = 0; i < 3; i++){
		v[i] = 0;
		if(i >= n)
			continue;
		p = f[i];
		while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '\v' || *p == '\f')
			p++;
		if(*p == '+')
			p++;
		for(x = 0; *p >= '0' && *p <= '9'; p++)
			x = x*10 + *p - '0';
		v[i] = x;
	}
}

static int
cmp(int a[3], int b[3])
{
	int i;

	for(i = 0; i < 3; i++)
		if(a[i] != b[i])
			return a[i] - b[i];
	return 0;
}

void
main(int argc, char *argv[])
{
	Biobuf *b;
	char *l, lo[] = "1.36.0", hi[] = "2.0.0";
	int v[3], vlo[3], vhi[3];

	if(argc != 2 || (b = Bopen(argv[1], OREAD)) == nil)
		sysfatal("usage: zver file");
	parse(lo, vlo);
	parse(hi, vhi);
	while((l = Brdstr(b, '\n', 1)) != nil){
		parse(l, v);
		print("%s\n", cmp(v, vlo) >= 0 && cmp(v, vhi) <= 0 ? "yes" : "no");
		free(l);
	}
	exits(nil);
}
