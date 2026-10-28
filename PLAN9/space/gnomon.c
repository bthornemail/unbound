/*
 * gnomon - polytron's tetra-gnomon-anim.c (docs/GNOMON.md) on Plan 9's
 * screen with draw(2) and event(2), in place of the terminal's escape codes.
 * The eight states from 0x4242 under K(p, 0x1D), one step every half second:
 * each at its cell on the terminal's 80 by 24 grid (column 40 + 30 cos a, row
 * 12 + 10 sin a), the current one lit in its colour, the others at half;
 * then the step, its State and SID, the state bar, the kernel and its constant.
 * A colour is the original's: r, g, b the state's first three nibbles over 15,
 * taken to the xterm 256-colour cube as set_color does (16 + 36r + 6g + b, each
 * scaled by 5), the cube's levels 0 95 135 175 215 255.
 *	gnomon			on the screen; q or Del quits
 *	gnomon -f step file	that step's frame, offscreen, to file (writeimage)
 * The original's info starts at row 22 and runs to row 30, so a 24-row terminal
 * scrolls it; here the grid has 30 rows. Its symbols "●◐○◑○◐●" are taken a byte
 * at a time from UTF-8 and never drawn; here the current step is a disc and the
 * others rings, as the ● and ○ it prints.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

enum {
	STEPS = 8,
	C = 0x1D,
	COLS = 80,
	ROWS = 30,
	CW = 10,	/* a terminal cell */
	CH = 20,
};

static ushort state[STEPS], sid[STEPS];
static int cube[6] = { 0, 95, 135, 175, 215, 255 };

static ushort
rotl(ushort x, int n)
{
	return x<<n | x>>(16-n);
}

static ushort
K(ushort p, ushort c)
{
	return rotl(p, 1) ^ rotl(p, 3) ^ rotl(p, 14) ^ c;
}

/* set_color: the nibbles over 15, times dim, to the cube */
static ulong
colour(ushort s, double dim)
{
	int b0, b1, ri, gi, bi;

	b0 = s >> 8 & 0xFF;
	b1 = s & 0xFF;
	ri = (b0 >> 4 & 0xF) / 15.0 * dim * 5;
	gi = (b0 & 0xF) / 15.0 * dim * 5;
	bi = (b1 >> 4 & 0xF) / 15.0 * dim * 5;
	return (ulong)cube[ri]<<24 | cube[gi]<<16 | cube[bi]<<8 | 0xFF;
}

static Image*
paint(ulong rgba)
{
	return allocimage(display, Rect(0, 0, 1, 1), screen ? screen->chan : RGB24, 1, rgba);
}

/* the centre of cell (col, row), both from 1 as the cursor's */
static Point
cell(Rectangle r, int col, int row)
{
	return Pt(r.min.x + (col-1)*CW + CW/2, r.min.y + (row-1)*CH + CH/2);
}

static void
text(Image *dst, Rectangle r, int row, Image *ink, char *s)
{
	string(dst, Pt(r.min.x, r.min.y + (row-1)*CH), ink, ZP, font, s);
}

static void
frame(Image *dst, Rectangle r, int cur)
{
	Image *bg, *ink, *im, *shade;
	Point p;
	char buf[64];
	int i, x, y, rad;
	float a;

	bg = paint(0x000000FF);
	ink = paint(0xD0D0D0FF);
	shade = paint(0x585858FF);
	draw(dst, r, bg, nil, ZP);
	text(dst, r, 1, ink, "═══ TETRA GNOMON ANIMATION ═══");
	snprint(buf, sizeof buf, "Period: %d steps", STEPS);
	text(dst, r, 2, ink, buf);
	rad = CH/2 - 2;
	for(i = 0; i < STEPS; i++){
		a = (float)i / STEPS * 6.28318f;
		x = 40 + 30 * cos(a);	/* the original's (int) casts */
		y = 12 + 10 * sin(a);
		p = cell(r, x, y);
		im = paint(colour(state[i], i == cur ? 1.0 : 0.5));
		if(i == cur)
			fillellipse(dst, p, rad, rad, im, ZP);
		else
			ellipse(dst, p, rad, rad, 1, im, ZP);
		freeimage(im);
	}
	snprint(buf, sizeof buf, "Current Step: %d", cur);
	text(dst, r, 22, ink, buf);
	snprint(buf, sizeof buf, "State: 0x%.4uX", state[cur]);
	text(dst, r, 23, ink, buf);
	snprint(buf, sizeof buf, "SID:  0x%.4uX", sid[cur]);
	text(dst, r, 24, ink, buf);
	text(dst, r, 26, ink, "State evolution:");
	for(i = 0; i < STEPS; i++){
		p = Pt(r.min.x + i*CW, r.min.y + 26*CH);
		if(i == cur){
			im = paint(colour(state[i], 1.0));
			draw(dst, Rect(p.x, p.y, p.x+CW, p.y+CH), im, nil, ZP);
			freeimage(im);
		}else
			border(dst, Rect(p.x+1, p.y+1, p.x+CW-1, p.y+CH-1), 1, shade, ZP);	/* ░ */
	}
	text(dst, r, 29, ink, "Kernel: K(p,C) = rotl(p,1) ^ rotl(p,3) ^ rotr(p,2) ^ C");
	snprint(buf, sizeof buf, "Constitutional constant: 0x%.2X", C);
	text(dst, r, 30, ink, buf);
	freeimage(bg);
	freeimage(ink);
	freeimage(shade);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
}

void
main(int argc, char **argv)
{
	Event e;
	Image *off;
	ulong timer;
	int i, k, cur, fd;
	ushort s;

	for(s = 0x4242, i = 0; i < STEPS; i++){
		state[i] = s;
		sid[i] = K(s, C);
		s = sid[i];
	}
	if(initdraw(nil, nil, "gnomon") < 0)
		sysfatal("initdraw: %r");
	if(argc == 4 && strcmp(argv[1], "-f") == 0){
		off = allocimage(display, Rect(0, 0, COLS*CW, ROWS*CH), RGB24, 0, DBlack);
		if(off == nil)
			sysfatal("allocimage: %r");
		frame(off, off->r, atoi(argv[2]) % STEPS);
		if((fd = create(argv[3], OWRITE, 0644)) < 0)
			sysfatal("create %s: %r", argv[3]);
		if(writeimage(fd, off, 0) < 0)
			sysfatal("writeimage: %r");
		close(fd);
		exits(nil);
	}
	einit(Emouse|Ekeyboard);
	timer = etimer(0, 500);	/* usleep(500000) */
	cur = 0;
	frame(screen, screen->r, cur);
	flushimage(display, 1);
	for(;;){
		k = event(&e);
		if(k == Ekeyboard && (e.kbdc == 'q' || e.kbdc == 0x7F))
			break;
		if(k == timer){
			cur = (cur + 1) % STEPS;
			frame(screen, screen->r, cur);
			flushimage(display, 1);
		}
	}
	closedisplay(display);
	exits(nil);
}
