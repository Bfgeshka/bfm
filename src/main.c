#include <dirent.h>
#include <errno.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#define CLEANMASK(mask) (mask & ~(GDK_MOD2_MASK))

/* Structs */
/* Main window */
typedef struct
{
	/* Widgets */
	GtkWidget * wind;
	GtkWidget * scrl;
	GtkWidget * tree;
	/* Current directory */
	gchar     * path;
	/* Showing dotfiles */
	gboolean	dtfl;
	/* Last update */
	time_t      mtim;
} St_win;

/* Passed argument */
typedef struct
{
	gboolean b;
	gint     i;
	/* Universal */
	void   * v;
} St_arg;

/* Keypress action */
typedef struct
{
	/* Keypress modificator (i.e. Shift or Ctrl buttons */
	guint mod;
	/* Key value */
	guint key;
	/* Invoked function */
	void (* func)( St_win * cr_w, const St_arg * args );
	/* Required arguments */
	const St_arg args;
} St_key;

/* Enums */
/* Columns for listed files */
enum ListColumns
{
	NAME_STR,
	PERMS_STR,
	SIZE_STR,
	MTIME_STR,
	IS_DIR
};

/* List movement */
enum Movement
{
	UP,
	DOWN,
	HOME,
	END,
	PAGEUP,
	PAGEDOWN
};

/* Globals */
static GList * windows = NULL;

/* Protos */
GList *  bfm_get_selected  ( St_win * );
St_win * bfm_create_window ( void );
gboolean bfm_keypress      ( GtkWidget *, GdkEventKey *, St_win * );
gchar *  bfm_col_ctr_perm  ( mode_t );
gchar *  bfm_col_ctr_size  ( size_t );
gchar *  bfm_col_ctr_time  ( const char *, const struct tm * );
gchar *  bfm_prev_dir      ( gchar * );
gchar *  bfm_text_dialog   ( GtkWindow *, const gchar *, const gchar * );
gint     bfm_compare       ( GtkTreeModel *, GtkTreeIter *, GtkTreeIter *, gpointer );
gint     bfm_get_mtime     ( const gchar *, time_t * );
int      bfm_name_validat  ( const char * s, int );
void     bfm_action        ( GtkWidget *, GtkTreePath *, GtkTreeViewColumn *, St_win * );
void     bfm_bookmark      ( St_win *, const St_arg * );
void     bfm_destroywin    ( GtkWidget *, St_win * );
void     bfm_dir_exec      ( St_win *, const St_arg * );
void     bfm_list_dir      ( St_win *, const char * );
void     bfm_make_dir      ( St_win *, const St_arg * );
void     bfm_move_cursor   ( St_win *, const St_arg * );
void     bfm_new_window    ( St_win *, const St_arg * );
void     bfm_option_toggle ( St_win *, const St_arg * );
void     bfm_read_files    ( St_win *, DIR * );
void     bfm_reload        ( St_win *, const St_arg * );
void     bfm_remove        ( St_win *, const St_arg * );
void     bfm_set_path      ( St_win *, const St_arg * );
void     bfm_spawn         ( const gchar * const *, const gchar * );
void     bfm_dialog_text   ( GtkWidget *, GtkDialog * );
void     bfm_update        ( St_win * );

/* Include compile-time configuration file */
#include "config.h"

/* Functions */
/* Changes option in runtime */
void
bfm_option_toggle( St_win * cr_w, const St_arg * args )
{
	(void)args;
	cr_w->dtfl = !cr_w->dtfl;
	bfm_reload( cr_w, NULL );
}

/* Checks if filename is beginnings with dot */
int
bfm_name_validat( const char * s, int dot_flag )
{
	return dot_flag ? ( g_strcmp0( s, "." ) != 0 && g_strcmp0( s, ".." ) != 0 ) : * s != '.';
}

/* Reload wrapper with time check */
void
bfm_update( St_win * cr_w )
{
	time_t mtime = 0;

	if ( cr_w->path )
		if ( bfm_get_mtime( cr_w->path, &mtime ) != 0 || mtime > cr_w->mtim )
			bfm_reload(cr_w, NULL);
}

/* Dialog response handler */
void
bfm_dialog_text( GtkWidget * w, GtkDialog * dialog )
{
	(void) w;
	gtk_dialog_response( dialog, 1 );
}

/* Create dialog with a text field */
gchar *
bfm_text_dialog( GtkWindow * p, const gchar * title, const gchar * text )
{
	GtkWidget * dialog = gtk_dialog_new_with_buttons( title, p, GTK_DIALOG_MODAL, NULL );
	GtkWidget * entry = gtk_entry_new();
	GtkWidget * area = gtk_dialog_get_content_area( GTK_DIALOG(dialog) );
	gchar * str = NULL;

	if (text)
		gtk_entry_set_text( GTK_ENTRY(entry), text );

	g_signal_connect( G_OBJECT(entry), "activate", G_CALLBACK(bfm_dialog_text), dialog );

	gtk_container_add( GTK_CONTAINER(area), entry );
	gtk_widget_show(entry);

	if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == 1 )
		str = g_strdup( gtk_entry_get_text( GTK_ENTRY(entry) ) );

	gtk_widget_destroy(dialog);
	return str;
}

/* Invoke external executor */
void
bfm_spawn( const gchar * const * argv, const gchar * path )
{
	char exz[BUFSIZ];
	sprintf( exz, "%s \"%s\"", *argv, path );
	if(system(exz))
	{
		fprintf( stderr, "execution of %s\n", exz );
	}
}

/* Set path to required directory */
void
bfm_set_path( St_win * cr_w, const St_arg * args )
{
	char * path;

	/* Via argument */
	if ( ( path = args->v ) )
		bfm_list_dir( cr_w, path );
	/* Via dialog */
	else if ( ( path = bfm_text_dialog( GTK_WINDOW( cr_w->wind ), "path", cr_w->path ) ) )
	{
		bfm_list_dir( cr_w, path );
		g_free(path);
	}
}

/* Refresh directory list view */
void
bfm_reload( St_win * cr_w, const St_arg * args )
{
	(void) args;
	bfm_list_dir( cr_w, cr_w->path );
}

/* Moving on tree element */
void
bfm_move_cursor( St_win * cr_w, const St_arg * args )
{
	GtkMovementStep m;
	gint v;
	gboolean ret;

	switch ( args->i )
	{
	case UP:
		m = GTK_MOVEMENT_DISPLAY_LINES;
		v = -1;
		break;
	case DOWN:
		m = GTK_MOVEMENT_DISPLAY_LINES;
		v = 1;
		break;
	case HOME:
		m = GTK_MOVEMENT_BUFFER_ENDS;
		v = -1;
		break;
	case END:
		m = GTK_MOVEMENT_BUFFER_ENDS;
		v = 1;
		break;
	case PAGEUP:
		m = GTK_MOVEMENT_PAGES;
		v = -1;
		break;
	case PAGEDOWN:
		m = GTK_MOVEMENT_PAGES;
		v = 1;
		break;
	default:
		return;
	}

	g_signal_emit_by_name( G_OBJECT( cr_w->tree ), "move-cursor", m, v, &ret);
}

/* Create a directory */
void
bfm_make_dir( St_win * cr_w, const St_arg * args )
{
	gchar * path;

	g_return_if_fail( cr_w->path );

	/* Invoke dialog */
	if ( ( path = bfm_text_dialog( GTK_WINDOW( cr_w->wind ), "make directory", NULL ) ) )
	{
		/* Try to create */
		if ( mkdir( path, args->i ) == -1 )
			g_warning( "mkdir: %s", strerror(errno) );
		g_free(path);
	}
}

/* Keypress handler */
gboolean
bfm_keypress( GtkWidget * w, GdkEventKey * ev, St_win * cr_w )
{
	(void) w;
	unsigned i;

	/* Check earch entry in array */
	for ( i = 0; i < ( sizeof(keys) / sizeof(* keys) ); i++ )
	{
		if
		(	/* Find keypress */
			gdk_keyval_to_lower( ev->keyval ) == keys[i].key &&
			CLEANMASK( ev->state ) == keys[i].mod &&
			keys[i].func
		)
			/* Invoke required function */
			keys[i].func( cr_w, &keys[i].args );
	}

	/* Reset boolean variable */
	return FALSE;
}

/* GTK handler for selected element */
GList *
bfm_get_selected( St_win * cr_w )
{
	GtkTreeSelection * sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( cr_w->tree ) );
	GtkTreeModel * model;
	GtkTreeIter iter;
	GList * lsel = gtk_tree_selection_get_selected_rows( sel, &model );
	GList * node = lsel;
	GList * files = NULL;
	gchar * name;

	while (node)
	{
		gtk_tree_model_get_iter( model, &iter, node->data );
		gtk_tree_model_get( model, &iter, NAME_STR, &name, -1 );
		files = g_list_append( files, name );

		node = g_list_next(node);
	}

	g_list_foreach( lsel, (GFunc)gtk_tree_path_free, NULL );
	g_list_free(lsel);

	return files;
}

void
bfm_remove ( St_win * cr_w, const St_arg * args )
{
	GList * sel = bfm_get_selected(cr_w);
	GList * i = sel;
	char name[BUFSIZ];

	while (i)
	{
		sprintf( name, "%s/%s", cr_w->path, (char *)i->data );
		bfm_spawn( rmcmd, name );

		i = g_list_next(i);
	}

//	g_list_foreach( sel, (GFunc)gtk_tree_path_free, NULL );
//	g_list_free(sel);

	bfm_reload( cr_w, args );
}

/* Get modification time */
gint
bfm_get_mtime( const gchar * path, time_t * time )
{
	struct stat st;
	gint err;

	g_return_val_if_fail(time, 0);

	if ( ( err = stat( path, &st ) ) == 0 )
		* time = st.st_mtime;

	return err;
}

/* Execute path */
void
bfm_dir_exec( St_win * cr_w, const St_arg * args )
{
	g_return_if_fail( cr_w->path && args->v );
	bfm_spawn( args->v, cr_w->path );
}

/* Proper window termination */
void
bfm_destroywin( GtkWidget * w, St_win * cr_w)
{
	(void) w;
	if ( (windows = g_list_remove( windows, cr_w ) ) == NULL )
		gtk_main_quit();

	gtk_widget_destroy( cr_w->tree );
	gtk_widget_destroy( cr_w->scrl );
	gtk_widget_destroy( cr_w->wind );

	if ( cr_w->path )
		g_free( cr_w->path );

	g_free(cr_w);
}

/* Return string with modification time */
gchar *
bfm_col_ctr_time( const char * fmt, const struct tm * time )
{
	gchar buf[64];
	strftime( buf, sizeof(buf), fmt, time );
	return g_strdup(buf);
}

/* Return string with file size */
gchar *
bfm_col_ctr_size( size_t size )
{
	/* Bytes */
	if ( size < 1024 )
		return g_strdup_printf( "%i B", (int)size );
	/* KiBytes */
	else if ( size < 1024*1024 )
		return g_strdup_printf( "%.1f KiB", size / 1024.0 );
	/* MiBytes */
	else if ( size < 1024*1024*1024 )
		return g_strdup_printf( "%.1f MiB", size / ( 1024.0 * 1024 ) );
	/* GiBytes */
	else
		return g_strdup_printf( "%.1f GiB", size / ( 1024.0 * 1024 * 1024 ) );
}

/* Return string with file permissions and type */
gchar *
bfm_col_ctr_perm( mode_t mode )
{
	/* File type */
	char ident;
	switch ( mode & S_IFMT )
	{
		case S_IFBLK:	ident = 'b'; break;
		case S_IFCHR:	ident = 'c'; break;
		case S_IFDIR:	ident = 'd'; break;
		case S_IFIFO:	ident = 'p'; break;
		case S_IFLNK:	ident = 'l'; break;
		case S_IFREG:	ident = '-'; break;
		case S_IFSOCK:	ident = 's'; break;
		default:		ident = '?'; break;
	}

	/* File permissions */
	char *permstr[] = { "---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx" };
	return g_strdup_printf
	   (
	   "%c%s%s%s",
	   ident,
	   permstr[ ( mode >> 6 ) & 7 ],
	   permstr[ ( mode >> 3 ) & 7 ],
	   permstr[ mode & 7 ]
	   );
}

gint
bfm_compare( GtkTreeModel * m, GtkTreeIter * a, GtkTreeIter * b, gpointer p )
{
	(void) p;
	gchar * name[2];
	gint isdir[2];
	gint ret;

	gtk_tree_model_get( m, a, NAME_STR, &name[0], IS_DIR, &isdir[0], -1 );
	gtk_tree_model_get( m, b, NAME_STR, &name[1], IS_DIR, &isdir[1], -1 );

	if ( isdir[0] == isdir[1] )
		ret = g_ascii_strcasecmp( name[0], name[1] );
	else
		ret = isdir[0] ? -1 : 1;

	g_free(name[0]);
	g_free(name[1]);
	return ret;
}

/* Go to bookmark */
void
bfm_bookmark( St_win * cr_w, const St_arg * args )
{
	if ( args->i >= 0 && (unsigned)args->i < ( sizeof(bookmarks) / sizeof(* bookmarks) ) )
		bfm_list_dir( cr_w, (char *)bookmarks[ args->i ] );
}

/* Wrapper for element action */
void
bfm_action( GtkWidget * w, GtkTreePath * p, GtkTreeViewColumn * c, St_win * cr_w )
{
	(void) w;
	(void) c;

	/* Declarations */
	GtkTreeIter    iter;
	GtkTreeModel * model = gtk_tree_view_get_model( GTK_TREE_VIEW( cr_w->tree ) );
	gboolean       is_dir;
	gchar          fpath[PATH_MAX];
	gchar *        name;

	/* Creating tree model */
	gtk_tree_model_get_iter( model, &iter, p );
	gtk_tree_model_get
	   (
	   model,
	   &iter,
	   NAME_STR,
	   &name,
	   IS_DIR,
	   &is_dir,
	   -1
	   );

	if ( chdir( cr_w->path ) < 0 )
		g_warning( "chdir: %s", strerror(errno) );

	if ( realpath( name, fpath ) == NULL )
		g_warning( "realpath: %s", strerror(errno) );
	g_free(name);

	if (is_dir)
		/* open directory */
		bfm_list_dir( cr_w, fpath );
	else
		/* execute program */
		bfm_spawn( filecmd, fpath );
}

void
bfm_read_files( St_win * cr_w, DIR * dir )
{
	GtkListStore    * store = GTK_LIST_STORE( gtk_tree_view_get_model( GTK_TREE_VIEW( cr_w->tree ) ) );
	GtkTreeIter       iter;
	GtkTreeSortable * sortable = GTK_TREE_SORTABLE(store);
	gchar           * mtime_str;
	gchar           * name_str;
	gchar           * perms_str;
	gchar           * size_str;
	struct dirent   * e;
	struct stat       st;
	struct tm       * time;

	/* remove previous entries */
	gtk_list_store_clear(store);

	/* disable sort to speed up insertion */
	gtk_tree_sortable_set_sort_column_id
	   (
	   sortable,
	   GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
	   GTK_SORT_ASCENDING
	   );

	while ( ( e = readdir(dir) ) )
	{
		if
		(
			bfm_name_validat( e->d_name, cr_w->dtfl )
			&&
			( stat( e->d_name, &st ) == 0 )
		)
		{
			if ( S_ISDIR( st.st_mode ) )
				name_str = g_strdup_printf( "%s/", e->d_name );
			else
				name_str = g_strdup( e->d_name );

			time = localtime( &st.st_mtime );
			mtime_str = bfm_col_ctr_time( timefmt, time );
			perms_str = bfm_col_ctr_perm( st.st_mode );
			size_str = bfm_col_ctr_size( st.st_size );

			gtk_list_store_append( store, &iter );
			gtk_list_store_set
			   (
			   store,
			   &iter,
			   NAME_STR, name_str,
			   PERMS_STR, perms_str,
			   SIZE_STR, size_str,
			   MTIME_STR, mtime_str,
			   IS_DIR, S_ISDIR(st.st_mode),
			   -1
			   );

			g_free(name_str);
			g_free(mtime_str);
			g_free(perms_str);
			g_free(size_str);
		}
	}

	/* reenable sort */
	gtk_tree_sortable_set_sort_column_id( sortable, NAME_STR, GTK_SORT_ASCENDING );
}

/* Return directory on upper level */
gchar *
bfm_prev_dir( gchar * path )
{
	gchar * p;
	if ( ( p = g_strrstr( path, "/" ) ) )
	{
		if (p == path)
			* (p + 1) = '\0';
		else
			* p = '\0';
	}
	return path;
}

/* Get directory content */
void
bfm_list_dir( St_win * cr_w, const char * str )
{
	g_return_if_fail(str);

	if ( cr_w->path )
		if ( chdir( cr_w->path ) == -1 )
			g_warning( "chdir: %s", strerror(errno) );

	char r_path[PATH_MAX];
	if ( realpath( str, r_path ) == NULL )
		g_warning( "realpath: %s", strerror(errno) );

	/* Try to open directory */
	DIR * dir;
	if ( !( dir = opendir(r_path) ) )
	{
		g_warning( "%s: %s\n", r_path, g_strerror(errno) );

		/* Check if in root */
		if ( strcmp( r_path, "/" ) != 0 )
			bfm_list_dir( cr_w, bfm_prev_dir(r_path) );
		return;
	}

	if ( cr_w->path )
		g_free( cr_w->path );

	/* Fill window struct */
	cr_w->path = g_strdup(r_path);
	if ( chdir( cr_w->path ) == -1 )
		g_warning( "chdir: %s", strerror(errno) );
	bfm_get_mtime( cr_w->path, &cr_w->mtim );
	gtk_window_set_title( GTK_WINDOW( cr_w->wind ), cr_w->path );

	/* Invoke wrapped function */
	bfm_read_files( cr_w, dir );

	closedir(dir);
}

/* Creates new main window */
St_win *
bfm_create_window()
{
	St_win        * cr_w;
	GtkCellRenderer * rend;
	GtkListStore    * store;
	GtkTreeSortable * sortable;

	/* Initialisation */
	cr_w       = g_malloc( sizeof(St_win) );
	cr_w->path = NULL;
	cr_w->wind = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	cr_w->dtfl = show_dotfiles;

	/* Scroll widget usage */
	cr_w->scrl = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy
	   (
	   GTK_SCROLLED_WINDOW( cr_w->scrl ),
	   GTK_POLICY_AUTOMATIC,
	   GTK_POLICY_ALWAYS
	   );

	/* Creating storage for directory content */
	store = gtk_list_store_new
			   (
			   5,
			   G_TYPE_STRING, /* Name */
			   G_TYPE_STRING, /* Prms */
			   G_TYPE_STRING, /* Size */
			   G_TYPE_STRING, /* Mdfd */
			   G_TYPE_BOOLEAN
			   );
	sortable = GTK_TREE_SORTABLE(store);

	/* Creating a widget for list */
	cr_w->tree = gtk_tree_view_new_with_model( GTK_TREE_MODEL(store) );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( cr_w->tree ), TRUE );
	gtk_tree_view_set_rubber_banding( GTK_TREE_VIEW( cr_w->tree ), TRUE );
	gtk_tree_view_set_rules_hint( GTK_TREE_VIEW( cr_w->tree ), TRUE );
	gtk_tree_selection_set_mode
	   (
	   gtk_tree_view_get_selection( GTK_TREE_VIEW( cr_w->tree ) ),
	   GTK_SELECTION_MULTIPLE
	   );

	#define MCR_SET_COLUMN(MCR_COL_NAME,MCR_COL_ENUM)                                              \
	   (gtk_tree_view_insert_column_with_attributes(                                               \
	   GTK_TREE_VIEW( cr_w->tree ), -1, MCR_COL_NAME, rend, "text", MCR_COL_ENUM, NULL) )

	rend = gtk_cell_renderer_text_new();
	MCR_SET_COLUMN( "Name", NAME_STR );
	rend = gtk_cell_renderer_text_new();
	MCR_SET_COLUMN( "Permissions", PERMS_STR );
	rend = gtk_cell_renderer_text_new();
	MCR_SET_COLUMN( "Size", SIZE_STR );
	rend = gtk_cell_renderer_text_new();
	MCR_SET_COLUMN( "Modified", MTIME_STR );

	/* Name column is expanded, others are using only required width */
	gtk_tree_view_column_set_expand
	   (
	   gtk_tree_view_get_column( GTK_TREE_VIEW( cr_w->tree ), 0 ),
	   TRUE
	   );

	/* Setup list sorting */
	gtk_tree_sortable_set_sort_func( sortable, NAME_STR, bfm_compare, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id( sortable, NAME_STR, GTK_SORT_ASCENDING );

	/* Connect signals */
	g_signal_connect( G_OBJECT( cr_w->wind ), "destroy", G_CALLBACK(bfm_destroywin), cr_w );
	g_signal_connect( G_OBJECT( cr_w->wind ), "key-press-event", G_CALLBACK(bfm_keypress), cr_w );
	g_signal_connect( G_OBJECT( cr_w->tree ), "row-activated", G_CALLBACK(bfm_action), cr_w );

	/* Add widgets */
	gtk_container_add( GTK_CONTAINER( cr_w->scrl ), cr_w->tree );
	gtk_container_add( GTK_CONTAINER( cr_w->wind ), cr_w->scrl );

	gtk_widget_show_all( cr_w->wind );
	return cr_w;
}

/* Wrapper for new main window */
void
bfm_new_window( St_win * cr_w, const St_arg * args )
{
	/* Create new window */
	St_win * new = bfm_create_window();
	windows = g_list_append( windows, new );

	/* Invoke wrapped function */
	bfm_list_dir( new, args->v ? args->v : (cr_w ? cr_w->path : NULL) );
}

int
main( int argc, char ** argv )
{
	/* Give arguments to gtk_init() for
	 * GTK+ standart arguments support */
	St_arg args;
	args.v = argv[ argc - 1 ];
	gtk_init( &argc, &argv );

	bfm_new_window( NULL, &args );

	gtk_main();

	return EXIT_SUCCESS;
}
