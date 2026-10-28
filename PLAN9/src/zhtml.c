/*
 * zhtml - read an HTML template through Plan 9's own libhtml, adding nothing.
 *
 *	zhtml file
 *
 * parsehtml(2) turns the document into its item list and Docinfo;
 * validitems checks the list; printitems prints it as libhtml describes it.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <html.h>

/* libhtml calls these and leaves them to the program */
void*
emalloc(ulong n)
{
	void *p;

	if((p = mallocz(n, 1)) == nil)
		sysfatal("out of memory");
	return p;
}

void*
erealloc(void *p, ulong n)
{
	if((p = realloc(p, n)) == nil)
		sysfatal("out of memory");
	return p;
}

void
main(int argc, char *argv[])
{
	uchar *data;
	long n, m, cap;
	int fd;
	Docinfo *di;
	Item *items, *it;
	int count[Ispacertag+1];
	static char *tagname[] = {
	[Itexttag] "Itext", [Iruletag] "Irule", [Iimagetag] "Iimage", [Iformfieldtag] "Iformfield",
	[Itabletag] "Itable", [Ifloattag] "Ifloat", [Ispacertag] "Ispacer",
	};

	if(argc != 2){
		fprint(2, "usage: zhtml file\n");
		exits("usage");
	}
	if((fd = open(argv[1], OREAD)) < 0)
		sysfatal("open %s: %r", argv[1]);
	cap = 8192;
	data = malloc(cap);
	n = 0;
	while((m = read(fd, data+n, cap-n)) > 0){
		n += m;
		if(n == cap){
			cap *= 2;
			data = realloc(data, cap);
		}
		if(data == nil)
			sysfatal("out of memory");
	}
	close(fd);
	items = parsehtml(data, n, runesmprint("%s", argv[1]), TextHtml, UTF_8, &di);
	memset(count, 0, sizeof count);
	for(it = items; it != nil; it = it->next)
		if(it->tag >= 0 && it->tag <= Ispacertag)
			count[it->tag]++;
	print("file %s\nbytes %ld\nvalid %d\n", argv[1], n, validitems(items));
	print("title %S\n", di->doctitle != nil ? di->doctitle : L"");
	for(m = 0; m <= Ispacertag; m++)
		print("%s %d\n", tagname[m], count[m]);
	printitems(items, argv[1]);
	exits(nil);
}
