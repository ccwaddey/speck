/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & \
				 (ShiftMask|ControlMask|Mod1Mask|Mod2Mask \
				  |Mod3Mask|Mod4Mask|Mod5Mask))
#define ISVISIBLE(C)            (((C)->tag == themon->tagset[themon->seltag]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)

/* enums */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw; /* border width */
	unsigned int tag;
	int isfixed, neverfocus, isfullscreen;
	Client *snext; /* stack next */
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

struct Monitor {
	int mw, mh;   /* screen size */
	unsigned int seltag;
	unsigned int tagset[2]; /* There are two for toggling like Alt-Tab */
	Client *sel;
	Client *stack;
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tag;
} Rule;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h);
static void arrange(int dorestack);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detachstack(Client *c);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void maximize(const Arg *arg);
static void minimize(const Arg *arg);
static void movemouse(const Arg *arg);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static void resize(Client *c, int x, int y, int w, int h);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updateclientlist(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);

/* variables */
static const char broken[] = "broken";
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Display *dpy;
static Monitor *themon;
static Window root, wmcheckwin;
static XColor focusbordercolor, unfocusbordercolor, dummy;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* function implementations */
void
applyrules(Client *c) {
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->tag = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
			c->tag = r->tag; /* last rule wins */
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	if (c->tag == 0)
		c->tag = themon->tagset[themon->seltag];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h) {
	int baseismin;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (*x >= themon->mw)
		*x = themon->mw - WIDTH(c);
	if (*y >= themon->mh)
		*y = themon->mh - HEIGHT(c);
	if (*x + *w + 2 * c->bw < 0)
		*x = 0;
	if (*y + *h + 2 * c->bw < 0)
		*y = 0;

	if (!c->hintsvalid)
		updatesizehints(c);
	/* see last two sentences in ICCCM 4.1.2.3 */
	baseismin = c->basew == c->minw && c->baseh == c->minh;
	if (!baseismin) { /* temporarily remove base dimensions */
		*w -= c->basew;
		*h -= c->baseh;
	}
	/* adjust for aspect limits */
	if (c->mina > 0 && c->maxa > 0) {
		if (c->maxa < (float)*w / *h)
			*w = *h * c->maxa + 0.5;
		else if (c->mina < (float)*h / *w)
			*h = *w * c->mina + 0.5;
	}
	if (baseismin) { /* increment calculation requires this */
		*w -= c->basew;
		*h -= c->baseh;
	}
	/* adjust for increment value */
	if (c->incw)
		*w -= *w % c->incw;
	if (c->inch)
		*h -= *h % c->inch;
	/* restore base dimensions */
	*w = MAX(*w + c->basew, c->minw);
	*h = MAX(*h + c->baseh, c->minh);
	if (c->maxw)
		*w = MIN(*w, c->maxw);
	if (c->maxh)
		*h = MIN(*h, c->maxh);
	
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(int dorestack) {
	showhide(themon->stack);
	if (dorestack) {
		restack(themon);
	}
}

void
attachstack(Client *c) {
	c->snext = themon->stack;
	themon->stack = c;
}

void
buttonpress(XEvent *e) {
	unsigned int i, click;
	Client *c;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;

	if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(themon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func &&
		    buttons[i].button == ev->button &&
		    CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(&buttons[i].arg);
}

void
checkotherwm(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void) {
	Client *c;

	for (c = themon->stack; c != NULL; c = c->snext)
		c->tag = themon->seltag;
	while (themon->stack)
		unmanage(themon->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	free(themon);
	XDestroyWindow(dpy, wmcheckwin);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
clientmessage(XEvent *e) {
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) 
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c,
			    (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD */
			    || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */
			    && !c->isfullscreen)));
}

void
configure(Client *c) {
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e) {
	focus(NULL);
	arrange(0);
}

void
configurerequest(XEvent *e) {
	Client *c;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else {
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > themon->mw)
				/* center in x direction */
				c->x = (themon->mw / 2 - WIDTH(c) / 2);
			if ((c->y + c->h) > themon->mh)
				/* center in y direction */
				c->y = (themon->mh / 2 - HEIGHT(c) / 2);
			if ((ev->value_mask & (CWX|CWY)) &&
			    !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w,
						  c->h);
		}
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void) {
	Monitor *m;

	m = ecalloc(1, sizeof(Monitor));
	m->mw = sw - xreserve;
	m->mh = sh - yreserve;
	m->tagset[0] = inittag;
	m->tagset[1] = altinittag;
	return m;
}

void
destroynotify(XEvent *e) {
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
}

void
detachstack(Client *c) {
	Client **tc, *t;

	for (tc = &themon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == themon->sel) {
		for (t = themon->stack; t && !ISVISIBLE(t); t = t->snext);
		themon->sel = t;
	}
}

void
focus(Client *c) {
	if (!c || !ISVISIBLE(c))
		for (c = themon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (themon->sel && themon->sel != c)
		unfocus(themon->sel, 0);
	if (c) {
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, focusbordercolor.pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	themon->sel = c;
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e) {
	XFocusChangeEvent *ev = &e->xfocus;

	if (themon->sel && ev->window != themon->sel->win)
		setfocus(themon->sel);
}

void
focusstack(const Arg *arg) {
	Client *c = NULL, *i;

	if (!themon->sel)
		return;
	if (arg->i > 0) {
		for (c = themon->sel->snext; c && !ISVISIBLE(c); c = c->snext);
		if (!c)
			for (c = themon->stack; c && !ISVISIBLE(c); c = c->snext);
	} else {
		for (i = themon->stack; i != themon->sel; i = i->snext)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->snext)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(themon);
	}
}

Atom
getatomprop(Client *c, Atom prop) {
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

int
getrootptr(int *x, int *y) {
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w) {
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False,
	    wmatom[WMState], &real, &format, &n, &extra, &p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

void
grabbuttons(Client *c, int focused) {
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask,
			numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)	/* This enables click to focus */
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None,
						None);
	}
}

void
grabkeys(void) {
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask,
			numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code,
						keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}

void
keypress(XEvent *e) {
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg) {
	if (!themon->sel)
		return;
	if (!sendevent(themon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XKillClient(dpy, themon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
manage(Window w, XWindowAttributes *wa) {
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->tag = t->tag;
	} else {
		applyrules(c);
	}

	if (c->x + WIDTH(c) > themon->mw)
		c->x = themon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > themon->mh)
		c->y = themon->mh - HEIGHT(c);
	c->x = MAX(c->x, 0);
	c->y = MAX(c->y, 0);

	c->bw = borderpx;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, unfocusbordercolor.pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatewmhints(c);
	XSelectInput(dpy, w, FocusChangeMask|PropertyChangeMask);
	grabbuttons(c, 0);

	XRaiseWindow(dpy, c->win);

	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
	    PropModeAppend, (unsigned char *) &(c->win), 1);
	/* some windows require this */
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h);
	setclientstate(c, NormalState);
	unfocus(themon->sel, 0);
	themon->sel = c;
	arrange(1);
	XMapWindow(dpy, c->win);
	focus(NULL);
}

void
mappingnotify(XEvent *e) {
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e) {
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
maximize(const Arg *arg) {
	Client *c = themon->sel;

	if (!c || c->isfullscreen || c->isfixed)
		return;
	resize(c, 0, 0, themon->mw - 2*c->bw, themon->mh - 2*c->bw);
}	

void
minimize(const Arg *arg) {
	Client *c = themon->sel;

	if (!c || c->isfullscreen || c->isfixed)
		return;
	resize(c, c->oldx, c->oldy, c->oldw, c->oldh);
}

void
movemouse(const Arg *arg) {
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = themon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(themon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, None, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|SubstructureRedirectMask,
			   &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(nx) < snap)
				nx = 0;
			else if (abs(themon->mw - (nx + WIDTH(c))) < snap)
				nx = themon->mw - WIDTH(c);
			if (abs(ny) < snap)
				ny = 0;
			else if (abs(themon->mh - (ny + HEIGHT(c))) < snap)
				ny = themon->mh - HEIGHT(c);
			
			resize(c, nx, ny, c->w, c->h);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
}

void
propertynotify(XEvent *e) {
	Client *c;
	XPropertyEvent *ev = &e->xproperty;

	if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			break;
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg) {
	running = 0;
}

void
resize(Client *c, int x, int y, int w, int h) {
	if (applysizehints(c, &x, &y, &w, &h))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h) {
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg) {
	int ocx, ocy, nw, nh;
	Client *c;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = themon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(themon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, None, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
		     c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|SubstructureRedirectMask,
			   &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (nw >= 0 && nw <= themon->mw && nh >= 0 &&
			    nh <= themon->mh)
				resize(c, c->x, c->y, nw, nh);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
		     c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
}

void
restack(Monitor *m) {
	if (!m->sel)
		return;
	XRaiseWindow(dpy, m->sel->win);
	XSync(dpy, False);
}

void
run(void) {
	XEvent ev;

	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
scan(void) {
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			    || wa.override_redirect
			    || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable
			    || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			    && (wa.map_state == IsViewable
				|| getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
setclientstate(Client *c, long state) {
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *c, Atom proto) {
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void
setfocus(Client *c) {
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen) {
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
		    PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldbw = c->bw;
		c->bw = 0;
		resizeclient(c, 0, 0, themon->mw, themon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
		    PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(1);
	}
}

void
setup(void) {
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	themon = createmon();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] =
	    XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] =
	    XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init border colors */
	XAllocNamedColor(dpy, DefaultColormap(dpy, screen), col_focus,
		     &focusbordercolor, &dummy);
	XAllocNamedColor(dpy, DefaultColormap(dpy, screen), col_unfocus,
		     &unfocusbordercolor, &dummy);
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "speck", 5);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask;
	XChangeWindowAttributes(dpy, root, CWEventMask, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void
showhide(Client *c) {
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if (!c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
sigchld(int unused) {
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg) {
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "speck: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void
tag(const Arg *arg) {
	if (themon->sel && arg->ui && arg->ui <= numtags) {
		themon->sel->tag = arg->ui;
		focus(NULL);
		arrange(1);
	}
}

void
unfocus(Client *c, int setfocus) {
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, unfocusbordercolor.pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed) {
	XWindowChanges wc;

	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		/* restore border */
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(1);
}

void
unmapnotify(XEvent *e) {
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}

void
updateclientlist() {
	Client *c;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (c = themon->stack; c; c = c->snext)
		XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}

void
updatenumlockmask(void) {
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c) {
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = 0;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw &&
		      c->maxh == c->minh);
	c->hintsvalid = 1;
}

void
updatewindowtype(Client *c) {
	Atom state = getatomprop(c, netatom[NetWMState]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
}

void
updatewmhints(Client *c) {
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg) {
	if (arg->ui == themon->tagset[themon->seltag])
		return;
	themon->seltag ^= 1; /* toggle sel tagset */
	if (arg->ui && arg->ui <= numtags)
		themon->tagset[themon->seltag] = arg->ui;
	focus(NULL);
	arrange(1);
}

Client *
wintoclient(Window w) {
	Client *c;

	for (c = themon->stack; c; c = c->snext)
		if (c->win == w)
			return c;
	return NULL;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee) {
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle
	    && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "speck: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee) {
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee) {
	die("speck: another window manager is already running");
	return -1;
}

int
main(int argc, char *argv[]) {
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("speck-"VERSION);
	else if (argc != 1)
		die("usage: speck [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("speck: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
