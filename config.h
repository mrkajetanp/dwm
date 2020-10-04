/* See LICENSE file for copyright and license details. */
#include <X11/XF86keysym.h>

/* appearance */
static unsigned int borderpx  = 3;        /* border pixel of windows */
static unsigned int snap      = 32;       /* snap pixel */
static int rmaster           = 0;        /* 1 means master-area is initially on the right */
static int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static unsigned int gappx     = 10; /* gap between windows */
static unsigned int cornerrad = 6; /* corner radius */

static int showbar            = 1;        /* 0 means no bar */
static int topbar             = 1;        /* 0 means bottom bar */
static const int usealtbar          = 1;        /* 1 means use non-dwm status bar */
static const char *altbarclass      = "Polybar"; /* Alternate bar class name */
static const char *altbarcmd        = "$HOME/.config/polybar/launch.sh"; /* Alternate bar launch command */

static const int vertpad            = 5;        /* vertical bar padding */
static const int sidepad            = 5;        /* horizontal bar padding */

static const int viewontag         = 1;     /* Switch view on tag switch */

static const char *fonts[]          = { "Open Sans:size=10" };
static const char dmenufont[]       = "Open Sans:size=10";

static char normbgcolor[]       = "#222222";
static char normbordercolor[]       = "#4C566A";
static char normfgcolor[]       = "#bbbbbb";
static char selfgcolor[]       = "#eeeeee";
static char selbgcolor[]        = "#005577";
static char selbordercolor[]  = "#ABB1BB";
static char urgbordercolor[]   = "#ff0000";

static char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { normfgcolor, normbgcolor, normbordercolor },
	[SchemeSel]  = { selfgcolor, selbgcolor,  selbordercolor  },
	[SchemeUrg]  = { selfgcolor, selbgcolor,  urgbordercolor  },
};

static const Inset default_inset = {
	.x = 0,
	.y = 9,
	.w = 0,
	.h = 48,
};

/* tagging */
static const char *tags[] = { "", "", "", "", "ﱅ", "", "", "", "漣", "" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
+	 *  WM_WINDOW_ROLE(STRING) = role
	 */
	/* class   role     instance  title           tags mask  isfloating  isterminal  noswallow  monitor   float x,y,w,h   floatborderpx */
	{ "Opera", NULL,    NULL,     NULL,           1 << 1,         0,          0,           0,        -1 },
	{ "Firefox", NULL, NULL,     NULL,           1 << 1,    0,          0,          -1,        -1 },
	{ "Emacs", NULL, NULL,     NULL,           1 << 3,    0,          0,          -1,        -1 },
	{ "st-256color", NULL,      NULL,     NULL,           0,         0,          1,           0,        -1, 100, 100, 50, 50, 4},
	{ NULL, NULL,      NULL,     "Event Tester", 0,         0,          0,           1,        -1 }, /* xev */
};

/* layout(s) */
static float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static int nmaster     = 1;    /* number of clients in master area */
static int resizehints = 0;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "(Tiled)",      tile },    /* first entry is default */
	{ "(Floating)",      NULL },    /* no layout function means floating behavior */
	{ "(Monocle)",      monocle },
	{ "(BStack)",      bstack },
	{ "(BStack (H))",      bstackhoriz },
	{ "(Centered)",      centeredmaster },
	{ "(Centered (F))",      centeredfloatingmaster },
	{ "(Deck)",      deck },
	{ "(Grid)",      grid },
	{ "(Tatami)",      tatami },
	{ NULL,       NULL },
};

static const MonitorRule monrules[] = {
	/* monitor  tag  layout  mfact  nmaster  showbar  topbar */
	// Main monitor
	{  0,      1,  0,      -1,    -1,      -1,      -1     },
	{  0,      2,  2,      -1,    -1,      -1,      -1     },
	{  0,      4,  2,      -1,    -1,      -1,      -1     },
	// Secondary monitor
	{  1,       1,  8,      -1,    -1,      -1,      -1     },
	{  1,       2,  3,      -1,    -1,      -1,      -1     },
	// Default
	{  -1,      -1,  0,      -1,    -1,      -1,      -1     },
};

/*
 * Xresources preferences to load at startup
 */
ResourcePref resources[] = {
		{ "normbgcolor",        STRING,  &normbgcolor },
		{ "normbordercolor",    STRING,  &normbordercolor },
		{ "normfgcolor",        STRING,  &normfgcolor },
		{ "selbgcolor",         STRING,  &selbgcolor },
		{ "selbordercolor",     STRING,  &selbordercolor },
		{ "selfgcolor",         STRING,  &selfgcolor },
		{ "borderpx",          	INTEGER, &borderpx },
		{ "snap",          		INTEGER, &snap },
		{ "showbar",          	INTEGER, &showbar },
		{ "topbar",          	INTEGER, &topbar },
		{ "nmaster",          	INTEGER, &nmaster },
		{ "resizehints",       	INTEGER, &resizehints },
		{ "mfact",      	 	FLOAT,   &mfact },
};

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbordercolor, "-sf", selfgcolor, NULL };
static const char *rofiruncmd[] = { "rofi", "-show", "run", "-font", "Noto Sans 12", "-theme", "materia", NULL };
static const char *rofidruncmd[] = { "rofi", "-show", "drun", "-font", "Noto Sans 12", "-theme", "clean", NULL };
static const char *termcmd[]  = { "st", NULL };
static const char scratchpadname[] = "scratchpad";
static const char *scratchpadcmd[] = { "st", "-t", scratchpadname, "-g", "120x34", NULL };
static const char *flameshotcmd[]  = { "flameshot", "gui" };

/* Brightness, volume & media commands */

static const char *volumeupcmd[]  = { "pulsemixer", "--change-volume", "+10" };
static const char *volumedowncmd[]  = { "pulsemixer", "--change-volume", "-10" };
static const char *volumemutecmd[]  = { "pulsemixer", "--toggle-mute" };

static const char *monbrightnessupcmd[]  = { "light", "-A", "10" };
static const char *monbrightnessdowncmd[]  = { "light", "-U", "10" };

static const char *kbdbrightnessupcmd[]  = { "light", "-s", "sysfs/leds/smc::kbd_backlight", "-A", "10" };
static const char *kbdbrightnessdowncmd[]  = { "light", "-s", "sysfs/leds/smc::kbd_backlight", "-U", "10" };

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_d,      spawn,          {.v = rofidruncmd } },
	{ MODKEY|ShiftMask,             XK_d,      spawn,          {.v = rofiruncmd } },
	{ MODKEY,                       XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY|ShiftMask,             XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,      pushdown,     { 0 } },
	{ MODKEY|ShiftMask,             XK_k,      pushup,     { 0 } },
	{ MODKEY,                       XK_h,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_l,      focusstack,     {.i = +1 } },
	{ MODKEY|ControlMask,           XK_j,      inplacerotate,  {.i = +1} },
	{ MODKEY|ControlMask,           XK_k,      inplacerotate,  {.i = -1} },
	{ MODKEY|ControlMask,           XK_h,      inplacerotate,  {.i = +2} },
	{ MODKEY|ControlMask,           XK_l,      inplacerotate,  {.i = -2} },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_v,      incnmaster,     {.i = -1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_h,      setcfact,       {.f = -0.25} },
	{ MODKEY|ControlMask|ShiftMask, XK_l,      setcfact,       {.f = +0.25} },
	{ MODKEY|ControlMask|ShiftMask, XK_o,      setcfact,       {.f = 0.00} },
	{ MODKEY|ShiftMask,             XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY|ShiftMask,             XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_f,      togglefullscr,  {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY|ShiftMask,             XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_u,      setlayout,      {.v = &layouts[3]} },
	{ MODKEY,                       XK_o,      setlayout,      {.v = &layouts[4]} },
	{ MODKEY,                       XK_c,      setlayout,      {.v = &layouts[5]} },
	{ MODKEY|ControlMask,           XK_c,      setlayout,      {.v = &layouts[6]} },
	{ MODKEY,                       XK_r,      setlayout,      {.v = &layouts[7]} },
	{ MODKEY,                       XK_g,      setlayout,      {.v = &layouts[8]} },
	{ MODKEY|ShiftMask,             XK_t,      setlayout,      {.v = &layouts[9]} },
	{ MODKEY|ControlMask,		    XK_comma,  cyclelayout,    {.i = -1 } },
	{ MODKEY|ControlMask,           XK_period, cyclelayout,    {.i = +1 } },
	{ MODKEY,                       XK_space,      switchcol,     { 0 } },
	{ MODKEY,                       XK_n,  togglermaster,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_numbersign,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_exclam,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_comma,  tagallmon,      {.i = +1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_period, tagallmon,      {.i = -1 } },
	{ MODKEY,                       XK_Left,   viewtoleft,     {0} },
	{ MODKEY,                       XK_Right,  viewtoright,    {0} },
	{ MODKEY|ShiftMask,             XK_Left,   tagtoleft,      {0} },
	{ MODKEY|ShiftMask,             XK_Right,  tagtoright,     {0} },

	{ MODKEY,                       XK_s,      togglesticky,   {0} },

	{ MODKEY,                       XK_dollar,  togglescratch,  {.v = scratchpadcmd } },
	TAGKEYS(                        XK_ampersand,                      0)
	TAGKEYS(                        XK_bracketleft,                      1)
	TAGKEYS(                        XK_braceleft,                      2)
	TAGKEYS(                        XK_braceright,                      3)
	TAGKEYS(                        XK_parenleft,                      4)
	TAGKEYS(                        XK_equal,                      5)
	TAGKEYS(                        XK_asterisk,                      6)
	TAGKEYS(                        XK_parenright,                      7)
	TAGKEYS(                        XK_plus,                      8)
	TAGKEYS(                        XK_bracketright,                      9)
	{ MODKEY,                       XK_q,      killclient,     {0} },
	{ MODKEY|ShiftMask, XK_x,      quit,           {0} },
	{ MODKEY|ShiftMask,             XK_r,      quit,      {1} },
	{ ShiftMask|ControlMask,        XK_braceleft,      spawn,      { .v = flameshotcmd } },

	/**** Brightness, volume & media keys ****/
	// Audio
	{ 0,        XF86XK_AudioRaiseVolume,      spawn,      { .v = volumeupcmd } },
	{ 0,        XF86XK_AudioLowerVolume,      spawn,      { .v = volumedowncmd } },
	{ 0,        XF86XK_AudioMute,            spawn,      { .v = volumemutecmd } },
	// Brightness
	{ 0,        XF86XK_MonBrightnessUp,      spawn,      { .v = monbrightnessupcmd } },
	{ 0,        XF86XK_MonBrightnessDown,      spawn,      { .v = monbrightnessdowncmd } },
	{ 0,        XF86XK_KbdBrightnessUp,      spawn,      { .v = kbdbrightnessupcmd } },
	{ 0,        XF86XK_KbdBrightnessDown,      spawn,      { .v = kbdbrightnessdowncmd } },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

static const char *ipcsockpath = "/tmp/dwm.sock";
static IPCCommand ipccommands[] = {
  IPCCOMMAND(  view,                1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  toggleview,          1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  tag,                 1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  toggletag,           1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  tagmon,              1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  focusmon,            1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  focusstack,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  zoom,                1,      {ARG_TYPE_NONE}   ),
  IPCCOMMAND(  incnmaster,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  killclient,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  togglefloating,      1,      {ARG_TYPE_NONE}   ),
  IPCCOMMAND(  setmfact,            1,      {ARG_TYPE_FLOAT}  ),
  IPCCOMMAND(  setlayoutsafe,       1,      {ARG_TYPE_PTR}    ),
  IPCCOMMAND(  quit,                1,      {ARG_TYPE_NONE}   )
};

void
setlayoutex(const Arg *arg)
{
	setlayout(&((Arg) { .v = &layouts[arg->i] }));
}

void
viewex(const Arg *arg)
{
	view(&((Arg) { .ui = 1 << arg->ui }));
}

void
viewall(const Arg *arg)
{
	view(&((Arg){.ui = ~0}));
}

void
toggleviewex(const Arg *arg)
{
	toggleview(&((Arg) { .ui = 1 << arg->ui }));
}

void
tagex(const Arg *arg)
{
	tag(&((Arg) { .ui = 1 << arg->ui }));
}

void
toggletagex(const Arg *arg)
{
	toggletag(&((Arg) { .ui = 1 << arg->ui }));
}

void
tagall(const Arg *arg)
{
	tag(&((Arg){.ui = ~0}));
}

/* signal definitions */
/* signum must be greater than 0 */
/* trigger signals using `xsetroot -name "fsignal:<signame> [<type> <value>]"` */
static Signal signals[] = {
	/* signum           function */
	{ "focusstack",     focusstack },
	{ "setmfact",       setmfact },
	{ "togglebar",      togglebar },
	{ "incnmaster",     incnmaster },
	{ "togglefloating", togglefloating },
	{ "focusmon",       focusmon },
	{ "tagmon",         tagmon },
	{ "zoom",           zoom },
	{ "view",           view },
	{ "viewall",        viewall },
	{ "viewex",         viewex },
	{ "toggleview",     view },
	{ "toggleviewex",   toggleviewex },
	{ "tag",            tag },
	{ "tagall",         tagall },
	{ "tagex",          tagex },
	{ "toggletag",      tag },
	{ "toggletagex",    toggletagex },
	{ "killclient",     killclient },
	{ "quit",           quit },
	{ "setlayout",      setlayout },
	{ "setlayoutex",    setlayoutex },
};
