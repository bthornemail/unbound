/*
 * htmlfs - serve HTML pages, as libhtml reads them, as a 9P file tree.
 *
 *	htmlfs [-s srvname] [-m mtpt] file.html ...
 *
 * Each page is read with parsehtml(2) and becomes a directory named by its
 * file, holding three read-only files built from its item list:
 *	title	the Docinfo's title
 *	items	one line per item: its tag, and for Itext its text
 *	text	the Itext runs alone, one per line
 * The tree is a 9pfile(2) in-memory hierarchy, posted and mounted with
 * postmountsrv(2), as /sys/src/lib9p/ramfs.c does.
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
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

static char *tagname[] = {
[Itexttag] "Itext", [Iruletag] "Irule", [Iimagetag] "Iimage", [Iformfieldtag] "Iformfield",
[Itabletag] "Itable", [Ifloattag] "Ifloat", [Ispacertag] "Ispacer",
};

static char*
slurp(char *name, long *np)
{
	int fd;
	Dir *d;
	char *b;

	if((fd = open(name, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", name);
	b = emalloc(d->length + 1);
	*np = readn(fd, b, d->length);
	close(fd);
	free(d);
	if(*np < 0)
		sysfatal("read %s: %r", name);
	return b;
}

/* a read-only file holding s; its length is set so stat (and tar) see it */
static void
addfile(File *dir, char *name, char *s)
{
	File *f;

	if((f = createfile(dir, name, nil, 0444, s)) == nil)
		sysfatal("createfile %s: %r", name);
	f->length = strlen(s);
	closefile(f);
}

static void
addpage(File *root, char *name)
{
	char *data, *base;
	long n;
	Docinfo *di;
	Item *it;
	Fmt items, text;
	File *dir;

	data = slurp(name, &n);
	it = parsehtml((uchar*)data, n, runesmprint("%s", name), TextHtml, UTF_8, &di);
	fmtstrinit(&items);
	fmtstrinit(&text);
	for(; it != nil; it = it->next){
		if(it->tag == Itexttag){
			fmtprint(&items, "%s %S\n", tagname[it->tag], ((Itext*)it)->s);
			fmtprint(&text, "%S\n", ((Itext*)it)->s);
		}else if(it->tag >= 0 && it->tag < nelem(tagname))
			fmtprint(&items, "%s\n", tagname[it->tag]);
	}
	base = utfrrune(name, '/') ? utfrrune(name, '/') + 1 : name;
	if((dir = createfile(root, base, nil, DMDIR|0555, nil)) == nil)
		sysfatal("createfile %s: %r", base);
	addfile(dir, "title", smprint("%S\n", di->doctitle ? di->doctitle : L""));
	addfile(dir, "items", fmtstrflush(&items));
	addfile(dir, "text", fmtstrflush(&text));
	closefile(dir);
	free(data);
}

static void
fsread(Req *r)
{
	char *s;

	if((s = r->fid->file->aux) == nil){
		respond(r, "not a file");
		return;
	}
	readstr(r, s);
	respond(r, nil);
}

Srv fs = {
	.read = fsread,
};

void
usage(void)
{
	fprint(2, "usage: htmlfs [-s srvname] [-m mtpt] file.html ...\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	char *srvname, *mtpt;
	int i;

	srvname = nil;
	mtpt = "/n/html";
	ARGBEGIN{
	case 's':
		srvname = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND
	if(argc == 0)
		usage();
	fs.tree = alloctree(nil, nil, DMDIR|0555, nil);
	for(i = 0; i < argc; i++)
		addpage(fs.tree->root, argv[i]);
	postmountsrv(&fs, srvname, mtpt, MREPL);
	exits(nil);
}
