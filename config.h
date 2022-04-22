/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 4;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const char col_gray2[]       = "#676767";
static const char col_orange[]      = "#ce5c00";
static const char *colors[][1]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray2 },
	[SchemeSel]  = { col_orange },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", };
static unsigned int inittag = 1, initalttag = 2;

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tag-mask    */
	{ "st",		NULL,	NULL,		2,	},
	{ "Emacs",	NULL,	NULL,		2,	},
	{ "Firefox",	NULL,	NULL,		4,	},
	{ "Tor Browser",NULL,	NULL,		4,	},
	{ "Gimp",	NULL,	NULL,		8,	},
	{ "Shotcut",	NULL,	NULL,		8,	},
	{ "Kicad",	NULL,	NULL,		8,	},
	{ "Audacity",	NULL,	NULL,		8,	},
	{ NULL,		"grdc",	NULL,		1,	},
};

/* key definitions */
#define MODKEY (ControlMask|ShiftMask)
#define HOMEBIN "/home/me/bin/"

/* commands */
static const char *termcmd[] = { "env", "LC_CTYPE=en_US.UTF-8", HOMEBIN"st",
	"-F", "-n", "st", "-T", "Terminal", "-e", "tmux",
	NULL };
static const char *emacscmd[] = { "emacs", NULL };
static const char *audacitycmd[] = { "audacity", NULL };
static const char *firefoxcmd[] = { "firefox", "-offline", NULL };
static const char *gimpcmd[] = { "gimp", NULL };
static const char *kicadcmd[] = { "kicad", NULL };
static const char *shotcutcmd[] = { "shotcut", "--fullscreen", NULL };
static const char *torcmd[] = { "tor-browser", NULL };

static Key keys[] = {
  /* modifier(s)	key		function	argument */
  { MODKEY,	 	XK_n,		spawn,          {.v = termcmd } },
  { MODKEY,	 	XK_m,		spawn,          {.v = emacscmd } },
  { MODKEY,	 	XK_y,		spawn,          {.v = audacitycmd } },
  { MODKEY,	 	XK_u,		spawn,          {.v = firefoxcmd } },
  { MODKEY,	 	XK_i,		spawn,          {.v = gimpcmd } },
  { MODKEY,	 	XK_o,		spawn,          {.v = kicadcmd } },
  { MODKEY,	 	XK_p,		spawn,          {.v = shotcutcmd } },
  { MODKEY,	 	XK_comma,	spawn,          {.v = torcmd } },
  { Mod1Mask,		XK_Tab,		view,		{0} },
  { MODKEY, 		XK_semicolon,	focusstack,	{.i = +1 } },
  { MODKEY|Mod1Mask,	XK_semicolon,	focusstack,	{.i = -1 } },
  { MODKEY,	 	XK_c,		killclient,     {0} },
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
/* click can be ClkClientWin or ClkRootWin */
static Button buttons[] = {
/* click                event mask      button          function        argument */
{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
};

