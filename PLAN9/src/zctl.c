/*
 * zctl - the atomic kernel's control plane (~/devops/genesis-frame/atomic-kernel,
 * control_plane.py, after its docs/CONTROL_PLANE_SPEC.md) in Plan 9 C. A stream is a
 * channel byte, FS GS RS US (0x1C to 0x1F), then a mask (lane in bits 7:4, the context
 * CT in bits 3:2, binary 11 in bits 1:0): CT=00 a control, CT=01 a number system
 * (DEC BIN HEX B36 from the lane's high two bits), CT=10 one UTF-8 codepoint follows,
 * CT=11 one algorithm byte follows, its high nibble the kernel's B[tick mod 8] (the
 * tick is not in the stream, so this is the one check the grammar wsn/ctl.wsn cannot
 * make). Every other byte is data. A stream that breaks a rule is refused with the
 * kernel's own code, at the first byte that breaks it. Lines in, lines out; a line is
 * its tick, then its bytes in hex, each followed by a space.
 *	zctl parse	each stream, then its events, one a line
 *	zctl reject	each stream, then "error CODE", or "accepted"
 *	zctl cobs	each payload (- if empty) COBS-encoded, as cobs_encode does
 *	zctl uncobs	each frame decoded (- if empty), or the frame then "error CODE"
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { Max = 4096 };

static int B[8] = { 0, 1, 3, 6, 9, 8, 6, 3 };	/* crystal.py: B, T = 8 */
static char *chname[4] = { "FS", "GS", "RS", "US" };
static char *numsys[4] = { "DEC", "BIN", "HEX", "B36" };
static Biobuf out;

static int
bytes(char *l, uchar *b, int skip)
{
	char *f[Max];
	int n, i;

	n = getfields(l, f, Max, 1, " ");
	for(i = skip; i < n; i++)
		b[i-skip] = strtol(f[i], nil, 16);
	return n - skip;
}

/* one codepoint at b[0..n), as Python's strict UTF-8 decode reads it: its length, or 0 and the error */
static int
codepoint(uchar *b, int n, long *cp, char **err)
{
	int need, i;
	uchar lo, hi;

	*err = "CT10_INVALID_UTF8";
	if(n <= 0){
		*err = "CT10_MISSING_UTF8";
		return 0;
	}
	lo = 0x80;
	hi = 0xBF;
	if(b[0] <= 0x7F){
		*cp = b[0];
		return 1;
	}else if(b[0] >= 0xC2 && b[0] <= 0xDF){
		need = 2;
		*cp = b[0] & 0x1F;
	}else if(b[0] >= 0xE0 && b[0] <= 0xEF){
		need = 3;
		*cp = b[0] & 0x0F;
		if(b[0] == 0xE0) lo = 0xA0;
		if(b[0] == 0xED) hi = 0x9F;	/* the surrogates */
	}else if(b[0] >= 0xF0 && b[0] <= 0xF4){
		need = 4;
		*cp = b[0] & 0x07;
		if(b[0] == 0xF0) lo = 0x90;
		if(b[0] == 0xF4) hi = 0x8F;
	}else
		return 0;
	if(need > n)
		return 0;
	for(i = 1; i < need; i++){
		if(b[i] < (i == 1 ? lo : 0x80) || b[i] > (i == 1 ? hi : 0xBF))
			return 0;
		*cp = *cp << 6 | (b[i] & 0x3F);
	}
	return need;
}

/* the events of b[0..n) at tick, printed if show; nil, or the code that refuses the stream */
static char*
parse(uchar *b, int n, int tick, int show)
{
	int i, j, k, lane, ct;
	uchar mask;
	long cp;
	char *err;

	for(i = 0; i < n; ){
		if(b[i] < 0x1C || b[i] > 0x1F){
			if(show)
				Bprint(&out, "data %02X\n", b[i]);
			i++;
			continue;
		}
		if(i + 1 >= n)
			return "MISSING_MASK";
		mask = b[i+1];
		if((mask & 3) != 3)
			return "INVALID_MASK_MARKER";
		lane = mask >> 4;
		ct = mask >> 2 & 3;
		j = b[i] - 0x1C;
		i += 2;
		switch(ct){
		case 0:
			if(show)
				Bprint(&out, "control %s lane %d mask %02X\n", chname[j], lane, mask);
			break;
		case 1:
			if(show)
				Bprint(&out, "numsys %s lane %d mask %02X %s %d\n", chname[j], lane, mask, numsys[lane>>2 & 3], lane & 3);
			break;
		case 2:
			if((k = codepoint(b+i, n-i, &cp, &err)) == 0)
				return err;
			if(show){
				Bprint(&out, "unicode %s lane %d mask %02X U+%04lX ", chname[j], lane, mask, cp);
				for(; k > 0; k--)
					Bprint(&out, "%02X", b[i++]);
				Bprint(&out, "\n");
			}else
				i += k;
			break;
		case 3:
			if(i >= n)
				return "CT11_MISSING_EXT";
			if((b[i] >> 4) != (B[tick % 8] & 0x0F))
				return "CT11_ALGO_MISMATCH";
			if(show)
				Bprint(&out, "extended %s lane %d mask %02X %02X %X %X\n", chname[j], lane, mask, b[i], b[i] >> 4, b[i] & 0x0F);
			i++;
			break;
		}
	}
	return nil;
}

static void
hexs(uchar *b, int n)
{
	int i;

	for(i = 0; i < n; i++)
		Bprint(&out, "%02X ", b[i]);
}

static int
cobs(uchar *d, int n, uchar *o)
{
	int i, m, at, code;

	m = 0;
	at = m++;
	code = 1;
	for(i = 0; i < n; i++){
		if(d[i] == 0){
			o[at] = code;
			at = m++;
			code = 1;
			continue;
		}
		o[m++] = d[i];
		if(++code == 0xFF){
			o[at] = code;
			at = m++;
			code = 1;
		}
	}
	o[at] = code;
	return m;
}

static char*
uncobs(uchar *d, int n, uchar *o, int *m)
{
	int i, code, end;

	*m = 0;
	if(n == 0)
		return "COBS_EMPTY";
	for(i = 0; i < n; i++)
		if(d[i] == 0)
			return "COBS_ZERO_IN_FRAME";
	for(i = 0; i < n; ){
		code = d[i++];
		end = i + code - 1;
		if(end > n)
			return "COBS_OVERRUN";
		while(i < end)
			o[(*m)++] = d[i++];
		if(code != 0xFF && i < n)
			o[(*m)++] = 0;
	}
	return nil;
}

void
main(int argc, char *argv[])
{
	Biobuf in;
	char *l, *s, *err, *f[2];
	uchar b[Max], o[2*Max];
	int n, m, tick;

	if(argc != 2)
		sysfatal("usage: zctl parse|reject|cobs|uncobs");
	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	while((l = Brdstr(&in, '\n', 1)) != nil){
		s = strdup(l);
		if(strcmp(argv[1], "parse") == 0 || strcmp(argv[1], "reject") == 0){
			if(getfields(s, f, 2, 1, " ") < 1)
				continue;
			tick = atoi(f[0]);
			n = bytes(l, b, 1);
			if(argv[1][0] == 'p'){
				Bprint(&out, "stream %d", tick);
				for(m = 0; m < n; m++)
					Bprint(&out, " %02X", b[m]);
				Bprint(&out, "\n");
				if((err = parse(b, n, tick, 1)) != nil)
					Bprint(&out, "error %s\n", err);
			}else{
				err = parse(b, n, tick, 0);
				Bprint(&out, "%d ", tick);
				hexs(b, n);
				Bprint(&out, "%s%s\n", err ? "error " : "accepted", err ? err : "");
			}
		}else if(strcmp(argv[1], "cobs") == 0){
			n = strcmp(l, "-") == 0 ? 0 : bytes(l, b, 0);
			m = cobs(b, n, o);
			hexs(o, m);
			Bprint(&out, "\n");
		}else if(strcmp(argv[1], "uncobs") == 0){
			n = strncmp(l, "-", 1) == 0 ? 0 : bytes(l, b, 0);
			if((err = uncobs(b, n, o, &m)) != nil){
				if(n == 0)
					Bprint(&out, "- ");
				else
					hexs(b, n);
				Bprint(&out, "error %s\n", err);
			}else if(m == 0)
				Bprint(&out, "-\n");
			else{
				hexs(o, m);
				Bprint(&out, "\n");
			}
		}else
			sysfatal("usage: zctl parse|reject|cobs|uncobs");
		free(s);
		free(l);
	}
	Bterm(&out);
	exits(nil);
}
