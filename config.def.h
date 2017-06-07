#define MODKEY GDK_CONTROL_MASK
#define VERSION "0.1"

/* Terminal */
#define TERMINAL (char *[]){ "st", "-e", "fish", NULL }

/* Bookmarks */
static const char *bookmarks[] = {
	"/",
	"/var/tmp",
	"/home/bfg",
};

/* Time format, check *man date* for more formatting info */
static const char *timefmt = "%Y/%m/%d %H:%M:%S";

/* Poll time for directory updating (in seconds) */
static const int polltime = 15;

/* Command to be executed when activating a file */
static const char *filecmd[] = { "/bin/sh/", "-c", "~/bin/exec_rifle", "\"$BFM_PATH\"", NULL };

/* Key bindings */
static S_key keys[] = {
	/* Movement */
	{ MODKEY,				GDK_j,			bfm_move_cursor,	{ .i = DOWN } },
	{ MODKEY,				GDK_k,			bfm_move_cursor,	{ .i = UP } },
	{ MODKEY|GDK_SHIFT_MASK,GDK_j,			bfm_move_cursor,	{ .i = PAGEDOWN } },
	{ MODKEY|GDK_SHIFT_MASK,GDK_k,			bfm_move_cursor,	{ .i = PAGEUP } },
	{ MODKEY,				GDK_g,			bfm_move_cursor,	{ .i = HOME } },
	{ MODKEY|GDK_SHIFT_MASK,GDK_g,			bfm_move_cursor,	{ .i = END } },

	/* Spawn new window */
	{ MODKEY,				GDK_n,			bfm_new_window,		{ 0 } },

	/* Go up one level */
	{ MODKEY,				GDK_h,			bfm_set_path,		{ .v = ".." } },
	{ 0,					GDK_BackSpace,	bfm_set_path,		{ .v = ".." } },

	/* Terminal launch */
	{ 0,					GDK_F4,			bfm_dir_exec,		{ .v = TERMINAL } },

	/* Make directory */
	{ 0,					GDK_F7,			bfm_make_dir,		{ .i = 0755 } },

	/*{ MODKEY,				GDK_m,			bfm_mv,				{ 0 } },*/

	/* Set path */
	{ MODKEY,				GDK_l,			bfm_set_path,		{ 0 } },

	/* Reload dir*/
	{ MODKEY, 				GDK_r,			bfm_reload,			{ 0 } },

	/* Bookmarks */
	{ MODKEY,                GDK_1,         bfm_bookmark,       { .i = 0 } },
	{ MODKEY,                GDK_2,         bfm_bookmark,       { .i = 1 } },
	{ MODKEY,                GDK_3,         bfm_bookmark,       { .i = 2 } },
};
