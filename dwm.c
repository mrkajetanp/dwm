/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
/* #define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \ */
                               /* * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy))) */
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->mx+(m)->mw) - MAX((x),(m)->mx)) \
                               * MAX(0, MIN((y)+(h),(m)->my+(m)->mh) - MAX((y),(m)->my)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]) || C->issticky)
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TAGSLENGTH              (LENGTH(tags))
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeUrg }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetDesktopNames, NetDesktopViewport, NetNumberOfDesktops, NetCurrentDesktop, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMWindowRole, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef struct TagState TagState;
struct TagState {
	int selected;
	int occupied;
	int urgent;
};

typedef struct ClientState ClientState;
struct ClientState {
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
};

typedef union {
	long i;
	unsigned long ui;
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
	float cfact;
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, isterminal, noswallow, issticky;
	int issteam;
	int floatborderpx;
	pid_t pid;
	Client *next;
	Client *snext;
	Client *swallowing;
	Monitor *mon;
	Window win;
	ClientState prevstate;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char * sig;
	void (*func)(const Arg *);
} Signal;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct {
	int x;
	int y;
	int w;
	int h;
} Inset;

typedef struct Pertag Pertag;
struct Monitor {
	char ltsymbol[16];
	char lastltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by, bh;           /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	TagState tagstate;
	int rmaster;
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *lastsel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
	const Layout *lastlt;
	Inset inset;
	Pertag *pertag;
};

typedef struct {
	const char *class;
	const char *role;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isterminal;
	int noswallow;
	int monitor;
	int floatx, floaty, floatw, floath;
	int floatborderpx;
} Rule;

/* Xresources preferences */
enum resource_type {
	STRING = 0,
	INTEGER = 1,
	FLOAT = 2
};

typedef struct {
	char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

typedef struct {
	int monitor;
	int tag;
	int layout;
	float mfact;
	int nmaster;
	int showbar;
	int topbar;
} MonitorRule;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachabove(Client *c);
static void attachstack(Client *c);
static int fake_signal(void);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void cyclelayout(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static Client *findbefore(Client *c);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static int handlexevent(struct epoll_event *ev);
static void incnmaster(const Arg *arg);
static void inplacerotate(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void managealtbar(Window win, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
/* static void pop(Client *); */
static Client *prevtiled(Client *c);
static void propertynotify(XEvent *e);
static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void runautostart(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setcurrentdesktop(void);
static void setdesktopnames(void);
static void setnumdesktops(void);
static void setviewport(void);
static void updatecurrentdesktop(void);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setlayoutsafe(const Arg *arg);
static void setmark(Client *c);
static void swapclient(const Arg *arg);
static void swapfocus(const Arg *arg);
static void togglemark(const Arg *arg);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void setupepoll(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void sighup(int unused);
static void sigterm(int unused);
static void spawn(const Arg *arg);
static void spawnbar();
static void switchcol(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tagallmon(const Arg *arg);
static void tagtoleft(const Arg *arg);
static void tagtoright(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglescratch(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void togglesticky(const Arg *arg);
static void togglermaster(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmanagealtbar(Window w);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static void warp(const Client *c);
static void viewtoleft(const Arg *arg);
static void viewtoright(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int wmclasscontains(Window win, const char *class, const char *name);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);
static void load_xresources(void);
static void resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst);

static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static Client *swallowingclient(Window w);
static Client *termforwin(const Client *c);
static pid_t winpid(Window w);

static void bstack(Monitor *m);
static void bstackhoriz(Monitor *m);
static void centeredmaster(Monitor *m);
static void centeredfloatingmaster(Monitor *m);
static void deck(Monitor *m);
static void grid(Monitor *m);
static void tatami(Monitor *m);

static void roundcorners(Client *c);

/* variables */
static const char autostartblocksh[] = "autostart_blocking.sh";
static const char autostartsh[] = "autostart.sh";
static Client *prevzoom = NULL;
static const char broken[] = "broken";
static const char dwmdir[] = "dwm";
static const char localshare[] = ".local/share";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int lrpad;            /* sum of left and right padding for text */
static int vp;               /* vertical padding for bar */
static int sp;               /* side padding for bar */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int epoll_fd;
static int dpy_fd;
static int restart = 0;
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon, *lastselmon;
static Window root, wmcheckwin;
static Client *mark;

static xcb_connection_t *xcon;

#include "ipc.h"

/* configuration, allows nested code to access above variables */
#include "config.h"

#ifdef VERSION
#include "IPCClient.c"
#include "yajl_dumps.c"
#include "ipc.c"
#endif

struct Pertag {
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
	int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

static unsigned int scratchtag = 1 << LENGTH(tags);

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	char role[64];
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;
	gettextprop(c->win, wmatom[WMWindowRole], role, sizeof(role));

	if (strstr(class, "Steam") || strstr(class, "steam_app_"))
		c->issteam = 1;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->role || strstr(role, r->role))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isterminal = r->isterminal;
			c->noswallow  = r->noswallow;
			c->isfloating = r->isfloating;
			c->tags |= r->tags;

			c->floatborderpx = r->floatborderpx;
			if (r->isfloating) {
				c->x = r->floatx;
				c->y = r->floaty;
				c->w = r->floatw;
				c->h = r->floath;
			}

			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
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
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachabove(Client *c)
{
	if (c->mon->sel == NULL || c->mon->sel == c->mon->clients || c->mon->sel->isfloating) {
		attach(c);
		return;
	}

	Client *at;
	for (at = c->mon->clients; at->next != c->mon->sel; at = at->next);
	c->next = at->next;
	at->next = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
swallow(Client *p, Client *c)
{

	if (c->noswallow || c->isterminal)
		return;
	if (c->noswallow && !swallowfloating && c->isfloating)
		return;

	detach(c);
	detachstack(c);

	setclientstate(c, WithdrawnState);
	XUnmapWindow(dpy, p->win);

	p->swallowing = c;
	c->mon = p->mon;

	Window w = p->win;
	p->win = c->win;
	c->win = w;
	updatetitle(p);
	XMoveResizeWindow(dpy, p->win, p->x, p->y, p->w, p->h);
	arrange(p->mon);
	configure(p);
	updateclientlist();
}

void
unswallow(Client *c)
{
	c->win = c->swallowing->win;

	free(c->swallowing);
	c->swallowing = NULL;

	/* unfullscreen the client */
	setfullscreen(c, 0);
	updatetitle(c);
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
	setclientstate(c, NormalState);
	focus(NULL);
	arrange(c->mon);
}

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + blw)
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - (int)TEXTW(stext))
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);

	ipc_cleanup();

	if (close(epoll_fd) < 0) {
			fprintf(stderr, "Failed to close epoll file descriptor\n");
	}
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	if (!usealtbar) {
		XUnmapWindow(dpy, mon->barwin);
		XDestroyWindow(dpy, mon->barwin);
	}
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
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
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx + sp, m->by + vp, m->ww -  2 * sp, m->bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (!c->issteam) {
				if (ev->value_mask & CWX) {
					c->oldx = c->x;
					c->x = m->mx + ev->x;
				}
				if (ev->value_mask & CWY) {
					c->oldy = c->y;
					c->y = m->my + ev->y;
				}
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
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
createmon(void)
{
	Monitor *m, *mon;
	unsigned int i;
	int mi, j, layout;
	const MonitorRule *mr;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->rmaster = rmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->bh = bh;
	/* m->lt[0] = &layouts[0]; */
	/* m->lt[1] = &layouts[1 % LENGTH(layouts)]; */
	/* strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol); */

	for (mi = 0, mon = mons; mon; mon = mon->next, mi++);
	for (j = 0; j < LENGTH(monrules); j++) {
		mr = &monrules[j];
		if ((mr->monitor == -1 || mr->monitor == mi)
				&& (mr->tag <= 0 || (m->tagset[0] & (1 << (mr->tag - 1))))
		) {
			layout = MAX(mr->layout, 0);
			layout = MIN(layout, LENGTH(layouts) - 1);
			m->lt[0] = &layouts[layout];
			m->lt[1] = &layouts[1 % LENGTH(layouts)];
			strncpy(m->ltsymbol, layouts[layout].symbol, sizeof m->ltsymbol);

			if (mr->mfact > -1)
				m->mfact = mr->mfact;
			if (mr->nmaster > -1)
				m->nmaster = mr->nmaster;
			if (mr->showbar > -1)
				m->showbar = mr->showbar;
			if (mr->topbar > -1)
				m->topbar = mr->topbar;
			break;
		}
	}

	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->sellts[i] = m->sellt;

		for (j = 0; j < LENGTH(monrules); j++) {
			mr = &monrules[j];
			if ((mr->monitor == -1 || mr->monitor == mi) && (mr->tag == -1 || mr->tag == i)) {
				layout = MAX(mr->layout, 0);
				layout = MIN(layout, LENGTH(layouts) - 1);
				m->pertag->ltidxs[i][0] = &layouts[layout];
				m->pertag->ltidxs[i][1] = m->lt[0];
				m->pertag->nmasters[i] = (mr->nmaster > -1 ? mr->nmaster : m->nmaster);
				m->pertag->mfacts[i] = (mr->mfact > -1 ? mr->mfact : m->mfact);
				m->pertag->showbars[i] = (mr->showbar > -1 ? mr->showbar : m->showbar);
				break;
			}
		}
	}

	m->inset = default_inset;
	return m;
}

void
cyclelayout(const Arg *arg) {
	Layout *l;
	for(l = (Layout *)layouts; l != selmon->lt[selmon->sellt]; l++);
	if(arg->i > 0) {
		if(l->symbol && (l + 1)->symbol)
			setlayout(&((Arg) { .v = (l + 1) }));
		else
			setlayout(&((Arg) { .v = layouts }));
	} else {
		if(l != layouts && (l - 1)->symbol)
			setlayout(&((Arg) { .v = (l - 1) }));
		else
			setlayout(&((Arg) { .v = &layouts[LENGTH(layouts) - 2] }));
	}
}

void
destroynotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else {
		if ((c = swallowingclient(ev->window)))
			unmanage(c->swallowing, 1);
		else if ((m = wintomon(ev->window)) && m->barwin == ev->window)
			unmanagealtbar(ev->window);
	}
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawbar(Monitor *m)
{
	if (usealtbar)
		return;

	int x, w, tw = 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		drw_setscheme(drw, scheme[SchemeNorm]);
		tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
		drw_text(drw, m->ww - tw - 2 * sp, 0, tw, bh, 0, stext, 0);
	}

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
				urg & 1 << i);
		x += w;
	}
	w = blw = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	if ((w = m->ww - tw - x) > bh) {
		if (m->sel) {
			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
			drw_text(drw, x, 0, w - 2 * sp, bh, lrpad / 2, m->sel->name, 0);
			if (m->sel->isfloating)
				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		} else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w - 2 * sp, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

Client *
findbefore(Client *c)
{
	Client *tmp;
	if (c == selmon->clients)
		return NULL;
	for (tmp = selmon->clients; tmp && tmp->next != c; tmp = tmp->next);
	return tmp;
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		if (c == mark)
			XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColMark].pixel);
		else
			XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel)
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
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
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}

int
handlexevent(struct epoll_event *ev)
{
	if (ev->events & EPOLLIN) {
		XEvent ev;
		while (running && XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (handler[ev.type]) {
				handler[ev.type](&ev); /* call handler */
				ipc_send_events(mons, &lastselmon, selmon);
			}
		}
	} else if (ev-> events & EPOLLHUP) {
		return -1;
	}

	return 0;
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
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

int
fake_signal(void)
{
	char fsignal[256];
	char indicator[9] = "fsignal:";
	char str_sig[50];
	char param[16];
	int i, len_str_sig, n, paramn;
	size_t len_fsignal, len_indicator = strlen(indicator);
	Arg arg;

	// Get root name property
	if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
		len_fsignal = strlen(fsignal);

		// Check if this is indeed a fake signal
		if (len_indicator > len_fsignal ? 0 : strncmp(indicator, fsignal, len_indicator) == 0) {
			paramn = sscanf(fsignal+len_indicator, "%s%n%s%n", str_sig, &len_str_sig, param, &n);

			if (paramn == 1) arg = (Arg) {0};
			else if (paramn > 2) return 1;
			else if (strncmp(param, "i", n - len_str_sig) == 0)
				sscanf(fsignal + len_indicator + n, "%i", (int*)&(arg.i));
			else if (strncmp(param, "ui", n - len_str_sig) == 0)
				sscanf(fsignal + len_indicator + n, "%u", (unsigned int*)&(arg.ui));
			else if (strncmp(param, "f", n - len_str_sig) == 0)
				sscanf(fsignal + len_indicator + n, "%f", &(arg.f));
			else return 1;

			// Check if a signal was found, and if so handle it
			for (i = 0; i < LENGTH(signals); i++)
				if (strncmp(str_sig, signals[i].sig, len_str_sig) == 0 && signals[i].func)
					signals[i].func(&(arg));

			// A fake signal was sent
			return 1;
		}
	}

	// No fake signal was sent, so proceed with update
	return 0;
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL, *term = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->pid = winpid(w);
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->cfact = 1.0;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
		term = termforwin(c);
	}

	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
		&& (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = borderpx;

	selmon->tagset[selmon->seltags] &= ~scratchtag;
	if (!strcmp(c->name, scratchpadname)) {
		c->mon->tagset[c->mon->seltags] |= c->tags = scratchtag;
		c->isfloating = True;
		c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
		c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
	}

	if (c->isfloating)
		wc.border_width = c->floatborderpx;
	else
		wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	if (c == mark)
		XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColMark].pixel);
	else
		XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	c->sfx = c->x;
	c->sfy = c->y;
	c->sfw = c->w;
	c->sfh = c->h;
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	/* attach(c); */
	attachabove(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	if (term)
		swallow(term, c);
	focus(NULL);
}

void
managealtbar(Window win, XWindowAttributes *wa)
{
	Monitor *m;
	if (!(m = recttomon(wa->x, wa->y, wa->width, wa->height)))
		return;

	m->barwin = win;
	m->by = wa->y;
	bh = m->bh = wa->height;
	updatebarpos(m);
	arrange(m);
	XSelectInput(dpy, win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	XMoveResizeWindow(dpy, win, wa->x, wa->y, wa->width, wa->height);
	XMapWindow(dpy, win);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &win, 1);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (wmclasscontains(ev->window, altbarclass, ""))
		managealtbar(ev->window, &wa);
	else if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
	Client *c;

	/* unsigned int n = 0; */
	/* for (c = m->clients; c; c = c->next) */
	/* 	if (ISVISIBLE(c)) */
	/* 		n++; */
	/* if (n > 0) /\* override layout symbol *\/ */
	/* 	snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n); */

	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

/* void */
/* pop(Client *c) */
/* { */
/* 	detach(c); */
/* 	attach(c); */
/* 	focus(c); */
/* 	arrange(c->mon); */
/* } */

Client *
prevtiled(Client *c) {
	Client *p, *r;

	for(p = selmon->clients, r = NULL; p && p != c; p = p->next)
		if(!p->isfloating && ISVISIBLE(p))
			r = p;
	return r;
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
		if (!fake_signal())
			updatestatus();
	}
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
pushdown(const Arg *arg) {
	Client *sel = selmon->sel, *c;

	if(!sel || sel->isfloating || sel == nexttiled(selmon->clients))
		return;
	if((c = nexttiled(sel->next))) {
		detach(sel);
		sel->next = c->next;
		c->next = sel;
	}
	focus(sel);
	arrange(selmon);
}

void
pushup(const Arg *arg) {
	Client *sel = selmon->sel, *c;

	if(!sel || sel->isfloating)
		return;
	if((c = prevtiled(sel)) && c != nexttiled(selmon->clients)) {
		detach(sel);
		sel->next = c;
		for(c = selmon->clients; c->next != sel->next; c = c->next);
		c->next = sel;
	}
	focus(sel);
	arrange(selmon);
}

void
quit(const Arg *arg)
{
	if(arg->i) restart = 1;
	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;
	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;

	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	roundcorners(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

    for (c = m->stack; c; c = c->snext)
		roundcorners(c);

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if (m == selmon && (m->tagset[m->seltags] & m->sel->tags) && m->lt[m->sellt]->arrange != &monocle)
		warp(m->sel);
}

void roundcorners(Client *c) {
	Window w = c->win;
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, w, &wa);

    // If this returns null, the window is invalid.
    if(!XGetWindowAttributes(dpy, w, &wa))
        return;

    int width = borderpx * 2 + wa.width;
    int height = borderpx * 2 + wa.height;
    /* int width = win_attr.border_width * 2 + win_attr.width; */
    /* int height = win_attr.border_width * 2 + win_attr.height; */
    int rad = cornerrad;
    int dia = 2 * rad;

    // do not try to round if the window would be smaller than the corners
    if(width < dia || height < dia)
        return;

    Pixmap mask = XCreatePixmap(dpy, w, width, height, 1);
    // if this returns null, the mask is not drawable
    if(!mask)
        return;

    XGCValues xgcv;
    GC shape_gc = XCreateGC(dpy, mask, 0, &xgcv);
    if(!shape_gc) {
        XFreePixmap(dpy, mask);
        return;
    }

    XSetForeground(dpy, shape_gc, 0);
    XFillRectangle(dpy, mask, shape_gc, 0, 0, width, height);
    XSetForeground(dpy, shape_gc, 1);
    XFillArc(dpy, mask, shape_gc, 0, 0, dia, dia, 0, 23040);
    XFillArc(dpy, mask, shape_gc, width-dia-1, 0, dia, dia, 0, 23040);
    XFillArc(dpy, mask, shape_gc, 0, height-dia-1, dia, dia, 0, 23040);
    XFillArc(dpy, mask, shape_gc, width-dia-1, height-dia-1, dia, dia, 0, 23040);
    XFillRectangle(dpy, mask, shape_gc, rad, 0, width-dia, height);
    XFillRectangle(dpy, mask, shape_gc, 0, rad, width, height-dia);
    XShapeCombineMask(dpy, w, ShapeBounding, 0-wa.border_width, 0-wa.border_width, mask, ShapeSet);
    XFreePixmap(dpy, mask);
    XFreeGC(dpy, shape_gc);
 }

void
run(void)
{
	int event_count = 0;
	const int MAX_EVENTS = 10;
	struct epoll_event events[MAX_EVENTS];

	XSync(dpy, False);

	/* main event loop */
	while (running) {
		event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

		for (int i = 0; i < event_count; i++) {
			int event_fd = events[i].data.fd;
			DEBUG("Got event from fd %d\n", event_fd);

			if (event_fd == dpy_fd) {
				// -1 means EPOLLHUP
				if (handlexevent(events + i) == -1)
					return;
			} else if (event_fd == ipc_get_sock_fd()) {
				ipc_handle_socket_epoll_event(events + i);
			} else if (ipc_is_client_registered(event_fd)){
				if (ipc_handle_client_epoll_event(events + i, mons, &lastselmon, selmon,
							tags, LENGTH(tags), layouts, LENGTH(layouts)) < 0) {
					fprintf(stderr, "Error handling IPC event on fd %d\n", event_fd);
				}
			} else {
				fprintf(stderr, "Got event from unknown fd %d, ptr %p, u32 %d, u64 %lu",
						event_fd, events[i].data.ptr, events[i].data.u32,
						events[i].data.u64);
				fprintf(stderr, " with events %d\n", events[i].events);
				return;
			}
		}
	}
}

void
runautostart(void)
{
	char *pathpfx;
	char *path;
	char *xdgdatahome;
	char *home;
	struct stat sb;

	if ((home = getenv("HOME")) == NULL)
		/* this is almost impossible */
		return;

	/* if $XDG_DATA_HOME is set and not empty, use $XDG_DATA_HOME/dwm,
	 * otherwise use ~/.local/share/dwm as autostart script directory
	 */
	xdgdatahome = getenv("XDG_DATA_HOME");
	if (xdgdatahome != NULL && *xdgdatahome != '\0') {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(xdgdatahome) + strlen(dwmdir) + 2);

		if (sprintf(pathpfx, "%s/%s", xdgdatahome, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	} else {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(home) + strlen(localshare)
		                     + strlen(dwmdir) + 3);

		if (sprintf(pathpfx, "%s/%s/%s", home, localshare, dwmdir) < 0) {
			free(pathpfx);
			return;
		}
	}

	/* check if the autostart script directory exists */
	if (! (stat(pathpfx, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		/* the XDG conformant path does not exist or is no directory
		 * so we try ~/.dwm instead
		 */
		char *pathpfx_new = realloc(pathpfx, strlen(home) + strlen(dwmdir) + 3);
		if(pathpfx_new == NULL) {
			free(pathpfx);
			return;
		}
   pathpfx = pathpfx_new;

		if (sprintf(pathpfx, "%s/.%s", home, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	}

	/* try the blocking script first */
	path = ecalloc(1, strlen(pathpfx) + strlen(autostartblocksh) + 2);
	if (sprintf(path, "%s/%s", pathpfx, autostartblocksh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(path);

	/* now the non-blocking script */
	if (sprintf(path, "%s/%s", pathpfx, autostartsh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(strcat(path, " &"));

	free(pathpfx);
	free(path);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wmclasscontains(wins[i], altbarclass, ""))
				managealtbar(wins[i], &wa);
			else if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	/* attach(c); */
	attachabove(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}
void
setcurrentdesktop(void){
	long data[] = { 0 };
	XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}
void setdesktopnames(void){
	XTextProperty text;
	Xutf8TextListToTextProperty(dpy, (char**)tags, TAGSLENGTH, XUTF8StringStyle, &text);
	XSetTextProperty(dpy, root, &text, netatom[NetDesktopNames]);
}

int
sendevent(Client *c, Atom proto)
{
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
setnumdesktops(void){
	long data[] = { TAGSLENGTH };
	XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

/* void */
/* setinset(Monitor *m, Inset inset) */
/* { */
/* 	m->inset = inset; */
/* 	updatebarpos(m); */
/* 	arrange(m); */
/* } */

/* void */
/* updateinset(const Arg *arg) */
/* { */
/* 	Inset *inset = (Inset *)arg->v; */

/* 	for (Monitor *m = mons; m; m = m->next) */
/* 		setinset(m, *inset); */
/* } */

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void setcfact(const Arg *arg) {
	float f;
	Client *c;

	c = selmon->sel;

	if(!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f + c->cfact;
	if(arg->f == 0.0)
		f = 1.0;
	else if(f < 0.25 || f > 4.0)
		return;
	c->cfact = f;
	arrange(selmon);
}

void
setmark(Client *c)
{
	if (c == mark)
		return;
	if (mark) {
		XSetWindowBorder(dpy, mark->win, scheme[mark == selmon->sel
				? SchemeSel : SchemeNorm][ColBorder].pixel);
		mark = 0;
	}
	if (c) {
		XSetWindowBorder(dpy, c->win, scheme[c == selmon->sel
				? SchemeSel : SchemeNorm][ColMark].pixel);
		mark = c;
	}
}


/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = usealtbar ? 0 : drw->fonts->h + 2;
	updategeom();
	sp = sidepad;
	vp = (topbar == 1) ? vertpad : - vertpad;

	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	wmatom[WMWindowRole] = XInternAtom(dpy, "WM_WINDOW_ROLE", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetDesktopViewport] = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);
	netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 4);
	/* init bars */
	updatebars();
	updatestatus();
	updatebarpos(selmon);
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	setnumdesktops();
	setcurrentdesktop();
	setdesktopnames();
	setviewport();
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
	setupepoll();
	spawnbar();
}

void
setviewport(void){
	long data[] = { 0, 0 };
	XChangeProperty(dpy, root, netatom[NetDesktopViewport], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 2);
}


void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void sighup(int unused) {
	Arg a = {.i = 1};
	quit(&a);
}

void sigterm(int unused) {
	Arg a = {.i = 0};
	quit(&a);
}

void
spawn(const Arg *arg)
{
	selmon->tagset[selmon->seltags] &= ~scratchtag;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void
swapclient(const Arg *arg)
{
	Client *s, *m, t;

	if (!mark || !selmon->sel || mark == selmon->sel
	    || !selmon->lt[selmon->sellt]->arrange)
		return;
	s = selmon->sel;
	m = mark;
	t = *s;
	strcpy(s->name, m->name);
	s->win = m->win;
	s->x = m->x;
	s->y = m->y;
	s->w = m->w;
	s->h = m->h;

	m->win = t.win;
	strcpy(m->name, t.name);
	m->x = t.x;
	m->y = t.y;
	m->w = t.w;
	m->h = t.h;

	selmon->sel = m;
	mark = s;
	focus(s);
	setmark(m);

	arrange(s->mon);
	if (s->mon != m->mon) {
		arrange(m->mon);
	}
}

void
swapfocus(const Arg *arg)
{
	Client *t;

	if (!selmon->sel || !mark || selmon->sel == mark)
		return;
	t = selmon->sel;
	if (mark->mon != selmon) {
		unfocus(selmon->sel, 0);
		selmon = mark->mon;
	}
	if (ISVISIBLE(mark)) {
		focus(mark);
		restack(selmon);
	} else {
		selmon->seltags ^= 1;
		selmon->tagset[selmon->seltags] = mark->tags;
		focus(mark);
		arrange(selmon);
	}
	setmark(t);
}

void
switchcol(const Arg *arg)
{
	Client *c, *t;
	int col = 0;
	int i;

	if (!selmon->sel)
		return;
	for (i = 0, c = nexttiled(selmon->clients); c ;
	     c = nexttiled(c->next), i++) {
		if (c == selmon->sel)
			col = (i + 1) > selmon->nmaster;
	}
	if (i <= selmon->nmaster)
		return;
	for (c = selmon->stack; c; c = c->snext) {
		if (!ISVISIBLE(c))
			continue;
		for (i = 0, t = nexttiled(selmon->clients); t && t != c;
		     t = nexttiled(t->next), i++);
		if (t && (i + 1 > selmon->nmaster) != col) {
			focus(c);
			restack(selmon);
			break;
		}
	}
}

void
spawnbar()
{
	if (*altbarcmd)
		system(altbarcmd);
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
		if(viewontag)
			view(arg);
	}
	updatecurrentdesktop();
}

void
tagmon(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c || !mons->next)
		return;
	if (c->isfullscreen) {
		c->isfullscreen = 0;
		sendmon(c, dirtomon(arg->i));
		c->isfullscreen = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else
		sendmon(c, dirtomon(arg->i));
}

void
tagtoleft(const Arg *arg) {
	if(selmon->sel != NULL
	&& __builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1
	&& selmon->tagset[selmon->seltags] > 1) {
		selmon->sel->tags >>= 1;
		focus(NULL);
		arrange(selmon);
	}
	updatecurrentdesktop();
}

void
tagtoright(const Arg *arg) {
	if(selmon->sel != NULL
	&& __builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1
	&& selmon->tagset[selmon->seltags] & (TAGMASK >> 1)) {
		selmon->sel->tags <<= 1;
		focus(NULL);
		arrange(selmon);
	}
}


void
tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	float mfacts = 0, sfacts = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;
	}
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster
			? m->ww * (m->rmaster ? 1.0 - m->mfact : m->mfact)
			: 0;
	else
		mw = m->ww;
	for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (m->wh - my) * (c->cfact / mfacts);
			/* resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0); */
			resize(c, m->rmaster ? m->wx + m->ww - mw : m->wx,
			       m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
			if (my + HEIGHT(c) < m->wh)
				my += HEIGHT(c);
     mfacts -= c->cfact;
		} else {
			h = (m->wh - ty) * (c->cfact / sfacts);
			/* resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0); */
			resize(c, m->rmaster ? m->wx : m->wx + mw, m->wy + ty,
			       m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
			if (ty + HEIGHT(c) < m->wh)
				ty += HEIGHT(c);
     sfacts -= c->cfact;
		}
}

void
togglermaster(const Arg *arg)
{
	selmon->rmaster = !selmon->rmaster;
	/* now mfact represents the left factor */
	selmon->mfact = 1.0 - selmon->mfact;
	if (selmon->lt[selmon->sellt]->arrange)
		arrange(selmon);
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx + sp, selmon->by + vp, selmon->ww - 2 * sp, selmon->bh);
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		/* restore last known float dimensions */
		resize(selmon->sel, selmon->sel->sfx, selmon->sel->sfy,
		       selmon->sel->sfw, selmon->sel->sfh, False);
	else {
		/* save last known float dimensions */
		selmon->sel->sfx = selmon->sel->x;
		selmon->sel->sfy = selmon->sel->y;
		selmon->sel->sfw = selmon->sel->w;
		selmon->sel->sfh = selmon->sel->h;
	}
	arrange(selmon);
}

void
togglescratch(const Arg *arg)
{
	Client *c;
	unsigned int found = 0;

	for (c = selmon->clients; c && !(found = c->tags & scratchtag); c = c->next);
	if (found) {
		unsigned int newtagset = selmon->tagset[selmon->seltags] ^ scratchtag;
		if (newtagset) {
			selmon->tagset[selmon->seltags] = newtagset;
			focus(NULL);
			arrange(selmon);
		}
		if (ISVISIBLE(c)) {
			focus(c);
			restack(selmon);
		}
	} else
		spawn(arg);
}

void
togglemark(const Arg *arg)
{
	if (!selmon->sel)
		return;
	setmark(selmon->sel == mark ? 0 : selmon->sel);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int i;

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;

		if (newtagset == ~0) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}

		/* test if the user did not select the same tag */
		if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}

		/* apply settings for this view */
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
		selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
			togglebar(NULL);

		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	if (c == mark)
		XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColMark].pixel);
	else
		XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	if (c == mark)
		setmark(0);

	if (c->swallowing) {
		unswallow(c);
		return;
	}

	Client *s = swallowingclient(c->win);
	if (s) {
		free(s->swallowing);
		s->swallowing = NULL;
		arrange(m);
		focus(NULL);
		return;
	}

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);

	if (!s) {
		arrange(m);
		focus(NULL);
		updateclientlist();
	}
}

void
unmanagealtbar(Window w)
{
    Monitor *m = wintomon(w);

    if (!m)
        return;

    m->barwin = 0;
    m->by = 0;
    m->bh = 0;
    updatebarpos(m);
    arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	} else if ((m = wintomon(ev->window)) && m->barwin == ev->window)
		unmanagealtbar(ev->window);
}

void
updatebars(void)
{
	if (usealtbar)
		return;

	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx + sp, m->by + vp, m->ww - 2 * sp, bh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh = m->wh - vertpad - bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh + vertpad;
		m->wy = m->topbar ? m->wy + bh + vp : m->wy;
	} else
		m->by = -bh - vp;

	// Custom insets
	Inset inset = m->inset;
	m->wx += inset.x;
	m->wy += inset.y;
	m->ww -= inset.w + inset.x;
	m->wh -= inset.h + inset.y;
}

void
updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}
void updatecurrentdesktop(void){
	long rawdata[] = { selmon->tagset[selmon->seltags] };
	int i=0;
	while(*rawdata >> (i+1)){
		i++;
	}
	long data[] = { i };
	XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					/* attach(c); */
					attachabove(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
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
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
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
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
	drawbar(selmon);
}

void
updatetitle(Client *c)
{
	char oldname[sizeof(c->name)];
	strcpy(oldname, c->name);

	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);

	for (Monitor *m = mons; m; m = m->next) {
		if (m->sel == c && strcmp(oldname, c->name) != 0)
			ipc_focused_title_change_event(m->num, c->win, oldname, c->name);
	}
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else {
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
			if (c->isurgent)
				XSetWindowBorder(dpy, c->win, scheme[SchemeUrg][ColBorder].pixel);
		}
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	int i;
	unsigned int tmptag;

	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
		togglebar(NULL);

	focus(NULL);
	arrange(selmon);
	updatecurrentdesktop();
}

void
viewtoleft(const Arg *arg) {
	if(__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1
	&& selmon->tagset[selmon->seltags] > 1) {
		selmon->seltags ^= 1; /* toggle sel tagset */
		selmon->tagset[selmon->seltags] = selmon->tagset[selmon->seltags ^ 1] >> 1;
		focus(NULL);
		arrange(selmon);
	}
}

void
viewtoright(const Arg *arg) {
	if(__builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1
	&& selmon->tagset[selmon->seltags] & (TAGMASK >> 1)) {
		selmon->seltags ^= 1; /* toggle sel tagset */
		selmon->tagset[selmon->seltags] = selmon->tagset[selmon->seltags ^ 1] << 1;
		focus(NULL);
		arrange(selmon);
	}
}


pid_t
winpid(Window w)
{

	pid_t result = 0;

	#ifdef __linux__
	xcb_res_client_id_spec_t spec = {0};
	spec.client = w;
	spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

	xcb_generic_error_t *e = NULL;
	xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
	xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

	if (!r)
		return (pid_t)0;

	xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
	for (; i.rem; xcb_res_client_id_value_next(&i)) {
		spec = i.data->spec;
		if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
			uint32_t *t = xcb_res_client_id_value_value(i.data);
			result = *t;
			break;
		}
	}

	free(r);

	if (result == (pid_t)-1)
		result = 0;

	#endif /* __linux__ */

	#ifdef __OpenBSD__
        Atom type;
        int format;
        unsigned long len, bytes;
        unsigned char *prop;
        pid_t ret;

        if (XGetWindowProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", 1), 0, 1, False, AnyPropertyType, &type, &format, &len, &bytes, &prop) != Success || !prop)
               return 0;

        ret = *(pid_t*)prop;
        XFree(prop);
        result = ret;

	#endif /* __OpenBSD__ */
	return result;
}

pid_t
getparentprocess(pid_t p)
{
	unsigned int v = 0;

#ifdef __linux__
	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	fscanf(f, "%*u %*s %*c %u", &v);
	fclose(f);
#endif /* __linux__*/

#ifdef __OpenBSD__
	int n;
	kvm_t *kd;
	struct kinfo_proc *kp;

	kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
	if (!kd)
		return 0;

	kp = kvm_getprocs(kd, KERN_PROC_PID, p, sizeof(*kp), &n);
	v = kp->p_ppid;
#endif /* __OpenBSD__ */

	return (pid_t)v;
}

int
isdescprocess(pid_t p, pid_t c)
{
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int)c;
}

Client *
termforwin(const Client *w)
{
	Client *c;
	Monitor *m;

	if (!w->pid || w->isterminal)
		return NULL;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid))
				return c;
		}
	}

	return NULL;
}

Client *
swallowingclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->swallowing && c->swallowing->win == w)
				return c;
		}
	}

	return NULL;
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

int
wmclasscontains(Window win, const char *class, const char *name)
{
	XClassHint ch = { NULL, NULL };
	int res = 1;

	if (XGetClassHint(dpy, win, &ch)) {
		if (ch.res_name && strstr(ch.res_name, name) == NULL)
			res = 0;
		if (ch.res_class && strstr(ch.res_class, class) == NULL)
			res = 0;
	} else
		res = 0;

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);

	return res;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;
	Client *at = NULL, *cold, *cprevious = NULL;

	if (!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients)) {
		at = findbefore(prevzoom);
		if (at)
			cprevious = nexttiled(at->next);
		if (!cprevious || cprevious != prevzoom) {
			prevzoom = NULL;
			if (!c || !(c = nexttiled(c->next)))
				return;
		} else
			c = cprevious;
	}
	cold = nexttiled(selmon->clients);
	if (c != cold && !at)
		at = findbefore(c);
	detach(c);
	attach(c);
	/* swap windows instead of pushing the previous one down */
	if (c != cold && at) {
		prevzoom = cold;
		if (cold && at != cold) {
			detach(cold);
			cold->next = at->next;
			at->next = cold;
		}
	}
	focus(c);
	arrange(c->mon);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	if (!(xcon = XGetXCBConnection(dpy)))
		die("dwm: cannot get xcb connection\n");
	checkotherwm();
	XrmInitialize();
	load_xresources();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec ps", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	runautostart();
	run();
	if(restart) execvp(argv[0], argv);
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}

void
centeredmaster(Monitor *m)
{
	unsigned int i, n, h, mw, mx, my, oty, ety, tw;
	float mfacts = 0, lfacts = 0, rfacts = 0;
	Client *c;

	/* count number of clients in the selected monitor */
	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
		else if ((n - m->nmaster) % 2)
			lfacts += c->cfact;
		else
			rfacts += c->cfact;
	}

	if (n == 0)
		return;

	if(n == 1){
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
		return;
	}

	/* initialize areas */
	mw = m->ww;
	mx = 0;
	my = 0;
	tw = mw;

	if (n > m->nmaster) {
		/* go mfact box in the center if more than nmaster clients */
		mw = m->nmaster ? m->ww * m->mfact : 0;
		tw = m->ww - mw;

		if (n - m->nmaster > 1) {
			/* only one client */
			mx = (m->ww - mw) / 2;
			tw = (m->ww - mw) / 2;
		}
	}

	oty = 0;
	ety = 0;
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
	if (i < m->nmaster) {
		/* nmaster clients are stacked vertically, in the center
		 * of the screen */
		h = (m->wh - my) * (c->cfact / mfacts);
		resize(c, m->wx + mx, m->wy + my, mw - 2*c->bw,
		      h - 2*c->bw, 0);
		if(my + HEIGHT(c) < m->mh)
			my += HEIGHT(c);
		mfacts -= c->cfact;
	} else {
		/* stack clients are stacked vertically */
		if ((i - m->nmaster) % 2) {
			h = (m->wh - ety) * (c->cfact / lfacts);
			if(m->nmaster == 0)
				resize(c, m->wx, m->wy + ety, tw - 2*c->bw,
			       h - 2*c->bw, 0);
			else
				resize(c, m->wx, m->wy + ety, tw - 2*c->bw,
			       h - 2*c->bw, 0);
			if(ety + HEIGHT(c) < m->mh)
				ety += HEIGHT(c);
			lfacts -= c->cfact;
		} else {
			h = (m->wh - oty) * (c->cfact / rfacts);
			resize(c, m->wx + mx + mw, m->wy + oty,
			       tw - 2*c->bw, h - 2*c->bw, 0);
			if(oty + HEIGHT(c) < m->mh)
				oty += HEIGHT(c);
			rfacts -= c->cfact;
		}
	}
}

void
centeredfloatingmaster(Monitor *m)
{
	unsigned int i, n, w, mh, mw, mx, mxo, my, myo, tx;
	float mfacts = 0, sfacts = 0;
	Client *c;

	/* count number of clients in the selected monitor */
	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;
	}
	if (n == 0)
		return;
	if(n == 1){
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
		return;
	}

	/* initialize nmaster area */
	if (n > m->nmaster) {
		/* go mfact box in the center if more than nmaster clients */
		if (m->ww > m->wh) {
			mw = m->nmaster ? m->ww * m->mfact : 0;
			mh = m->nmaster ? m->wh * 0.9 : 0;
		} else {
			mh = m->nmaster ? m->wh * m->mfact : 0;
			mw = m->nmaster ? m->ww * 0.9 : 0;
		}
		mx = mxo = (m->ww - mw) / 2;
		my = myo = (m->wh - mh) / 2;
	} else {
		/* go fullscreen if all clients are in the master area */
		mh = m->wh;
		mw = m->ww;
		mx = mxo = 0;
		my = myo = 0;
	}

	for(i = tx = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
	if (i < m->nmaster) {
		/* nmaster clients are stacked horizontally, in the center
		 * of the screen */
		w = (mw + mxo - mx) * (c->cfact / mfacts);
		resize(c, m->wx + mx, m->wy + my, w - 2*c->bw,
		       mh - 2*c->bw, 0);
		if(mx + WIDTH(c) < m->mw)
			mx += WIDTH(c);
		mfacts -= c->cfact;
	} else {
		/* stack clients are stacked horizontally */
		w = (m->ww - tx) * (c->cfact / sfacts);
		resize(c, m->wx + tx, m->wy, w - 2*c->bw,
		       m->wh - 2*c->bw, 0);
		if(tx + WIDTH(c) < m->mw)
			tx += WIDTH(c);
		sfacts -= c->cfact;
	}
}

void
bstack(Monitor *m)
{
	unsigned int i, n, w, mh, mx, tx;
	float mfacts = 0, sfacts = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;
	}
	if (n == 0)
		return;
	if(n == 1){
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
		return;
	}

	if (n > m->nmaster)
		mh = m->nmaster ? m->wh * m->mfact : 0;
	else
		mh = m->wh;
	for (i = 0, mx = tx = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			w = (m->ww - mx) * (c->cfact / mfacts);
			resize(c, m->wx + mx, m->wy, w - (2*c->bw), mh - 2*c->bw, 0);
			if(mx + WIDTH(c) < m->mw)
				mx += WIDTH(c);
			mfacts -= c->cfact;
		} else {
			w = (m->ww - tx) * (c->cfact / sfacts);
			resize(c, m->wx + tx, m->wy + mh, w - (2*c->bw), m->wh - mh - 2*(c->bw), 0);
			if(tx + WIDTH(c) < m->mw)
				tx += WIDTH(c);
			sfacts -= c->cfact;
		}
}

void
bstackhoriz(Monitor *m)
{
	unsigned int i, n, h, mw, mh, my, ty;
	float mfacts = 0, sfacts = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;
	}
	if (n == 0)
		return;
	if(n == 1){
		c = nexttiled(m->clients);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
		return;
	}

	if (n > m->nmaster)
		mh = m->nmaster ? m->wh * m->mfact : 0;
	else
		mh = m->wh;
	mw = m->ww;

	for (i = ty = 0, my = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (mh - my) * (c->cfact / mfacts);
			resize(c, m->wx, m->wy + my, mw - 2*c->bw, h - 2*c->bw, 0);
			if(my + HEIGHT(c) < m->mh)
				my += HEIGHT(c);
			mfacts -= c->cfact;
		} else {
			h = (m->wh - mh - ty) * (c->cfact / sfacts);
			resize(c, m->wx, m->wy + mh + ty, mw - 2*c->bw, h - (2*c->bw), 0);
			if(ty + HEIGHT(c) < m->mh)
				ty += HEIGHT(c);
			sfacts -= c->cfact;
		}
}

void deck(Monitor *m) {
	unsigned int i, n, h, mw, my;
	float mfacts = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
	}
	if(n == 0)
		return;

	if(n > m->nmaster) {
		mw = m->nmaster ? m->ww * m->mfact : 0;
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n - m->nmaster);
	}
	else
		mw = m->ww;
	for(i = my = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if(i < m->nmaster) {
			h = (m->wh - my) * (c->cfact / mfacts);
			resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), False);
			if (my + HEIGHT(c) < m->wh)
				my += HEIGHT(c);
			mfacts -= c->cfact;
		}
		else
			resize(c, m->wx + mw, m->wy, m->ww - mw - (2*c->bw), m->wh - (2*c->bw), False);
}

void grid(Monitor *m) {
	unsigned int i, n, cx, cy, cw, ch, aw, ah, cols, rows;
	Client *c;

	for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next))
		n++;

	/* grid dimensions */
	for(rows = 0; rows <= n/2; rows++)
		if(rows*rows >= n)
			break;
	cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;

	/* window geoms (cell height/width) */
	ch = m->wh / (rows ? rows : 1);
	cw = m->ww / (cols ? cols : 1);
	for(i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
		cx = m->wx + (i / rows) * cw;
		cy = m->wy + (i % rows) * ch;
		/* adjust height/width of last row/column's windows */
		ah = ((i + 1) % rows == 0) ? m->wh - ch * rows : 0;
		aw = (i >= rows * (cols - 1)) ? m->ww - cw * cols : 0;
		resize(c, cx, cy, cw - 2 * c->bw + aw, ch - 2 * c->bw + ah, False);
		i++;
	}
}


void tatami(Monitor *m) {
	unsigned int i, n, nx, ny, nw, nh,
				 mats, tc,
				 tnx, tny, tnw, tnh;
	Client *c;

	for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), ++n);
	if(n == 0)
		return;

	nx = m->wx;
	ny = 0;
	nw = m->ww;
	nh = m->wh;

	c = nexttiled(m->clients);

	if(n != 1)  nw = m->ww * m->mfact;
				ny = m->wy;

	resize(c, nx, ny, nw - 2 * c->bw, nh - 2 * c->bw, False);

	c = nexttiled(c->next);

	nx += nw;
	nw = m->ww - nw;

	if(n>1)
	{

	tc = n-1;
	mats = tc/5;

	nh/=(mats + (tc % 5 > 0));

	for(i = 0; c && (i < (tc % 5)); c = nexttiled(c->next))
	{
		tnw=nw;
		tnx=nx;
		tnh=nh;
		tny=ny;
		switch(tc - (mats*5))
				{
					case 1://fill
						break;
					case 2://up and down
						if((i % 5) == 0) //up
						tnh/=2;
						else if((i % 5) == 1) //down
						{
							tnh/=2;
							tny += nh/2;
						}
						break;
					case 3://bottom, up-left and up-right
						if((i % 5) == 0) //up-left
						{
						tnw = nw/2;
						tnh = (2*nh)/3;
						}
						else if((i % 5) == 1)//up-right
						{
							tnx += nw/2;
							tnw = nw/2;
							tnh = (2*nh)/3;
						}
						else if((i % 5) == 2)//bottom
						{
							tnh = nh/3;
							tny += (2*nh)/3;
						}
						break;
					case 4://bottom, left, right and top
						if((i % 5) == 0) //top
						{
							tnh = (nh)/4;
						}
						else if((i % 5) == 1)//left
						{
							tnw = nw/2;
							tny += nh/4;
							tnh = (nh)/2;
						}
						else if((i % 5) == 2)//right
						{
							tnx += nw/2;
							tnw = nw/2;
							tny += nh/4;
							tnh = (nh)/2;
						}
						else if((i % 5) == 3)//bottom
						{
							tny += (3*nh)/4;
							tnh = (nh)/4;
						}
						break;
				}
		++i;
		resize(c, tnx, tny, tnw - 2 * c->bw, tnh - 2 * c->bw, False);
	}

	++mats;

	for(i = 0; c && (mats>0); c = nexttiled(c->next)) {

			if((i%5)==0)
			{
			--mats;
			if(((tc % 5) > 0)||(i>=5))
			ny+=nh;
			}

			tnw=nw;
			tnx=nx;
			tnh=nh;
			tny=ny;


			switch(i % 5)
			{
				case 0: //top-left-vert
					tnw = (nw)/3;
					tnh = (nh*2)/3;
					break;
				case 1: //top-right-hor
					tnx += (nw)/3;
					tnw = (nw*2)/3;
					tnh = (nh)/3;
					break;
				case 2: //center
					tnx += (nw)/3;
					tnw = (nw)/3;
					tny += (nh)/3;
					tnh = (nh)/3;
					break;
				case 3: //bottom-right-vert
					tnx += (nw*2)/3;
					tnw = (nw)/3;
					tny += (nh)/3;
					tnh = (nh*2)/3;
					break;
				case 4: //(oldest) bottom-left-hor
					tnw = (2*nw)/3;
					tny += (2*nh)/3;
					tnh = (nh)/3;
					break;
				default:
					break;
			}

			++i;
			//i%=5;
		resize(c, tnx, tny, tnw - 2 * c->bw, tnh - 2 * c->bw, False);
		}
	}
}

void
insertclient(Client *item, Client *insertItem, int after) {
	Client *c;
	if (item == NULL || insertItem == NULL || item == insertItem) return;
	detach(insertItem);
	if (!after && selmon->clients == item) {
		attach(insertItem);
		return;
	}
	if (after) {
		c = item;
	} else {
		for (c = selmon->clients; c; c = c->next) { if (c->next == item) break; }
	}
	insertItem->next = c->next;
	c->next = insertItem;
}

void
inplacerotate(const Arg *arg)
{
	if(!selmon->sel || (selmon->sel->isfloating && !arg->f)) return;

	unsigned int selidx = 0, i = 0;
	Client *c = NULL, *stail = NULL, *mhead = NULL, *mtail = NULL, *shead = NULL;

	// Determine positionings for insertclient
	for (c = selmon->clients; c; c = c->next) {
		if (ISVISIBLE(c) && !(c->isfloating)) {
		if (selmon->sel == c) { selidx = i; }
		if (i == selmon->nmaster - 1) { mtail = c; }
		if (i == selmon->nmaster) { shead = c; }
		if (mhead == NULL) { mhead = c; }
		stail = c;
		i++;
		}
	}

	// All clients rotate
	if (arg->i == 2) insertclient(selmon->clients, stail, 0);
	if (arg->i == -2) insertclient(stail, selmon->clients, 1);
	// Stack xor master rotate
	if (arg->i == -1 && selidx >= selmon->nmaster) insertclient(stail, shead, 1);
	if (arg->i == 1 && selidx >= selmon->nmaster) insertclient(shead, stail, 0);
	if (arg->i == -1 && selidx < selmon->nmaster)  insertclient(mtail, mhead, 1);
	if (arg->i == 1 && selidx < selmon->nmaster)  insertclient(mhead, mtail, 0);

	// Restore focus position
	i = 0;
	for (c = selmon->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || (c->isfloating)) continue;
		if (i == selidx) { focus(c); break; }
		i++;
	}
	arrange(selmon);
	focus(c);
}

void
togglesticky(const Arg *arg)
{
	if (!selmon->sel)
		return;
	selmon->sel->issticky = !selmon->sel->issticky;
	arrange(selmon);
}

void
togglefullscr(const Arg *arg)
{
  if(selmon->sel)
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

void
setlayoutsafe(const Arg *arg)
{
	const Layout *ltptr = (Layout *)arg->v;
	if (ltptr == 0)
			setlayout(arg);
	for (int i = 0; i < LENGTH(layouts); i++) {
		if (ltptr == &layouts[i])
			setlayout(arg);
	}
}

void
setupepoll(void)
{
	epoll_fd = epoll_create1(0);
	dpy_fd = ConnectionNumber(dpy);
	struct epoll_event dpy_event;

	// Initialize struct to 0
	memset(&dpy_event, 0, sizeof(dpy_event));

	DEBUG("Display socket is fd %d\n", dpy_fd);

	if (epoll_fd == -1) {
		fputs("Failed to create epoll file descriptor", stderr);
	}

	dpy_event.events = EPOLLIN;
	dpy_event.data.fd = dpy_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dpy_fd, &dpy_event)) {
		fputs("Failed to add display file descriptor to epoll", stderr);
		close(epoll_fd);
		exit(1);
	}

	if (ipc_init(ipcsockpath, epoll_fd, ipccommands, LENGTH(ipccommands)) < 0) {
		fputs("Failed to initialize IPC\n", stderr);
	}
}

void
warp(const Client *c)
{
	int x, y;

	if (!c) {
		XWarpPointer(dpy, None, root, 0, 0, 0, 0, selmon->wx + selmon->ww / 2, selmon->wy + selmon->wh / 2);
		return;
	}

	if (!getrootptr(&x, &y) ||
		(x > c->x - c->bw &&
		 y > c->y - c->bw &&
		 x < c->x + c->w + c->bw*2 &&
		 y < c->y + c->h + c->bw*2) ||
		x < c->mon->wx ||
		x > c->mon->wx + c->mon->ww ||
		y < c->mon->wy ||
		y > c->mon->wy + c->mon->wh
	)
		return;

	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w / 2, c->h / 2);
}

void
tagallmon(const Arg *arg)
{
	Monitor *m;
	Client *c, *last, *slast, *next;

	if (!mons->next)
		return;

	m = dirtomon(arg->i);
	for (last = m->clients; last && last->next; last = last->next);
	for (slast = m->stack; slast && slast->snext; slast = slast->snext);

	for (c = selmon->clients; c; c = next) {
		next = c->next;
		if (!ISVISIBLE(c))
			continue;
		unfocus(c, 1);
		detach(c);
		detachstack(c);
		c->mon = m;
		c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
		c->next = NULL;
		c->snext = NULL;
		if (last)
			last = last->next = c;
		else
			m->clients = last = c;
		if (slast)
			slast = slast->snext = c;
		else
			m->stack = slast = c;
		if (c->isfullscreen) {
			resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
			XRaiseWindow(dpy, c->win);
		}
	}

	focus(NULL);
	arrange(NULL);
}

void
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	char *sdst = NULL;
	int *idst = NULL;
	float *fdst = NULL;

	sdst = dst;
	idst = dst;
	fdst = dst;

	char fullname[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s", "dwm", name);
	fullname[sizeof(fullname) - 1] = '\0';

	XrmGetResource(db, fullname, "*", &type, &ret);
	if (!(ret.addr == NULL || strncmp("String", type, 64)))
	{
		switch (rtype) {
		case STRING:
			strcpy(sdst, ret.addr);
			break;
		case INTEGER:
			*idst = strtoul(ret.addr, NULL, 10);
			break;
		case FLOAT:
			*fdst = strtof(ret.addr, NULL);
			break;
		}
	}
}

void
load_xresources(void)
{
	Display *display;
	char *resm;
	XrmDatabase db;
	ResourcePref *p;

	display = XOpenDisplay(NULL);
	resm = XResourceManagerString(display);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LENGTH(resources); p++)
		resource_load(db, p->name, p->type, p->dst);
	XCloseDisplay(display);
}
