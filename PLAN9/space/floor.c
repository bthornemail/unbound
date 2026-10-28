/*
 * floor - the Agent Commons floor (boxedPINE space/src/plane.ts drawFloor) on
 * Plan 9's screen, with draw(2) and event(2).
 *
 * A place is (u, v) on the Commons plane, the rim at 1, hue 0 to the north.
 * The floor shows the wheel's twelve sectors of 30, the rim, and the eight
 * characters at their hues (space/src/characters.ts; the Observer, no hue, at
 * the centre). Each character steps its own K (space/src/gnomon.ts) from its
 * seed, the pair of its tag's first letter and its point, once a tick; its
 * state is written under its name. Button 1 selects the nearest within 0.15
 * of the rim (plane.ts nearest); q or Del quits. With -o (observed) the
 * keyboard stays the shell's, for the Observer (observe.c), and button 3 quits.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

enum {
	RIM = 7,	/* world units to the rim */
	FLOOR = 8,	/* world radius the floor covers */
	NCH = 8,
};

typedef struct Ch Ch;
struct Ch {
	int	point;
	char	*tag;
	int	hue;	/* -1: none */
	ushort	state;
};

/* space/src/border.ts CHARACTER_TAGS and characters.ts CHARACTER_HUES */
Ch ch[NCH] = {	/* not static: the observer finds it by name (observe.c) */
	{ 1, "METATRON", 0 },
	{ 2, "SOLOMON", 30 },
	{ 3, "SOLON", 60 },
	{ 4, "ASABIYAH", 120 },
	{ 5, "ENOCH", 240 },
	{ 6, "SPEAKER", 270 },
	{ 7, "GENESIS", 300 },
	{ 8, "OBSERVER", -1 },
};
static int selected = -1;
int tick;

static ushort
rotl(ushort x, int n)
{
	return x<<n | x>>(16-n);
}

/* gnomon.ts K: rotl 1 ^ rotl 3 ^ rotr 2 ^ C */
static ushort
K(ushort p, ushort c)
{
	return rotl(p, 1) ^ rotl(p, 3) ^ rotl(p, 14) ^ c;
}

/* plane.ts atHue: hue 0 to the north, clockwise; none, the centre */
static void
athue(int hue, double *u, double *v)
{
	double a;

	if(hue < 0){
		*u = *v = 0;
		return;
	}
	a = (hue - 90) * PI / 180;
	*u = cos(a);
	*v = sin(a);
}

static double
scale(int size)
{
	return (double)RIM / FLOOR * size / 2;
}

/* plane.ts toFloor */
static Point
tofloor(Rectangle r, double u, double v)
{
	int size;
	double k;

	size = Dx(r) < Dy(r) ? Dx(r) : Dy(r);
	k = scale(size);
	return Pt(r.min.x + size/2 + u*k, r.min.y + size/2 + v*k);
}

static ulong
hsl(int hue, int s, int l)
{
	double h, S, L, c, x, m, rr, g, b;

	if(hue < 0)
		s = 0;
	h = (hue < 0 ? 0 : hue) / 60.0;
	S = s / 100.0;
	L = l / 100.0;
	c = (1 - fabs(2*L - 1)) * S;
	x = c * (1 - fabs(fmod(h, 2) - 1));
	m = L - c/2;
	rr = g = b = 0;
	switch((int)h % 6){
	case 0: rr = c; g = x; break;
	case 1: rr = x; g = c; break;
	case 2: g = c; b = x; break;
	case 3: g = x; b = c; break;
	case 4: rr = x; b = c; break;
	case 5: rr = c; b = x; break;
	}
	return (ulong)((rr+m)*255+0.5)<<24 | (ulong)((g+m)*255+0.5)<<16 | (ulong)((b+m)*255+0.5)<<8 | 0xFF;
}

static Image*
paint(ulong rgba)
{
	return allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, rgba);
}

static void
redraw(void)
{
	Rectangle r;
	Point c, p, q;
	Image *im, *white, *text, *rim;
	int size, s, rk, i, mr;
	double k, u, v;
	char buf[32];

	r = screen->r;
	size = Dx(r) < Dy(r) ? Dx(r) : Dy(r);
	k = scale(size);
	c = Pt(r.min.x + size/2, r.min.y + size/2);
	im = paint(0x151C24FF);
	draw(screen, r, im, nil, ZP);
	freeimage(im);
	/* the twelve sectors: canvas angles run clockwise, draw(2)'s counterclockwise */
	rk = k * 520 / 480;
	for(s = 0; s < 12; s++){
		im = paint(hsl(s*30, 70, 14));
		fillarc(screen, c, rk, rk, im, ZP, 75 - s*30, 30);
		freeimage(im);
	}
	rim = paint(0x34404AFF);
	ellipse(screen, c, k, k, size/256, rim, ZP);
	freeimage(rim);
	white = paint(0xFFFFFFFF);
	text = paint(0xEDF0E9FF);
	mr = size / 70;
	for(i = 0; i < NCH; i++){
		athue(ch[i].hue, &u, &v);
		p = tofloor(r, u, v);
		im = paint(hsl(ch[i].hue, 70, 55));
		fillellipse(screen, p, mr, mr, im, ZP);
		freeimage(im);
		if(i == selected)
			ellipse(screen, p, mr*9/5, mr*9/5, size/300 + 1, white, ZP);
		q = stringsize(font, ch[i].tag);
		string(screen, Pt(p.x - q.x/2, p.y + mr*13/5 - q.y/2), text, ZP, font, ch[i].tag);
		snprint(buf, sizeof buf, "%.4uX", ch[i].state);
		q = stringsize(font, buf);
		string(screen, Pt(p.x - q.x/2, p.y + mr*13/5 + q.y/2), text, ZP, font, buf);
	}
	snprint(buf, sizeof buf, "tick %d", tick);
	string(screen, addpt(r.min, Pt(8, 8)), text, ZP, font, buf);
	freeimage(white);
	freeimage(text);
	flushimage(display, 1);
}

/* plane.ts nearest: the character nearest the point, within reach of the rim */
static int
nearest(Point m)
{
	Rectangle r;
	int i, best, size;
	double u, v, mu, mv, d, e, k;

	r = screen->r;
	size = Dx(r) < Dy(r) ? Dx(r) : Dy(r);
	k = scale(size);
	mu = (m.x - r.min.x - size/2) / k;
	mv = (m.y - r.min.y - size/2) / k;
	best = -1;
	d = 0.15;
	for(i = 0; i < NCH; i++){
		athue(ch[i].hue, &u, &v);
		e = hypot(u - mu, v - mv);
		if(e <= d){
			best = i;
			d = e;
		}
	}
	return best;
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	redraw();
}

void
main(int argc, char **argv)
{
	Event e;
	ulong timer;
	int i, k, kbd;

	kbd = Ekeyboard;
	ARGBEGIN{
	case 'o':
		kbd = 0;
		break;
	}ARGEND
	for(i = 0; i < NCH; i++)
		ch[i].state = (ch[i].tag[0] & 0xFF)<<8 | (ch[i].point & 0xFF);	/* characters.ts seedOf */
	if(initdraw(nil, nil, "floor") < 0)
		sysfatal("initdraw: %r");
	einit(Emouse|kbd);
	timer = etimer(0, 1000);
	redraw();
	for(;;){
		k = event(&e);
		if(k == Ekeyboard && (e.kbdc == 'q' || e.kbdc == 0x7F))
			break;
		if(k == Emouse && (e.mouse.buttons & 4))
			break;
		if(k == Emouse && (e.mouse.buttons & 1)){
			selected = nearest(e.mouse.xy);
			redraw();
		}
		if(k == timer){
			for(i = 0; i < NCH; i++)
				ch[i].state = K(ch[i].state, 0x1D);
			tick++;
			redraw();
		}
	}
	closedisplay(display);
	exits(nil);
}
