/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx = 4;        /* border pixel of windows */
static const unsigned int snap     = 32;       /* snap pixel */
static const char col_unfocus[]    = "#676767";
static const char col_focus[]      = "#ce5c00";
static const int yreserve = 0, xreserve = 0;  /* screen space to reserve */

/* tagging */
static const unsigned int numtags = 4;
static const unsigned int inittag = 1, altinittag = 2;

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class	instance	title		tag (<=numtags,!=0) */
	{ "st",		NULL,		NULL,		2,		},
};

/* key definitions */
#define MODKEY (ControlMask|ShiftMask)

/* commands */
static const char *termcmd[] = { "st", NULL }; /* Edit at least this */

static Key keys[] = {
  /* modifier(s)	key		function	argument */
  { MODKEY,	 	XK_n,		spawn,          {.v = termcmd } },
  { Mod1Mask,		XK_Tab,		view,		{0} },
  { MODKEY, 		XK_semicolon,	focusstack,	{.i = +1 } },
  { MODKEY|Mod1Mask,	XK_semicolon,	focusstack,	{.i = -1 } },
  { MODKEY,	 	XK_w,		killclient,     {0} },
  { MODKEY,	 	XK_comma,	minimize,	{0} },
  { MODKEY,	 	XK_period,	maximize,	{0} },
  { MODKEY, 		XK_h,		view,           {.ui = 1 } },
  { MODKEY, 		XK_j,		view,           {.ui = 2 } },
  { MODKEY, 		XK_k,		view,           {.ui = 3 } },
  { MODKEY, 		XK_l,		view,           {.ui = 4 } },
  { MODKEY|Mod1Mask,	XK_h,		tag,		{.ui = 1 } },
  { MODKEY|Mod1Mask,	XK_j,		tag,		{.ui = 2 } },
  { MODKEY|Mod1Mask,	XK_k,		tag,		{.ui = 3 } },
  { MODKEY|Mod1Mask,	XK_l,		tag,		{.ui = 4 } },
  { MODKEY, 		XK_q,		quit,           {0} },
};

/* button definitions */
/* click can be ClkClientWin or ClkRootWin */
static Button buttons[] = {
/* click                event mask      button          function        argument */
{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
};
