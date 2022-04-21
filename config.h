/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 4;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 0;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "DejaVu Sans Mono:size=14" };
static const char dmenufont[]       = "DejaVu Sans Mono:size=14";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#676767";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_orange[]      = "#ce5c00";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_orange,  col_orange  },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ NULL,		NULL,	"Terminal",	2,		1,	-1 },
	{ NULL,		NULL,	"Emacs",	4,		1,	-1 },
	{ "Firefox",	NULL,	NULL,		8,		1,	-1 },
	{ "Tor Browser",NULL,	NULL,		8,		1,	-1 },
	{ "Gimp",	NULL,	NULL,		1,		1,	-1 },
	{ "Shotcut",	NULL,	NULL,		1,		1,	-1 },
	{ "Kicad",	NULL,	NULL,		1,		1,	-1 },
	{ "Audacity",	NULL,	NULL,		1,		1,	-1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[]=",      tile },    /* first entry is default */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY (ControlMask|ShiftMask)
#define TAGKEYS(KEY,TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont,
	"-nb", col_gray1, "-nf", col_gray3, "-sb", col_orange, "-sf", col_gray4,
	NULL };
static const char *termcmd[]  = { "env", "LC_CTYPE=en_US.UTF-8", "/home/me/bin/st",
	"-F", "-n", "st", "-T", "Terminal", "-e", "tmux",
	NULL };

static Key keys[] = {
  /* modifier(s)	key		function	argument */
  { MODKEY, 		XK_b,		togglebar,      {0} },
  { MODKEY,	 	XK_n,		spawn,          {.v = termcmd } },
  { Mod1Mask,		XK_Tab,		view,		{0} },
  { MODKEY,		XK_comma,	focusmon,	{.i = -1 } },
  { MODKEY,		XK_period,	focusmon,	{.i = +1 } },
  { MODKEY, 		XK_i,		incnmaster,     {.i = +1 } },
  { MODKEY, 		XK_d,		incnmaster,     {.i = -1 } },
  { MODKEY, 		XK_semicolon,	focusstack,	{.i = +1 } },
  { MODKEY,	 	XK_c,		killclient,     {0} },
  { MODKEY, 		XK_t,		setlayout,      {.v = &layouts[1]} },
  { MODKEY, 		XK_f,		setlayout,      {.v = &layouts[0]} },
  { MODKEY, 		XK_m,		setlayout,      {.v = &layouts[2]} },
  { MODKEY, 		XK_h,		view,           {.ui = 1 << 0} },
  { MODKEY, 		XK_j,		view,           {.ui = 1 << 1} },
  { MODKEY, 		XK_k,		view,           {.ui = 1 << 2} },
  { MODKEY, 		XK_l,		view,           {.ui = 1 << 3} },
  { MODKEY|Mod1Mask,	XK_h,		tag,		{.ui = 1 << 0} },
  { MODKEY|Mod1Mask,	XK_j,		tag,		{.ui = 1 << 1} },
  { MODKEY|Mod1Mask,	XK_k,		tag,		{.ui = 1 << 2} },
  { MODKEY|Mod1Mask,	XK_l,		tag,		{.ui = 1 << 3} },
  { MODKEY, 		XK_q,		quit,           {0} },
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

