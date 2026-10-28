/*
 * ztrie - a folder's paths as one production in Wirth syntax notation, a
 * Patricia trie: each folder's entries are alternatives, and every chain of
 * single children is merged into one LITERAL.
 *
 *	ztrie NAME <paths
 *
 * paths: one relative path per line (a/b/c), folders and files alike. The
 * sentences of NAME are exactly the leaves: the paths with nothing under them.
 * A quote in a name is written "" .
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

typedef struct Node Node;
struct Node {
	char	*name;
	Node	**kid;
	int	nkid;
};

static Node root;
static Biobuf out;

static Node*
child(Node *n, char *name)
{
	int i;
	Node *c;

	for(i = 0; i < n->nkid; i++)
		if(strcmp(n->kid[i]->name, name) == 0)
			return n->kid[i];
	c = mallocz(sizeof *c, 1);
	c->name = strdup(name);
	n->kid = realloc(n->kid, (n->nkid + 1) * sizeof n->kid[0]);
	n->kid[n->nkid++] = c;
	return c;
}

static void
lit(char *s)
{
	Bputc(&out, '"');
	for(; *s; s++){
		if(*s == '"')
			Bputc(&out, '"');
		Bputc(&out, *s);
	}
	Bputc(&out, '"');
}

/* the subtree under n, its edge so far spelled label */
static void
emit(Node *n, char *label)
{
	int i;
	char *l;

	if(n->nkid == 0){
		lit(label);
		return;
	}
	if(n->nkid == 1){
		l = smprint("%s/%s", label, n->kid[0]->name);
		emit(n->kid[0], l);
		free(l);
		return;
	}
	l = smprint("%s/", label);
	lit(l);
	free(l);
	Bprint(&out, " ( ");
	for(i = 0; i < n->nkid; i++){
		if(i > 0)
			Bprint(&out, " | ");
		emit(n->kid[i], n->kid[i]->name);
	}
	Bprint(&out, " )");
}

void
main(int argc, char *argv[])
{
	Biobuf in;
	char *line, *f[64];
	int nf, i;
	Node *n;

	if(argc != 2)
		sysfatal("usage: ztrie NAME <paths");
	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	while((line = Brdstr(&in, '\n', 1)) != nil){
		nf = getfields(line, f, nelem(f), 1, "/");
		for(n = &root, i = 0; i < nf; i++)
			n = child(n, f[i]);
	}
	Bprint(&out, "%s =", argv[1]);
	for(i = 0; i < root.nkid; i++){
		Bprint(&out, i == 0 ? " " : "\n  | ");
		emit(root.kid[i], root.kid[i]->name);
	}
	if(root.nkid == 0)
		Bprint(&out, " { \" \" }");
	Bprint(&out, " .\n");
	Bterm(&out);
	exits(nil);
}
