/*
 * mkfs - Metaverse Kit's world as a 9P file server: a world holds spaces, a
 * space holds tiles, and each tile is two files.
 *	mkfs [-m mtpt] -g grammar -l zlazy -f zmk [space tile events]...
 *	(mtpt defaults to /n/mk; the server is also posted as /srv/mk)
 *	ctl				write "tile SPACE Z/X/Y": make that tile
 *	spaces/SPACE/tiles/Z/X/Y/events	append-only, one event a line
 *					(wsn/mkevent.wsn); a write is taken
 *					whole when zlazy accepts every line of
 *					it, else refused, nothing appended
 *	spaces/SPACE/tiles/Z/X/Y/state	read: zmk's fold of the events, one line
 *					a node; a write is refused
 * Each space tile events makes a tile at the start with a file's events, checked
 * the same way. The fold and the grammar are the programs zmk and zlazy,
 * run as they are; this server only keeps the lines and hands them over.
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

typedef struct Tile Tile;
struct Tile {
	char	*space;
	char	*id;		/* z/x/y */
	char	*ev;		/* the events, as appended */
	long	nev;
	File	*events, *state;
};

typedef struct Fold Fold;
struct Fold {
	char	*s;
	long	n;
};

enum { Qctl, Qevents, Qstate, Qdir };

extern Srv fs;

static char *grammar, *zlazy, *zmk;
static Tile **tiles;
static int ntiles;

/* run argv with in on its standard input; its standard output in *out; its exit status returned */
static char*
run(char **argv, char *in, long nin, char **out, long *nout)
{
	int pin[2], pout[2], pid, n;
	long m, cap;
	char *buf;
	Waitmsg *w;
	char *status;

	if(pipe(pin) < 0 || pipe(pout) < 0)
		return "pipe";
	switch(pid = rfork(RFPROC|RFFDG)){
	case -1:
		return "fork";
	case 0:
		dup(pin[1], 0);
		dup(pout[1], 1);
		close(pin[0]); close(pin[1]); close(pout[0]); close(pout[1]);
		exec(argv[0], argv);
		sysfatal("exec %s: %r", argv[0]);
	}
	close(pin[1]);
	close(pout[1]);
	/* a writer of its own, so a long input cannot stall against an unread output */
	if(nin > 0 && rfork(RFPROC|RFFDG|RFNOWAIT) == 0){
		close(pout[0]);
		write(pin[0], in, nin);
		_exits(nil);
	}
	close(pin[0]);
	cap = 8192;
	buf = malloc(cap);
	m = 0;
	while((n = read(pout[0], buf+m, cap-m)) > 0){
		m += n;
		if(m == cap)
			buf = realloc(buf, cap *= 2);
	}
	close(pout[0]);
	status = "wait";
	while((w = wait()) != nil){
		if(w->pid == pid){
			status = w->msg[0] ? estrdup9p(w->msg) : nil;
			free(w);
			break;
		}
		free(w);
	}
	if(out != nil){
		*out = buf;
		*nout = m;
	}else
		free(buf);
	return status;
}

/* each line of s accepted by the grammar's EVENT, or the first refused */
static char*
check(char *s, long n)
{
	char *t, *l, *e, *argv[5];
	int i;

	t = emalloc9p(n + 1);
	memmove(t, s, n);
	t[n] = 0;
	for(l = t, i = 1; *l; l = e, i++){
		if((e = strchr(l, '\n')) == nil)
			e = l + strlen(l);
		else
			*e++ = 0;
		if(*l == 0)
			continue;
		argv[0] = zlazy; argv[1] = grammar; argv[2] = "EVENT"; argv[3] = l; argv[4] = nil;
		if(run(argv, nil, 0, nil, nil) != nil){
			free(t);
			return smprint("line %d is not an event of %s", i, grammar);
		}
	}
	free(t);
	return nil;
}

static File*
dir(File *parent, char *name)
{
	File *f;

	if((f = walkfile(parent, name)) != nil)
		return f;
	return createfile(parent, name, nil, DMDIR|0555, (void*)Qdir);
}

static char*
mktile(char *space, char *id)
{
	File *d;
	char *f[3], *s;
	Tile *t;
	int i;

	s = estrdup9p(id);
	if(getfields(s, f, 3, 0, "/") != 3 || f[0][0] != 'z' || f[1][0] != 'x' || f[2][0] != 'y'){
		free(s);
		return "a tile is z/x/y";
	}
	for(i = 0; i < ntiles; i++)
		if(strcmp(tiles[i]->space, space) == 0 && strcmp(tiles[i]->id, id) == 0){
			free(s);
			return nil;
		}
	d = dir(dir(dir(dir(dir(dir(fs.tree->root, "spaces"), space), "tiles"), f[0]), f[1]), f[2]);
	free(s);
	t = emalloc9p(sizeof *t);
	t->space = estrdup9p(space);
	t->id = estrdup9p(id);
	t->ev = estrdup9p("");
	t->events = createfile(d, "events", nil, DMAPPEND|0666, t);
	t->state = createfile(d, "state", nil, 0444, t);
	tiles = erealloc9p(tiles, (ntiles+1) * sizeof(Tile*));
	tiles[ntiles++] = t;
	return nil;
}

static char*
append(Tile *t, char *s, long n)
{
	char *err;

	if(n == 0)
		return nil;
	if((err = check(s, n)) != nil)
		return err;
	t->ev = erealloc9p(t->ev, t->nev + n + 2);
	memmove(t->ev + t->nev, s, n);
	t->nev += n;
	if(t->ev[t->nev-1] != '\n')
		t->ev[t->nev++] = '\n';
	t->ev[t->nev] = 0;
	t->events->length = t->nev;
	return nil;
}

static void
fsopen(Req *r)
{
	Tile *t;
	char *argv[2], *err;
	Fold *f;

	if(r->fid->file->aux == (void*)Qctl || r->fid->file->aux == (void*)Qdir){
		respond(r, nil);
		return;
	}
	t = r->fid->file->aux;
	if(r->fid->file == t->events && (r->ifcall.mode & OTRUNC)){
		respond(r, "the events are append-only");
		return;
	}
	if(r->fid->file == t->state){
		/* the fold, once an open, so every read of this fid sees the same state */
		argv[0] = zmk; argv[1] = nil;
		f = emalloc9p(sizeof *f);
		if((err = run(argv, t->ev, t->nev, &f->s, &f->n)) != nil){
			free(f);
			respond(r, smprint("zmk: %s", err));
			return;
		}
		r->fid->aux = f;
		r->fid->file->length = f->n;
	}
	respond(r, nil);
}

static void
fsread(Req *r)
{
	Tile *t;

	if(r->fid->file->aux == (void*)Qctl){
		readstr(r, "");
		respond(r, nil);
		return;
	}
	t = r->fid->file->aux;
	if(r->fid->file == t->state)
		readbuf(r, ((Fold*)r->fid->aux)->s, ((Fold*)r->fid->aux)->n);
	else
		readbuf(r, t->ev, t->nev);
	respond(r, nil);
}

static void
fswrite(Req *r)
{
	char *s, *f[4], *err;
	Tile *t;
	long n;

	n = r->ifcall.count;
	if(r->fid->file->aux == (void*)Qctl){
		s = emalloc9p(n + 1);
		memmove(s, r->ifcall.data, n);
		s[n] = 0;
		if(tokenize(s, f, 4) != 3 || strcmp(f[0], "tile") != 0)
			err = "ctl takes: tile SPACE Z/X/Y";
		else
			err = mktile(f[1], f[2]);
		free(s);
	}else{
		t = r->fid->file->aux;
		err = r->fid->file == t->events ? append(t, r->ifcall.data, n) : "the state is the fold of the events: write to events";
	}
	if(err == nil)
		r->ofcall.count = n;
	respond(r, err);
}

static void
fsdestroyfid(Fid *fid)
{
	Tile *t;

	if(fid->file != nil && fid->file->aux != (void*)Qctl && fid->file->aux != (void*)Qdir){
		t = fid->file->aux;
		if(fid->file == t->state && fid->aux != nil){
			free(((Fold*)fid->aux)->s);
			free(fid->aux);
		}
	}
}

Srv fs = {
	.open = fsopen,
	.read = fsread,
	.write = fswrite,
	.destroyfid = fsdestroyfid,
};

static char*
slurp(char *file, long *n)
{
	int fd;
	Dir *d;
	char *s;

	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", file);
	*n = d->length;
	free(d);
	s = emalloc9p(*n + 1);
	if(readn(fd, s, *n) != *n)
		sysfatal("read %s: %r", file);
	close(fd);
	return s;
}

void
main(int argc, char *argv[])
{
	char *mtpt, *err, *s;
	int i;
	long n;
	Tile *t;

	mtpt = "/n/mk";
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(sysfatal("-m mtpt"));
		break;
	case 'g':
		grammar = EARGF(sysfatal("-g grammar"));
		break;
	case 'l':
		zlazy = EARGF(sysfatal("-l zlazy"));
		break;
	case 'f':
		zmk = EARGF(sysfatal("-f zmk"));
		break;
	}ARGEND
	if(grammar == nil || zlazy == nil || zmk == nil || argc % 3 != 0)
		sysfatal("usage: mkfs [-m mtpt] -g grammar -l zlazy -f zmk [space tile events]...");
	fs.tree = alloctree(nil, nil, DMDIR|0555, nil);
	createfile(fs.tree->root, "ctl", nil, 0222, (void*)Qctl);
	dir(fs.tree->root, "spaces");
	for(i = 0; i < argc; i += 3){
		if((err = mktile(argv[i], argv[i+1])) != nil)
			sysfatal("%s %s: %s", argv[i], argv[i+1], err);
		t = tiles[ntiles-1];
		s = slurp(argv[i+2], &n);
		if((err = append(t, s, n)) != nil)
			sysfatal("%s: %s", argv[i+2], err);
		free(s);
	}
	postmountsrv(&fs, "mk", mtpt, MREPL);
	exits(nil);
}
