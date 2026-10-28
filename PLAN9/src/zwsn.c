/*
 * zwsn - read Wirth syntax notation by its own grammar.
 *
 *	zwsn file ...
 *
 *	SYNTAX     = { PRODUCTION } .
 *	PRODUCTION = IDENTIFIER "=" EXPRESSION "." .
 *	EXPRESSION = TERM { "|" TERM } .
 *	TERM       = FACTOR { FACTOR } .
 *	FACTOR     = IDENTIFIER | LITERAL | "[" EXPRESSION "]"
 *	           | "(" EXPRESSION ")" | "{" EXPRESSION "}" .
 *	IDENTIFIER = letter { letter } .
 *	LITERAL    = """" character { character } """" .
 *
 * For each file: its productions, or the line where it stops parsing.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static char *s, *name;
static int line, nprod;
static jmp_buf fail;

static void
bad(char *what)
{
	print("%s:%d: %s\n", name, line, what);
	longjmp(fail, 1);
}

static void
space(void)
{
	for(; *s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'; s++)
		if(*s == '\n')
			line++;
}

static int
letter(int c)
{
	return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z';
}

static void expression(void);

static void
identifier(void)
{
	if(!letter(*s))
		bad("expected an identifier");
	while(letter(*s))
		s++;
}

static void
literal(void)
{
	s++;
	if(*s == '"' && s[1] != '"')
		bad("empty literal");
	for(;;){
		if(*s == 0 || *s == '\n')
			bad("unterminated literal");
		if(*s == '"'){
			if(s[1] != '"'){
				s++;
				return;
			}
			s++;
		}
		s++;
	}
}

static void
closer(int c)
{
	space();
	if(*s != c)
		bad("unclosed bracket");
	s++;
}

static int
factor(void)
{
	space();
	if(letter(*s))
		identifier();
	else if(*s == '"')
		literal();
	else if(*s == '['){
		s++; expression(); closer(']');
	}else if(*s == '('){
		s++; expression(); closer(')');
	}else if(*s == '{'){
		s++; expression(); closer('}');
	}else
		return 0;
	return 1;
}

static void
term(void)
{
	if(!factor())
		bad("expected a factor");
	while(factor())
		;
}

static void
expression(void)
{
	term();
	for(space(); *s == '|'; space()){
		s++;
		term();
	}
}

static void
production(void)
{
	identifier();
	space();
	if(*s != '=')
		bad("expected =");
	s++;
	expression();
	space();
	if(*s != '.')
		bad("expected .");
	s++;
	nprod++;
}

void
main(int argc, char *argv[])
{
	int i, fd;
	char *buf;
	long n;
	Dir *d;

	for(i = 1; i < argc; i++){
		name = argv[i];
		if((fd = open(name, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
			sysfatal("open %s: %r", name);
		buf = mallocz(d->length + 1, 1);
		n = readn(fd, buf, d->length);
		close(fd);
		free(d);
		if(n < 0)
			sysfatal("read %s: %r", name);
		s = buf;
		line = 1;
		nprod = 0;
		if(setjmp(fail) == 0){
			for(space(); *s; space())
				production();
			print("%s: %d productions\n", name, nprod);
		}
		free(buf);
	}
	exits(nil);
}
