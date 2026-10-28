/*
 * zaudit - the contract of light-garden's audit records as Wirth syntax
 * notation, to fd 1, from the keys the records use (in/audit/KEYS).
 *	zaudit KEYS
 * A record is canonical JSON (no white space) whose top-level keys are only
 * the audit's own (the doctrine's strict keysets: unknown keys reject); an
 * ERRORRECORD is a record with at least one key naming an error (one that
 * contains "rror": expectedError, expected_error, expected_errors). Values
 * follow JSON (RFC 8259): string, number, true, false, null, array, object.
 * One production a line, in OMI.WSN's render layout, so the text is already
 * in the normal form the Haskell normalizer writes.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static char *keys[256];
static int nkeys;

/* a WSN literal holding a JSON string: the quote doubled inside */
static void
jsonkey(char *k)
{
	print("\"\"\"%s\"\"\"", k);
}

static void
alternatives(char *name, int errors)
{
	int i, first;

	print("%s =", name);
	first = 1;
	for(i = 0; i < nkeys; i++){
		if(errors && strstr(keys[i], "rror") == nil)
			continue;
		print(first ? " " : " | ");
		jsonkey(keys[i]);
		first = 0;
	}
	print(" .\n");
}

void
main(int argc, char **argv)
{
	Biobuf *b;
	char *l;
	int c, first;

	if(argc != 2){
		fprint(2, "usage: zaudit KEYS\n");
		exits("usage");
	}
	if((b = Bopen(argv[1], OREAD)) == nil)
		sysfatal("%s: %r", argv[1]);
	while(nkeys < nelem(keys) && (l = Brdstr(b, '\n', 1)) != nil)
		if(l[0])
			keys[nkeys++] = l;
	Bterm(b);

	print("RECORD = \"{\" MEMBER { \",\" MEMBER } \"}\" .\n");
	print("ERRORRECORD = \"{\" { MEMBER \",\" } ERRORMEMBER { \",\" MEMBER } \"}\" .\n");
	print("MEMBER = KEY \":\" VALUE .\n");
	print("ERRORMEMBER = ERRORKEY \":\" VALUE .\n");
	alternatives("KEY", 0);
	alternatives("ERRORKEY", 1);
	print("VALUE = STRING | NUMBER | \"true\" | \"false\" | \"null\" | ARRAY | OBJECT .\n");
	print("ARRAY = \"[\" [ VALUE { \",\" VALUE } ] \"]\" .\n");
	print("OBJECT = \"{\" [ PAIR { \",\" PAIR } ] \"}\" .\n");
	print("PAIR = STRING \":\" VALUE .\n");
	print("STRING = \"\"\"\" { CHAR | ESCAPE } \"\"\"\" .\n");
	/* every printable ASCII character but the quote and the backslash */
	print("CHAR =");
	first = 1;
	for(c = ' '; c <= '~'; c++){
		if(c == '"' || c == '\\')
			continue;
		print(first ? " \"%c\"" : " | \"%c\"", c);
		first = 0;
	}
	print(" .\n");
	print("ESCAPE = \"\\\" ( \"\"\"\" | \"\\\" | \"/\" | \"b\" | \"f\" | \"n\" | \"r\" | \"t\" ) .\n");
	print("NUMBER = [ \"-\" ] INT [ \".\" DIGITS ] [ ( \"e\" | \"E\" ) [ \"+\" | \"-\" ] DIGITS ] .\n");
	print("INT = \"0\" | ONENINE { DIGIT } .\n");
	print("DIGITS = DIGIT { DIGIT } .\n");
	print("DIGIT = \"0\" | ONENINE .\n");
	print("ONENINE = \"1\" | \"2\" | \"3\" | \"4\" | \"5\" | \"6\" | \"7\" | \"8\" | \"9\" .\n");
	exits(nil);
}
