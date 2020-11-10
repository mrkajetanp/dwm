/* See LICENSE file for copyright and license details. */
#include <X11/XF86keysym.h>

/* appearance */
static unsigned int borderpx     = 3;        /* border pixel of windows */
static unsigned int snap         = 32;       /* snap pixel */
static int rmaster               = 0;        /* 1 means master-area is initially on the right */
static int swallowfloating       = 0;        /* 1 means swallow floating windows by default */
static unsigned int cornerrad    = 0;        /* corner radius */
static Gap default_gap     = {.isgap = 1, .realgap = 10, .gappx = 10};

static int showbar               = 1;        /* 0 means no bar */
static int topbar                = 1;        /* 0 means bottom bar */

static int enablekeys = 1;

static const int usealtbar       = 1;        /* 1 means use non-dwm status bar */
static const char *altbarclass   = "Polybar";/* Alternate bar class name */
static const char *altbarcmd     = "$HOME/.config/polybar/launch.sh"; /* Alternate bar launch command */

static const int vertpad         = 0;        /* vertical bar padding */
static const int sidepad         = 0;        /* horizontal bar padding */

static const int viewontag       = 1;        /* Switch view on tag switch */

static const char *fonts[]       = { "Open Sans:size=10" };
static const char dmenufont[]    = "Open Sans:size=10";

static char normbgcolor[]        = "#222222";
static char normbordercolor[]    = "#665c54";
static char normfgcolor[]        = "#bbbbbb";
static char normmarkcolor[]      = "#B48EAD";
static char selfgcolor[]         = "#eeeeee";
static char selbgcolor[]         = "#005577";
static char selbordercolor[]     = "#d5c4a1";
static char selmarkcolor[]       = "#5E81AC";
static char urgbordercolor[]     = "#ff0000";

static char *colors[][4]      = {
	/*               fg             bg             border   */
	[SchemeNorm] = { normfgcolor,   normbgcolor,   normbordercolor, normmarkcolor },
	[SchemeSel]  = { selfgcolor,    selbgcolor,    selbordercolor,  selmarkcolor },
	[SchemeUrg]  = { selfgcolor,    selbgcolor,    urgbordercolor,  selmarkcolor  },
};

static const Inset default_inset = {
	.x = 0,
	.y = 0,
	.w = 0,
	.h = 38,
};

/* tagging */
static const char *tags[] = { "", "", "", "", "ﱅ", "", "", "", "漣", "" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
+	 *  WM_WINDOW_ROLE(STRING) = role
	 */
	/* class              role     instance      title                           tags mask     isfloating   isterminal  noswallow  monitor   float x,y,w,h       fborder */
	{ "Alacritty",        NULL,    NULL,         NULL,                           0,            0,           1,           0,        -1,        100, 100, 50, 50,    0},
	{ "st-256color",      NULL,    NULL,         NULL,                           0,            0,           1,           0,        -1,        100, 100, 50, 50,    0},
	{ "Opera",            NULL,    NULL,         NULL,                           1 << 1,       0,           0,           0,        -1,        -1,  -1,  -1, -1,    -1 },
	{ "Chromium",         NULL,    NULL,         NULL,                           1 << 1,       0,           0,           0,        -1,        -1,  -1,  -1, -1,    -1 },
	{ "Firefox",          NULL,    NULL,         NULL,                           1 << 1,       0,           0,          -1,        -1 },
	{ "code-oss",         NULL,    NULL,         NULL,                           1 << 3,       0,           0,          -1,        -1 },
	{ "Emacs",            NULL,    NULL,         NULL,                           1 << 3,       0,           0,          -1,        -1 },
	{ "Spotify",          NULL,    NULL,         NULL,                           1 << 6,       0,           0,           0,        -1,        -1,  -1,  -1, -1,    -1 },
	{ "Signal Beta",      NULL,    NULL,         NULL,                           1 << 8,       0,           0,          -1,        -1 },
	{ "tutanota-desktop", NULL,    NULL,         NULL,                           1 << 8,       0,           0,           0,        -1,        -1,  -1,  -1, -1,    -1 },
	{ "Steam",            NULL,    NULL,         NULL,                           1 << 9,       0,           0,          -1,        -1 },
	{ "Bitwarden",        NULL,    NULL,         NULL,                           1 << 9,       0,           0,           0,        -1,        -1,  -1,  -1, -1,    -1 },
	{ NULL,               NULL,    NULL,         "Android Emulator - Pixel:5554",0,            1,           0,           1,        -1 }, /* xev */
	{ NULL,               NULL,    NULL,         "Event Tester",                 0,            0,           0,           1,        -1 }, /* xev */
	{ "Dwarf_Fortress",   NULL,    NULL,         "Dwarf Fortress",               0,            0,           0,           1,        -1 },
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
	{ "(TStack)",      tstack },
	{ "(TStack (H))",      tstackhoriz },
	{ "(Spiral)",      spiral },
	{ "(Dwindle)",      dwindle },
	{ NULL,       NULL },
};

static const MonitorRule monrules[] = {
	/* monitor  tag  layout  mfact  nmaster  showbar  topbar */
	// Main monitor
	{  0,       1,   12,     -1,    -1,      -1,      -1     },
	{  0,       2,   2,      -1,    -1,      -1,      -1     },
	{  0,       4,   2,      -1,    -1,      -1,      -1     },
	// Secondary monitor
	{  1,       1,   8,      -1,    -1,      -1,      -1     },
	{  1,       2,   3,      -1,    -1,      -1,      -1     },
	// Default
	{  -1,      -1,  0,      -1,    -1,      -1,      -1     },
};

/* Xresources preferences to load at startup */
ResourcePref resources[] = {
		{ "normbgcolor",        STRING,   &normbgcolor },
		{ "normbordercolor",    STRING,   &normbordercolor },
		{ "normfgcolor",        STRING,   &normfgcolor },
		{ "selbgcolor",         STRING,   &selbgcolor },
		{ "selbordercolor",     STRING,   &selbordercolor },
		{ "selfgcolor",         STRING,   &selfgcolor },
		{ "selmarkcolor",       STRING,   &selmarkcolor },
		{ "normmarkcolor",      STRING,   &normmarkcolor },
		{ "borderpx",          	INTEGER,  &borderpx },
		{ "snap",          		  INTEGER,  &snap },
		{ "showbar",          	INTEGER,  &showbar },
		{ "topbar",          	  INTEGER,  &topbar },
		{ "nmaster",          	INTEGER,  &nmaster },
		{ "resizehints",       	INTEGER,  &resizehints },
		{ "mfact",      	 	    FLOAT,    &mfact },
		{ "gappx",          	  INTEGER,  &(default_gap.gappx) },
		{ "cornerrad",       	  INTEGER,  &cornerrad },
		{ "rmaster",       	    INTEGER,  &rmaster },
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
static const char *rofiruncmd[] = { "rofi", "-show", "run", "-font", "Noto Sans 12", "-theme", "slate-gruvbox", NULL };
static const char *rofidruncmd[] = { "rofi", "-show", "drun", "-font", "Noto Sans 12", "-theme", "clean", NULL };
static const char *roficalccmd[] = { "rofi", "-show", "calc", "-modi", "calc", "-no-show-match", "-no-sort", "-font", "Noto Sans 12", "-theme", "slate-gruvbox", NULL };
static const char *termcmd[]  = { "alacritty", NULL };

static const char scratchpadname[] = "scratchpad";
static const char *scratchpadcmd[] = { "st", "-t", scratchpadname, "-g", "120x34", NULL };
static const char *flameshotcmd[]  = { "flameshot", "gui", NULL };
static const char *lockscreencmd[]  = { "betterlockscreen", "--lock", NULL };

static const char *filestermcmd[]  = { "alacritty", "-t", "files", "-e", "fish", "-C", "abduco -A files fish -C ls", NULL };
static const char *nftermcmd[]  = { "alacritty", "-t", "secondary", "-e", "fish", "-C", "abduco -A second fish -C neofetch", NULL };
static const char *maintermcmd[]  = { "alacritty", "-t", "main", "-e", "fish", "-C", "abduco -A main fish", NULL };

static const char* roficlipboardcmd[] = {"/home/kajetan/.bin/rofi-clipboard", NULL};
static const char* rofipasscmd[] = {"rofi-pass", NULL};
static const char* rofirbwcmd[] = {"/home/kajetan/.bin/rofi-rbw", NULL};
static const char* rofiemojicmd[] = {"/home/kajetan/.bin/rofi-emoji", NULL};
static const char* rofinmclicmd[] = {"/home/kajetan/.bin/rofi-nmcli", NULL};
static const char* rofitrcmd[] = {"/home/kajetan/.bin/rofi-tr", NULL};

/* Brightness, volume & media commands */
static const char *volumeupcmd[]  = { "pulsemixer", "--change-volume", "+10", NULL };
static const char *volumedowncmd[]  = { "pulsemixer", "--change-volume", "-10", NULL };
static const char *volumeupcmdslow[]  = { "pulsemixer", "--change-volume", "+5", NULL };
static const char *volumedowncmdslow[]  = { "pulsemixer", "--change-volume", "-5", NULL };
static const char *volumemutecmd[]  = { "pulsemixer", "--toggle-mute", NULL };

static const char *mediaplaycmd[]  = { "playerctl", "play-pause", NULL };
static const char *mediaprevcmd[]  = { "playerctl", "previous", NULL };
static const char *medianextcmd[]  = { "playerctl", "next", NULL };

static const char *monbrightnessupcmd[]  = { "light", "-A", "10", NULL };
static const char *monbrightnessdowncmd[]  = { "light", "-U", "10", NULL };
static const char *monbrightnessupcmdslow[]  = { "light", "-A", "5", NULL};
static const char *monbrightnessdowncmdslow[]  = { "light", "-U", "5", NULL };

static const char *kbdbrightnessupcmd[]  = { "light", "-s", "sysfs/leds/smc::kbd_backlight", "-A", "10", NULL };
static const char *kbdbrightnessdowncmd[]  = { "light", "-s", "sysfs/leds/smc::kbd_backlight", "-U", "10", NULL };
static const char *kbdbrightnessupcmdslow[]  = { "light", "-s", "sysfs/leds/smc::kbd_backlight", "-A", "5", NULL };
static const char *kbdbrightnessdowncmdslow[]  = { "light", "-s", "sysfs/leds/smc::kbd_backlight", "-U", "5", NULL };

static Key keys[] = {
	/* modifier                     key              function        argument */
	{ MODKEY,                       0xff9a,          togglekeys,     {0} },

	/**** Navigation ****/
	{ MODKEY,                       XK_j,            focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,            focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_h,            focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_l,            focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_Tab,          view,           {0} },
	{ MODKEY,                       XK_space,        switchcol,      { 0 } },

	/**** Layout states & modifications ****/
	{ MODKEY|ShiftMask,             XK_Return,       zoom,           {0} },
	{ MODKEY|ShiftMask,             XK_j,            pushdown,       { 0 } },
	{ MODKEY|ShiftMask,             XK_k,            pushup,         { 0 } },
	{ MODKEY,                       XK_s,            togglesticky,   {0} },
	{ MODKEY,                       XK_dollar,       togglescratch,  {.v = scratchpadcmd } },
	{ MODKEY|ControlMask,           XK_j,            inplacerotate,  {.i = +1} },
	{ MODKEY|ControlMask,           XK_k,            inplacerotate,  {.i = -1} },
	{ MODKEY|ControlMask,           XK_h,            inplacerotate,  {.i = +2} },
	{ MODKEY|ControlMask,           XK_l,            inplacerotate,  {.i = -2} },
	{ MODKEY,                       XK_i,            incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_v,            incnmaster,     {.i = -1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_h,            setcfact,       {.f = -0.25} },
	{ MODKEY|ControlMask|ShiftMask, XK_l,            setcfact,       {.f = +0.25} },
	{ MODKEY|ControlMask|ShiftMask, XK_o,            setcfact,       {.f = 0.00} },
	{ MODKEY|ShiftMask,             XK_h,            setmfact,       {.f = -0.05} },
	{ MODKEY|ShiftMask,             XK_l,            setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_n,            togglermaster,  {0} },
	{ MODKEY|ShiftMask,             XK_space,        togglefloating, {0} },
	{ MODKEY,                       XK_f,            togglefullscr,  {0} },

	{ MODKEY|ShiftMask,             XK_slash,        setgaps,        {.i = -5 } },
	{ MODKEY|ShiftMask,             XK_at,           setgaps,        {.i = +5 } },
	{ MODKEY|ControlMask|ShiftMask, XK_slash,        setgaps,        {.i = GAP_RESET } },
	{ MODKEY|ControlMask|ShiftMask, XK_at,           setgaps,        {.i = GAP_TOGGLE} },

  { MODKEY,                       XK_minus,        togglemark,     {0} },
  { MODKEY,                       XK_slash,        swapfocus,      {0} },
  { MODKEY,                       XK_at,           swapclient,     {0} },

	/**** Multiple monitors ****/
	{ MODKEY,                       XK_comma,        focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period,       focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,        tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period,       tagmon,         {.i = +1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_comma,        tagallmon,      {.i = +1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_period,       tagallmon,      {.i = -1 } },

	/**** Switching layouts ****/
	{ MODKEY,                       XK_t,            setlayout,      {.v = &layouts[0]} }, /* Tiled */
	{ MODKEY|ShiftMask,             XK_f,            setlayout,      {.v = &layouts[1]} }, /* Floating */
	{ MODKEY,                       XK_m,            setlayout,      {.v = &layouts[2]} }, /* Monocle */
	{ MODKEY,                       XK_u,            setlayout,      {.v = &layouts[3]} }, /* Bstack */
	{ MODKEY|ShiftMask,             XK_u,            setlayout,      {.v = &layouts[4]} }, /* Bstack Horiz */
	{ MODKEY,                       XK_c,            setlayout,      {.v = &layouts[5]} }, /* Centered */
	{ MODKEY|ControlMask,           XK_c,            setlayout,      {.v = &layouts[6]} }, /* Centered floating */
	{ MODKEY|ShiftMask,             XK_m,            setlayout,      {.v = &layouts[7]} }, /* Deck */
	{ MODKEY,                       XK_g,            setlayout,      {.v = &layouts[8]} }, /* Grid */
	{ MODKEY|ShiftMask,             XK_t,            setlayout,      {.v = &layouts[9]} }, /* Tatami */
	{ MODKEY,                       XK_o,            setlayout,      {.v = &layouts[10]} }, /* Tstack */
	{ MODKEY|ShiftMask,             XK_o,            setlayout,      {.v = &layouts[11]} }, /* Tstack horiz */
	{ MODKEY,                       XK_r,            setlayout,      {.v = &layouts[12]} }, /* Spiral */

	{ MODKEY|ControlMask,		        XK_comma,        cyclelayout,    {.i = -1 } },
	{ MODKEY|ControlMask,           XK_period,       cyclelayout,    {.i = +1 } },

	/**** Switching tags ****/
	TAGKEYS(                        XK_ampersand,                    0)
	TAGKEYS(                        XK_bracketleft,                  1)
	TAGKEYS(                        XK_braceleft,                    2)
	TAGKEYS(                        XK_braceright,                   3)
	TAGKEYS(                        XK_parenleft,                    4)
	TAGKEYS(                        XK_equal,                        5)
	TAGKEYS(                        XK_asterisk,                     6)
	TAGKEYS(                        XK_parenright,                   7)
	TAGKEYS(                        XK_plus,                         8)
	TAGKEYS(                        XK_bracketright,                 9)
	{ MODKEY,                       XK_Left,         viewtoleft,     {0} },
	{ MODKEY,                       XK_Right,        viewtoright,    {0} },
	{ MODKEY|ShiftMask,             XK_Left,         tagtoleft,      {0} },
	{ MODKEY|ShiftMask,             XK_Right,        tagtoright,     {0} },
	{ MODKEY,                       XK_numbersign,   view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_exclam,       tag,            {.ui = ~0 } },

	/**** Killing and quitting ****/
	{ MODKEY,                       XK_q,            killclient,     {0} },
	{ MODKEY|ShiftMask,             XK_c,            killclient,     {0} },
	{ MODKEY|ShiftMask,             XK_x,            quit,           {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_x,            spawn,          {.v = lockscreencmd } },
	{ MODKEY|ShiftMask,             XK_r,            quit,           {1} },

	/**** Custom launch commands ****/
	{ MODKEY,                       XK_d,            spawn,          {.v = rofidruncmd } },
	{ MODKEY|ShiftMask,             XK_d,            spawn,          {.v = rofiruncmd } },
	{ MODKEY|ControlMask,           XK_d,            spawn,          {.v = roficalccmd } },
	{ MODKEY,                       XK_Return,       spawn,          {.v = termcmd } },
	{ ShiftMask|ControlMask,        XK_braceleft,    spawn,          {.v = flameshotcmd } },
	{ 0,                            0xff61,          spawn,          {.v = flameshotcmd } },
	{ MODKEY,                       0xff96,          spawn,          {.v = maintermcmd } },
	{ MODKEY,                       0xff9d,          spawn,          {.v = nftermcmd } },
	{ MODKEY,                       0xff98,          spawn,          {.v = filestermcmd } },

	/**** Misc ****/
	{ MODKEY|ControlMask|ShiftMask, XK_c,            spawn,          {.v = roficlipboardcmd } },
	{ MODKEY,                       XK_w,            spawn,          {.v = rofirbwcmd } },
	{ MODKEY|ShiftMask,             XK_w,            spawn,          {.v = rofipasscmd } },
	{ MODKEY|ShiftMask,             XK_e,            spawn,          {.v = rofiemojicmd } },
	{ MODKEY|ShiftMask,             XK_n,            spawn,          {.v = rofinmclicmd } },
	{ MODKEY|ControlMask|ShiftMask, XK_t,            spawn,          {.v = rofitrcmd } },

	/**** Brightness, volume & media keys ****/
	// Audio
	{ 0,                            XF86XK_AudioRaiseVolume,      spawn,      { .v = volumeupcmd } },
	{ 0,                            XF86XK_AudioLowerVolume,      spawn,      { .v = volumedowncmd } },
	{ ShiftMask,                    XF86XK_AudioRaiseVolume,      spawn,      { .v = volumeupcmdslow } },
	{ ShiftMask,                    XF86XK_AudioLowerVolume,      spawn,      { .v = volumedowncmdslow } },
	{ 0,                            XF86XK_AudioMute,             spawn,      { .v = volumemutecmd } },
	// Media controls
	{ 0,                            XF86XK_AudioPlay,             spawn,      { .v = mediaplaycmd } },
	{ 0,                            XF86XK_AudioNext,             spawn,      { .v = medianextcmd } },
	{ 0,                            XF86XK_AudioPrev,             spawn,      { .v = mediaprevcmd } },
	// Monitor brightness
	{ 0,                            XF86XK_MonBrightnessUp,       spawn,      { .v = monbrightnessupcmd } },
	{ 0,                            XF86XK_MonBrightnessDown,     spawn,      { .v = monbrightnessdowncmd } },
	{ ShiftMask,                    XF86XK_MonBrightnessUp,       spawn,      { .v = monbrightnessupcmdslow } },
	{ ShiftMask,                    XF86XK_MonBrightnessDown,     spawn,      { .v = monbrightnessdowncmdslow } },
	// Keyboard brightness
	{ 0,                            XF86XK_KbdBrightnessUp,       spawn,      { .v = kbdbrightnessupcmd } },
	{ 0,                            XF86XK_KbdBrightnessDown,     spawn,      { .v = kbdbrightnessdowncmd } },
	{ ShiftMask,                    XF86XK_KbdBrightnessUp,       spawn,      { .v = kbdbrightnessupcmdslow } },
	{ ShiftMask,                    XF86XK_KbdBrightnessDown,     spawn,      { .v = kbdbrightnessdowncmdslow } },
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
