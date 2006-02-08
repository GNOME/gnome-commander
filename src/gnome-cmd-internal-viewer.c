/*
    File Viewer Gtk Widget for GNOME Commander 
    Copyright (C) 2006 Assaf Gordon.

    Code Used from the following projects:
	GNOME Commander - A GNOME based file manager 
	Copyright (C) 2001-2004 Marcus Bjurman

	View file module for the Midnight Commander
	Copyright (C) 1994, 1995, 1996 The Free Software Foundation
	Written by: 1994, 1995, 1998 Miguel de Icaza
		1994, 1995 Janne Kukonlehto
		1995 Jakub Jelinek
		1996 Joseph M. Hinkle
		1997 Norbert Warmuth
		1998 Pavel Machek

	gtkhex.c - a GtkHex widget, modified for use in GHex
	Author: Jaka Mocnik <jaka@gnu.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 

#ifndef NO_GNOME_CMD
#define HAVE_GNOME_CMD
#endif

#define HAVE_MMAP

#ifdef HAVE_GNOME_CMD
	#include <config.h>
	#include "gnome-cmd-includes.h"
#else
	#include <errno.h>
	#include <unistd.h>
	#include <string.h>
	#include <stdlib.h>
	#include <gdk/gdkx.h>
	#include <gtk/gtk.h>
#endif

#include <gdk/gdkevents.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#ifdef HAVE_GNOME_CMD
#include "imageloader.h"
#include "utils.h"
#endif

#include "gnome-cmd-internal-viewer.h"

#ifndef HAVE_GNOME_CMD
/* when compiling as part of GnomeCommander, these are found in 'utils.h' */
#if 0
#define DEBUG(...) 
#else
#define DEBUG debug
#endif

#define _(x) (x)
void debug (gchar flag, const gchar *format, ...)
{
	va_list ap;		
	va_start(ap, format);
	fprintf (stderr, "[%c%c] ", flag-32, flag-32);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
#endif

#define G_OBJ_CHARSET_KEY "charset"

#define DBG DEBUG('v',"%s:%d\n",__FUNCTION__,__LINE__);

#define READ_BLOCK 8192
#define VIEW_PAGE_SIZE 8192
#define BUF_MEDIUM 256
#define BUF_LARGE 1024
#define LINES 80
#define MAX_TAB_SIZE 16
#define DEFAULT_TAB_SIZE 8
#define DEFAULT_FIXED_FONT_NAME "Monospace"
#define DEFAULT_VARIABLE_FONT_NAME "Lucida"
#define DEFAULT_FONT_SIZE 13
#define MAX_FONT_SIZE 72
#define MIN_FONT_SIZE 6

typedef enum
{
	DISP_FIXED, /* ASCII/Unicode/UTF/Localized Input, Fixed Font, either wrap or no wrap */
	DISP_VARIABLE, /* ASCII/Unicode/UTF/Localized Input, Variable Font, either wrap or no wrap */
	DISP_BINARY, /* ASCII input, fixed font, line length fixed at 80 chars per line */
	DISP_HEX,   /* ASCII input, fixed font, hex display (16 bytes per line) */
	DISP_IMAGE /* Image display using GtkWidget */
} DISPLAYMODE ;

typedef enum 
{
	MI_NORMAL,
	MI_CHECK,
} MENUITEMTYPE;

typedef enum
{
	ROTATE_CLOCKWISE,
	ROTATE_COUNTERCLOCKWISE,
	ROTATE_UPSIDEDOWN,
	FLIP_VERTICAL,
	FLIP_HORIZONTAL
} PIXBUF_MANIPULATION ;

/* Offset in bytes into a file */
typedef unsigned long offset_type;

typedef guint32  char_type ;

/* TODO: Change these for Big-Endian machines */
#define FIRST_BYTE(x)  ((x)&0xFF)
#define SECOND_BYTE(x) (((x)>>8)&0xFF)
#define THIRD_BYTE(x)  (((x)>>16)&0xFF)
#define FOURTH_BYTE(x) (((x)>>24)&0xFF)

#define INVALID_OFFSET ((offset_type) -1)

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))

/* display mode function types */
typedef offset_type (*dispmode_get_text_proc)(GnomeCmdInternalViewer *iv, offset_type start);
typedef void (*dispmode_move_lines_proc)(GnomeCmdInternalViewer *iv, int delta) ;
typedef void (*dispmode_scroll_to_offset_proc)(GnomeCmdInternalViewer *iv, offset_type offset) ;

/* input function types */
typedef char_type (*input_get_char_proc)(GnomeCmdInternalViewer *iv, offset_type offset);
typedef offset_type (*input_get_offset_proc)(GnomeCmdInternalViewer *iv, offset_type offset);



static unsigned int ascii_cp437_to_unicode[256];

static GnomeCmdInternalViewerClass *parent_class = NULL;

struct _GnomeCmdInternalViewerPrivate 
{
	/* Gtk User Interface */
	GtkWidget *vbox1;
	GtkWidget *menu;
	GtkWidget *table1;
	GtkWidget *vscrollbar1;
	GtkWidget *hscrollbar1;
	GtkWidget *statusbar;
	GtkWidget *xdisp;
	GtkAccelGroup *accel_group;
	GtkWidget *ascii_menu_item;
	GtkWidget *imagedisp ; /* lazy initialization */

	GtkAdjustment *hadj;
	GtkAdjustment *vadj;
	
	GdkPixbuf *orig_pixbuf ;
	
	/* context id and text buffer for the status bar */
	guint sb_msg_ctx_id;	
	gchar statusbar_msg[BUF_LARGE];
	
	/* Pango Related Stuff (based on 'ghex2'-'s gtkhex.c) */
	PangoFontMetrics *disp_font_metrics;
	PangoFontDescription *font_desc;
	PangoLayout *xlayout;
	GdkGC *gc;
	
	/* These are calculated in "recalc_display" */
	int char_width; 
	int char_height;
	int xdisp_width ;
	int xdisp_height ;

	/* persistant buffers for ASCII/Unicode/UTF handling */
	char_type *line_buffer ;	/* buffer to hold a line's data,  */
				/* will be re-alloc'd to the longest line length */
	
	int line_buffer_alloc_len ; /* number of elements g_allocated for 'line_buffer' */
	int line_buffer_content_len ; /* number of Elements (not bytes) currently in 'line_buffer' */

	unsigned char *line_work_buffer ; /* temporary buffer for Pango's UTF-8 strings */
		
	int hscroll_upper ;	/* the length of the longest line, so far */

	/* File handling (based on 'Midnight Commander'-'s view.c) */
	char *filename;		/* Name of the file */
	unsigned char *data;	/* Memory area for the file to be viewed */
	int file;		/* File descriptor (for mmap and munmap) */
	int mmapping;		/* Did we use mmap on the file? */
	
	/* Growing buffers information */
	int growing_buffer;	/* Use the growing buffers? */
	char **block_ptr;	/* Pointer to the block pointers */
	int   blocks;		/* The number of blocks in *block_ptr */
	struct stat s;		/* stat for file */
	
	/* Display information */
	int bytes_per_line;	/* Number of bytes per line in ascii/hex/binary modes */
	int lines_displayed;	/* number of displayeable lines on the window */
	gboolean image_displayed ; /* TRUE if we're showing the imagedisp widget */
	int current_col ;
	
	/* Charset/Codepage conversions */
	gchar *from_charset;    /* to_charset is always UTF-8 for Pango */
	GIConv icnv;		/* GLib's IConvert interface/emulation */
	
	offset_type logical_display_offset; /* logical display offset */
				    /* when in ASCII mode, logical offset=byte offset */
				    /* in other modes, this isn't nesseceraly so */
	
	
	offset_type last;           /* Last byte shown */
	offset_type last_byte;      /* Last byte of file */
	offset_type first;		/* First byte in file */
	offset_type bottom_first;	/* First byte shown when very last page is displayed */
					/* For the case of WINCH we should reset it to -1 */
	offset_type bytes_read;     /* How much of file is read */
	
	/* user settings */
	GnomeCmdStateVariables state;
	
	/* Two helper variables, to avoid strcmp(state.charset) every time */
	char_type codepage_translation_cache[256];
	
	DISPLAYMODE dispmode ;
	gchar *current_font ;
	
	/* TODO: make colors work... */
	GdkColor foreground_color;
	GdkColor background_color;

	/* display mode functions */
	dispmode_scroll_to_offset_proc dispmode_scroll_to_offset ;
	dispmode_move_lines_proc dispmode_move_lines ;
	dispmode_get_text_proc dispmode_get_text;

	input_get_char_proc input_get_char ;
	input_get_offset_proc input_get_next_char_offset ;
	input_get_offset_proc input_get_prev_char_offset ;
};


void print_hex_buf(unsigned char*buf, int len)
{
	int i;
	for (i=0;i<len;i++) {
		printf("%02X ", buf[i]);
	}
	printf("\n");
}

/* Gtk class related functions */
static void init (GnomeCmdInternalViewer *iv);
static void class_init (GnomeCmdInternalViewerClass *class);
static void destroy (GtkObject *object);
static void map (GtkWidget *widget);

/* Event Handlers */
static void gc_iv_menu_file_close (GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_view_mode_ascii(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_view_mode_binary(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_view_mode_hex(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_view_mode_pango(GtkMenuItem *item, GnomeCmdInternalViewer *iv); 
static void gc_iv_menu_view_mode_image(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_view_set_charset(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_image_rotate_cw(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_image_rotate_ccw(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_image_rotate_usd(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_image_flip_horz(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_image_flip_vert(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_view_wrap(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_view_input_ascii(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_view_input_utf8(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_view_larger_font(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_view_smaller_font(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_view_mode_bin_20_chars(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_view_mode_bin_40_chars(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_view_mode_bin_80_chars(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_settings_hex_decimal_offset(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_settings_save_settings (GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_menu_help_quick_help(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;
static void gc_iv_menu_help_about(GtkMenuItem *item, GnomeCmdInternalViewer *iv) ;

static void gc_iv_size_allocate (GtkWidget *w, GtkAllocation *alloc, GnomeCmdInternalViewer *iv);
/*static void gc_iv_size_allocate_image(GtkWidget *w, GtkAllocation *alloc, GnomeCmdInternalViewer *iv) ;*/
static void gc_iv_drawing_area_exposed (GtkWidget *w, GdkEventExpose *event, GnomeCmdInternalViewer *iv) ;
static void gc_iv_drawing_area_realized(GtkWidget *widget, GnomeCmdInternalViewer *iv) ;

static gboolean gc_iv_scrolled(GtkWidget *widget, GdkEventScroll *event, GnomeCmdInternalViewer *iv) ;
static gboolean gc_iv_vscroll (GtkRange *range,GtkScrollType scroll,gdouble value,GnomeCmdInternalViewer *iv);
static gboolean gc_iv_hscroll (GtkRange *range,GtkScrollType scroll,gdouble value,GnomeCmdInternalViewer *iv);
static gboolean gc_iv_delete_event (GtkWidget *widget,GdkEvent  *event,GnomeCmdInternalViewer *iv );
static gboolean gc_iv_key_pressed (GtkWidget *widget,GdkEventKey  *event,GnomeCmdInternalViewer *iv );


/* internal initialization functions */
static void gc_iv_setup_font(GnomeCmdInternalViewer *iv);
static void gc_iv_recalc_display(GnomeCmdInternalViewer* iv);
static void gc_iv_init_data(GnomeCmdInternalViewer *iv);
static GtkWidget* gc_iv_create_menu_seperator(GtkWidget* container);
static GtkWidget* gc_iv_create_menu_item (MENUITEMTYPE type,const gchar* name,GtkWidget* container,
	GtkAccelGroup* accel,guint keyval,guint modifier,GCallback callback,gpointer userdata);
static GtkWidget* gc_iv_create_radio_menu_item (GSList **group,const gchar* name,GtkWidget* container,
	GtkAccelGroup* accel,guint keyval,guint modifier,GCallback callback,gpointer userdata);
static GtkWidget* gc_iv_create_sub_menu(const gchar *name, GtkWidget* container);
static GtkWidget* internal_viewer_create_menu(GnomeCmdInternalViewer *iv);
static void gc_iv_set_input_mode(GnomeCmdInternalViewer *iv, const gchar *from_charset);
static void gc_iv_set_display_mode(GnomeCmdInternalViewer *iv, DISPLAYMODE newmode) ;
static void gc_iv_pixbuf_manipulation(GnomeCmdInternalViewer *iv, PIXBUF_MANIPULATION manip) ;
/* 
	File Handling functions (based on Midnight Commander's view.c) 
	
	'open' & 'close' : just open and close the file handle
	'load' & 'free' : allocate & free buffer memory, call mmap/munmap
	
	calling order should be: open->load->[use file with "get_byte"]->free (which calls close)
*/
static int gc_iv_file_open(GnomeCmdInternalViewer *iv, const gchar* _file);
static char *gc_iv_file_load (GnomeCmdInternalViewer *iv, int fd);
static char *gc_iv_file_init_growing_view (GnomeCmdInternalViewer *iv, const char *name, const char *filename);
static int gc_iv_file_get_byte (GnomeCmdInternalViewer *iv, unsigned int byte_index);
static void gc_iv_file_close (GnomeCmdInternalViewer *iv);
static void gc_iv_file_free (GnomeCmdInternalViewer *iv);

/* Utility functions */
static DISPLAYMODE guess_display_mode(const unsigned char* data, int len);
static guint get_max_char_width(GtkWidget *widget, PangoFontDescription *font_desc, PangoFontMetrics *font_metrics);
static PangoFontMetrics* load_font (const char *font_name);
const char *unix_error_string (int error_num);
static int unicode2utf8(unsigned int unicode, unsigned char*out);
static void load_settings(GnomeCmdStateVariables *state) ;
static void save_settings(GnomeCmdStateVariables *state) ;

static void gc_iv_set_auto_display_mode(GnomeCmdInternalViewer *iv) ;
static void gc_iv_set_wrap_mode(GnomeCmdInternalViewer *iv, gboolean wrap) ;
static void gc_iv_set_image_widget(GnomeCmdInternalViewer *iv, gboolean showimage);
static void gc_iv_grow_line_buffer(GnomeCmdInternalViewer *iv, int newsize) ;
static void gc_iv_clear_line_buffer(GnomeCmdInternalViewer *iv) ;

/*static void gc_iv_resize_image(GnomeCmdInternalViewer *iv) ;*/


/* Generic display functions */
static gboolean gc_iv_gtk_ready(GnomeCmdInternalViewer* iv);
static void gc_iv_refresh_display(GnomeCmdInternalViewer *iv) ;

/* text mode display functions */
static offset_type gc_iv_ascii_get_text_line(GnomeCmdInternalViewer *iv, offset_type start);
static offset_type gc_iv_find_previous_text_line(GnomeCmdInternalViewer *iv, offset_type start);
static offset_type gc_iv_find_previous_crlf(GnomeCmdInternalViewer *iv, offset_type start);

static void gc_iv_ascii_move_lines(GnomeCmdInternalViewer *iv, int delta) ;
static void gc_iv_ascii_scroll_to_offset(GnomeCmdInternalViewer *iv, offset_type offset) ;
static void gc_iv_ascii_draw_line(GnomeCmdInternalViewer *iv, int x, int y) ;
static void gc_iv_ascii_draw_screen(GnomeCmdInternalViewer *iv);

/* Binary mode display functions */
static offset_type gc_iv_binary_get_text_line(GnomeCmdInternalViewer *iv, offset_type start);
static void gc_iv_binary_move_lines(GnomeCmdInternalViewer *iv, int delta) ;
static void gc_iv_binary_scroll_to_offset(GnomeCmdInternalViewer *iv, offset_type offset) ;

/* Hex mode display functions */
static offset_type gc_iv_hex_get_text_line(GnomeCmdInternalViewer *iv, offset_type start);
static void gc_iv_hex_move_lines(GnomeCmdInternalViewer *iv, int delta) ;
static void gc_iv_hex_scroll_to_offset(GnomeCmdInternalViewer *iv, offset_type offset) ;


/* ASCII (with or without charset conversions) input functions */
static char_type gc_iv_ascii_get_char(GnomeCmdInternalViewer *iv, offset_type offset);
static offset_type gc_iv_ascii_get_previous_char_offset(GnomeCmdInternalViewer *iv, offset_type offset);
static offset_type gc_iv_ascii_get_next_char_offset(GnomeCmdInternalViewer *iv, offset_type offset);

/* UTF-8 input functions */
static char_type gc_iv_utf8_get_char(GnomeCmdInternalViewer *iv, offset_type offset);
static offset_type gc_iv_utf8_get_previous_char_offset(GnomeCmdInternalViewer *iv, offset_type offset);
static offset_type gc_iv_utf8_get_next_char_offset(GnomeCmdInternalViewer *iv, offset_type offset);


/*ToDO: gc_iv_ascii_adjust_char_offset_boundries(GnomeCmdIntenalViewer (iv, offset_type offset) */

/***********************************
* Public functions
* (declated in the header)
***********************************/
GtkType gnome_cmd_internal_viewer_get_type (void)
{
	static GtkType type = 0;
	if (type == 0)	{
		GtkTypeInfo info =
		{
			"GnomeCmdInternalViewer",
			sizeof (GnomeCmdInternalViewer),
			sizeof (GnomeCmdInternalViewerClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};
		type = gtk_type_unique (gtk_window_get_type(), &info);
	}
	return type;
}

GtkWidget* gnome_cmd_internal_viewer_new ()
{
	return gtk_type_new (gnome_cmd_internal_viewer_get_type());
}

void do_internal_file_view(const gchar * path)
{
	GtkWidget *iv;
	iv = gnome_cmd_internal_viewer_new();
	gnome_cmd_internal_viewer_load_file(GNOME_CMD_INTERNAL_VIEWER(iv),path);
}

void gnome_cmd_internal_viewer_load_file (GnomeCmdInternalViewer *iv, const gchar *filename)
{
	g_return_if_fail (GNOME_CMD_IS_INTERNAL_VIEWER(iv));
	gc_iv_file_open(iv,filename);
	gc_iv_set_auto_display_mode(iv);
}

/*****************************************************************
* Gtk class functions
****************************************************************/

/* Initializes the Gtk object, user interface and the private data structure */
static void init (GnomeCmdInternalViewer *iv)
{
	GtkWindow *window ;
	
	iv->priv = g_new (GnomeCmdInternalViewerPrivate, 1);
	memset(iv->priv,0,sizeof(GnomeCmdInternalViewerPrivate));

	gc_iv_init_data(iv);
	
	window = GTK_WINDOW(iv);
	
	gtk_window_set_title(GTK_WINDOW(window), "Viewer");
	
	iv->priv->imagedisp = NULL ;
	
	gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 
		iv->priv->state.rect.width, iv->priv->state.rect.height);
	
	g_signal_connect ( G_OBJECT(iv), "key_press_event",
			G_CALLBACK(gc_iv_key_pressed), iv);
	
	g_signal_connect (G_OBJECT (window), "delete_event",
			G_CALLBACK (gc_iv_delete_event), iv);
			
			
	/* Setup Child widgets */
	iv->priv->vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (iv->priv->vbox1);
	gtk_container_add (GTK_CONTAINER (window), iv->priv->vbox1);
	
	iv->priv->menu = internal_viewer_create_menu(iv);
	gtk_widget_show (iv->priv->menu);
	gtk_box_pack_start (GTK_BOX (iv->priv->vbox1), iv->priv->menu, FALSE, FALSE, 0);
	
	iv->priv->table1 = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (iv->priv->table1);
	gtk_box_pack_start (GTK_BOX (iv->priv->vbox1), iv->priv->table1, TRUE, TRUE, 0);
	
	/* Create GtkDrawingArea, connect signals and create fonts */
	iv->priv->xdisp = gtk_drawing_area_new();
	
	/* explanation for the extra ref count in 'gc_iv_set_image_widget' */
	g_object_ref(iv->priv->xdisp); 
	
	gtk_table_attach (GTK_TABLE (iv->priv->table1),iv->priv->xdisp , 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
		   
	gc_iv_setup_font(iv);
	
	iv->priv->xlayout = gtk_widget_create_pango_layout (iv->priv->xdisp, "H");

	g_signal_connect ( G_OBJECT(iv->priv->xdisp), "expose_event",
			G_CALLBACK(gc_iv_drawing_area_exposed), iv);
	g_signal_connect ( G_OBJECT(iv->priv->xdisp), "scroll_event",
			G_CALLBACK(gc_iv_scrolled), iv);
	g_signal_connect ( G_OBJECT(iv->priv->xdisp), "realize",
			G_CALLBACK(gc_iv_drawing_area_realized), iv);
	g_signal_connect ( G_OBJECT(iv->priv->xdisp), "size-allocate",
			G_CALLBACK(gc_iv_size_allocate), iv);
	gtk_widget_add_events(iv->priv->xdisp, GDK_SCROLL_MASK);
	gtk_widget_show(iv->priv->xdisp);		
	
	/* Create two scroll bars, connect signals and adjustments */
	iv->priv->vadj = (GtkAdjustment*)gtk_adjustment_new(0,0,0,0,0,0);
	iv->priv->vscrollbar1 = gtk_vscrollbar_new (iv->priv->vadj);
	gtk_widget_show (iv->priv->vscrollbar1);
	gtk_table_attach (GTK_TABLE (iv->priv->table1), iv->priv->vscrollbar1, 1, 2, 0, 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_FILL), 0, 0);
	g_signal_connect ( G_OBJECT(iv->priv->vscrollbar1), "change-value",
			G_CALLBACK(gc_iv_vscroll), iv);

	iv->priv->hadj = (GtkAdjustment*)gtk_adjustment_new(0,0,0,0,0,0);
	iv->priv->hscrollbar1 = gtk_hscrollbar_new (iv->priv->hadj);
	gtk_widget_show (iv->priv->hscrollbar1);
	gtk_table_attach (GTK_TABLE (iv->priv->table1), iv->priv->hscrollbar1, 0, 1, 1, 2,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_FILL), 0, 0);
	g_signal_connect ( G_OBJECT(iv->priv->hscrollbar1), "change-value",
			G_CALLBACK(gc_iv_hscroll), iv);

	/* create status bar */
	iv->priv->statusbar = gtk_statusbar_new ();
  	gtk_widget_show (iv->priv->statusbar);
  	gtk_box_pack_start (GTK_BOX (iv->priv->vbox1), iv->priv->statusbar, FALSE, FALSE, 0);
			
	gtk_widget_show (GTK_WIDGET(window));
		
#ifdef HAVE_GNOME_CMD
	/* use the GnomeCommander's Icon */
	gdk_window_set_icon (GTK_WIDGET(window)->window, NULL,
						IMAGE_get_pixmap (PIXMAP_LOGO),
						IMAGE_get_mask (PIXMAP_LOGO));
#endif
}

static void destroy (GtkObject *object)
{
	DEBUG('v',"destroy\n");

	GnomeCmdInternalViewer *iv = GNOME_CMD_INTERNAL_VIEWER (object);

	/* see explanation for this in 'gc_iv_set_image_widget' */
	g_object_unref(iv->priv->xdisp);
	if (iv->priv->imagedisp)
		g_object_unref(iv->priv->imagedisp);
	
	if (iv->priv->icnv && iv->priv->icnv!=(GIConv)-1)
		g_iconv_close(iv->priv->icnv) ;
	iv->priv->icnv = (GIConv)-1 ;

	/* copied from MC's view.c "view_done" */
	if (!iv->priv->mmapping && !iv->priv->growing_buffer && iv->priv->data != NULL) {
		g_free (iv->priv->data);
		iv->priv->data = NULL;
	}

	if (iv->priv->disp_font_metrics)
		pango_font_metrics_unref(iv->priv->disp_font_metrics);
	if (iv->priv->font_desc)
		pango_font_description_free(iv->priv->font_desc);

	/* free allocated buffers, munmap, and close the file handle */
	gc_iv_file_free(iv);
	
	if (iv->priv->line_buffer)
		g_free (iv->priv->line_buffer) ;
	
	if (iv->priv->line_work_buffer)
		g_free (iv->priv->line_work_buffer);
	
	iv->priv->line_buffer = NULL ;
	iv->priv->line_work_buffer = NULL ;
	iv->priv->line_buffer_alloc_len = 0 ;
	iv->priv->line_buffer_content_len = 0 ;
	
	if (iv->priv)
		g_free (iv->priv);

	/* TODO: find out why these hangs the whole application
	         may it got something to do with destroying the parent GtkWindow object/class? */	
//	if (GTK_OBJECT_CLASS (parent_class)->destroy)
//		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);

	#ifndef HAVE_GNOME_CMD
	g_warning("sending gtk_main_quit because HAVE_GNOME_CMD is not defined\n");
	gtk_main_quit();
	#endif
}

static void map (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
		GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdInternalViewerClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	parent_class = gtk_type_class (gtk_window_get_type ());
	
	object_class->destroy = destroy;
	widget_class->map = map;	
}


/**************************************************
 Event Handlers, for both the main GtkWindow and the GtkDrawingArea
**************************************************/
static gboolean gc_iv_scrolled(GtkWidget *widget, GdkEventScroll *event, GnomeCmdInternalViewer *iv) 
{
	switch(event->direction)
	{
	case GDK_SCROLL_UP: 
		iv->priv->dispmode_move_lines(iv, -1);
		return TRUE;
		
	case GDK_SCROLL_DOWN: 
		iv->priv->dispmode_move_lines(iv, 1);
		return TRUE;
		
	case GDK_SCROLL_LEFT: 
		if (!iv->priv->state.ascii_wrap && iv->priv->current_col>0) {
			iv->priv->current_col-- ;
			gc_iv_refresh_display(iv);
		}
		return TRUE;
	
	case GDK_SCROLL_RIGHT: 
		if (!iv->priv->state.ascii_wrap && iv->priv->current_col<iv->priv->hscroll_upper) {
			iv->priv->current_col++ ;
			gc_iv_refresh_display(iv);
		}
		return TRUE;
	}
	
	return FALSE;
}

static void gc_iv_drawing_area_realized(GtkWidget *widget, GnomeCmdInternalViewer *iv) 
{
	iv->priv->gc = gdk_gc_new(iv->priv->xdisp->window);
	gdk_gc_set_exposures(iv->priv->gc, TRUE);
	
	iv->priv->char_width = get_max_char_width(iv->priv->xdisp, iv->priv->font_desc, 
			iv->priv->disp_font_metrics);
	
	iv->priv->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (iv->priv->disp_font_metrics)) + 
			PANGO_PIXELS(pango_font_metrics_get_descent (iv->priv->disp_font_metrics)) + 2;
			
	DEBUG('v',"realized, char width = %d, height = %d\n", iv->priv->char_width, iv->priv->char_height);	

	gc_iv_recalc_display(iv);
}

static void gc_iv_drawing_area_exposed(GtkWidget *w, GdkEventExpose *event, GnomeCmdInternalViewer *iv) 
{
	if (event->count > 0)
		return ;
	gc_iv_ascii_draw_screen(iv);
}

/*******************************
* Event Handlers for GnomeCmdInternalViewer Window
********************************/
static void gc_iv_menu_help_quick_help(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
}

static void gc_iv_menu_help_about(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
}

static void gc_iv_menu_view_mode_ascii(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_display_mode(iv,DISP_FIXED);
}

static void gc_iv_menu_view_mode_binary(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_display_mode(iv,DISP_BINARY);
}

static void gc_iv_menu_view_mode_hex(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_display_mode(iv,DISP_HEX);
}

static void gc_iv_menu_view_mode_pango(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_display_mode(iv,DISP_VARIABLE);
}

static void gc_iv_menu_view_mode_image(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_display_mode(iv,DISP_IMAGE);
}

static void gc_iv_menu_view_larger_font(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	if (iv->priv->state.font_size<MAX_FONT_SIZE) {
		iv->priv->state.font_size++ ;
		gc_iv_setup_font(iv);
	}
}

static void gc_iv_menu_view_smaller_font(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	if (iv->priv->state.font_size>MIN_FONT_SIZE) {
		iv->priv->state.font_size-- ;
		gc_iv_setup_font(iv);
	}
}

static void gc_iv_menu_view_wrap(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_wrap_mode(iv,!iv->priv->state.ascii_wrap);
}

static void gc_iv_menu_view_input_ascii(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_input_mode(iv,"ASCII");
	gc_iv_refresh_display(iv);
}

static void gc_iv_menu_view_input_utf8(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gc_iv_set_input_mode(iv,"UTF8");
	gc_iv_refresh_display(iv);
}

static void gc_iv_menu_view_set_charset(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{	
	gchar *charset = NULL ;
	
	charset = g_object_get_data(G_OBJECT(item),G_OBJ_CHARSET_KEY);
	g_return_if_fail(charset!=NULL);
	
	/* ASCII is set implicitly when setting a charset */
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(iv->priv->ascii_menu_item), TRUE);
	
	gc_iv_set_input_mode(iv,charset);
	gc_iv_refresh_display(iv);
}

static void gc_iv_menu_image_rotate_cw(GtkMenuItem *item, GnomeCmdInternalViewer *iv)
{
	gc_iv_pixbuf_manipulation(iv,ROTATE_CLOCKWISE);
}

static void gc_iv_menu_image_rotate_ccw(GtkMenuItem *item, GnomeCmdInternalViewer *iv)
{
	gc_iv_pixbuf_manipulation(iv,ROTATE_COUNTERCLOCKWISE);
}

static void gc_iv_menu_image_rotate_usd(GtkMenuItem *item, GnomeCmdInternalViewer *iv)
{
	gc_iv_pixbuf_manipulation(iv,ROTATE_UPSIDEDOWN);
}

static void gc_iv_menu_image_flip_horz(GtkMenuItem *item, GnomeCmdInternalViewer *iv)
{
	gc_iv_pixbuf_manipulation(iv,FLIP_HORIZONTAL);
}

static void gc_iv_menu_image_flip_vert(GtkMenuItem *item, GnomeCmdInternalViewer *iv)
{
	gc_iv_pixbuf_manipulation(iv,FLIP_VERTICAL);
}





static void gc_iv_menu_view_mode_bin_20_chars(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	iv->priv->state.binary_bytes_per_line = 20 ;
	gc_iv_refresh_display(iv);
}

static void gc_iv_menu_view_mode_bin_40_chars(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	iv->priv->state.binary_bytes_per_line = 40 ;
	gc_iv_refresh_display(iv);
}

static void gc_iv_menu_view_mode_bin_80_chars(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	iv->priv->state.binary_bytes_per_line = 80 ;
	gc_iv_refresh_display(iv);
}

static void gc_iv_menu_settings_hex_decimal_offset(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	iv->priv->state.hex_decimal_offset = !iv->priv->state.hex_decimal_offset;
	gc_iv_refresh_display(iv);
}


static void gc_iv_menu_file_close(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	DEBUG('v',"menu_file_close\n");
	gtk_widget_destroy(GTK_WIDGET(iv));
}

static void gc_iv_menu_settings_save_settings(GtkMenuItem *item, GnomeCmdInternalViewer *iv) 
{
	gtk_window_get_position(GTK_WINDOW(iv), &iv->priv->state.rect.x, &iv->priv->state.rect.y ) ;
	gtk_window_get_size(GTK_WINDOW(iv), &iv->priv->state.rect.width, &iv->priv->state.rect.height) ;
	
	save_settings(&iv->priv->state);
}


static gboolean gc_iv_vscroll (GtkRange *range,
                                            GtkScrollType scroll,
                                            gdouble value,
                                            GnomeCmdInternalViewer *iv)
{
	offset_type offset;
	
	if (value<0)
		value = 0 ;
	if (value>iv->priv->s.st_size-1)
		value = iv->priv->s.st_size-1;

		
	switch(scroll)
	{
	case GTK_SCROLL_JUMP: 
		offset = (offset_type)value ;
		iv->priv->dispmode_scroll_to_offset(iv, offset);
		break;
		
	case GTK_SCROLL_STEP_BACKWARD: 
		iv->priv->dispmode_move_lines(iv, -1 );
		return TRUE;
		
	case GTK_SCROLL_STEP_FORWARD: 
		iv->priv->dispmode_move_lines(iv, 1 );
		return TRUE;
		
	case GTK_SCROLL_PAGE_BACKWARD: 
		iv->priv->dispmode_move_lines(iv, -1 * (iv->priv->lines_displayed-1) );
		return TRUE;
		
	case GTK_SCROLL_PAGE_FORWARD: 
		iv->priv->dispmode_move_lines(iv, iv->priv->lines_displayed-1 );
		return TRUE;
		
	default:
		break; 
  	}
	
	return TRUE;
}


static gboolean gc_iv_hscroll (GtkRange *range,
                                            GtkScrollType scroll,
                                            gdouble value,
                                            GnomeCmdInternalViewer *iv)
{
	if (iv->priv->state.ascii_wrap)
		return FALSE;
	
	if (value<0)
		value = 0 ;
	if (value>iv->priv->hscroll_upper)
		value = value>iv->priv->hscroll_upper;
		
	iv->priv->current_col = (int)value; 
	gc_iv_refresh_display(iv);
		
	return FALSE;
}
								    
static gboolean gc_iv_delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              GnomeCmdInternalViewer *iv )
{
	g_warning ("delete event occurred\n");
	
	
	return FALSE;
}

/*
static void gc_iv_size_allocate_image(GtkWidget *w, GtkAllocation *alloc, GnomeCmdInternalViewer *iv) 
{
	DEBUG('v',"Size-allocate-Image, width = %d\n", alloc->width ) ;
	gc_iv_resize_image(iv);
}
*/
static void gc_iv_size_allocate(GtkWidget *w, GtkAllocation *alloc, GnomeCmdInternalViewer *iv) 
{
	DEBUG('v',"Size-allocate, width = %d\n", alloc->width ) ;
	gc_iv_recalc_display(iv);
}

static gboolean gc_iv_key_pressed( GtkWidget *widget,
                    		GdkEventKey  *event,
                    		GnomeCmdInternalViewer *iv )
{
	if (event->state & GDK_CONTROL_MASK) 
	{
		switch(event->keyval)
		{
		case GDK_W:
		case GDK_w:
			gc_iv_menu_file_close(NULL,iv);
			break ;
		default:
			return FALSE;
		}
		return TRUE ;
	}

	switch(event->keyval)
	{
	case GDK_Up:
		iv->priv->dispmode_move_lines(iv, -1);
		break ;

	case GDK_Page_Up:
		iv->priv->dispmode_move_lines(iv, -1 * (iv->priv->lines_displayed-1) );
		break ;
	
	case GDK_Page_Down:
		iv->priv->dispmode_move_lines(iv, iv->priv->lines_displayed-1 );
		break ;
		
	case GDK_Down:
		iv->priv->dispmode_move_lines(iv, 1);
		break ;
		
	case GDK_Left:
		if (!iv->priv->state.ascii_wrap && iv->priv->current_col>0) {
			iv->priv->current_col-- ;
			gc_iv_refresh_display(iv);
		}
		break ;
	
	case GDK_Right:
		if (!iv->priv->state.ascii_wrap && iv->priv->current_col<iv->priv->hscroll_upper) {
			iv->priv->current_col++ ;
			gc_iv_refresh_display(iv);
		}
		break ;
		
	case GDK_Home:
		iv->priv->current_col = 0 ;
		iv->priv->dispmode_scroll_to_offset(iv, 0);
		break ;
	
	case GDK_End:
		iv->priv->dispmode_scroll_to_offset(iv, iv->priv->s.st_size-1);
		break ;
		
	default:
		return FALSE;
	}
	
	return TRUE ;
}


/**************************************
* internal initialization functions
***************************************/
static void gc_iv_pixbuf_manipulation(GnomeCmdInternalViewer *iv, PIXBUF_MANIPULATION manip) 
{
	GdkPixbuf *temp = NULL ;
	
	if (!iv->priv->orig_pixbuf)
		return ;

	switch(manip)
	{
	case ROTATE_CLOCKWISE:
		temp = gdk_pixbuf_rotate_simple(iv->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE) ;
		break;
	case ROTATE_COUNTERCLOCKWISE:
		temp = gdk_pixbuf_rotate_simple(iv->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE) ;
		break;
	case ROTATE_UPSIDEDOWN:
		temp = gdk_pixbuf_rotate_simple(iv->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_UPSIDEDOWN) ;
		break;
	case FLIP_VERTICAL:
		temp = gdk_pixbuf_flip(iv->priv->orig_pixbuf, FALSE);
		break;
	case FLIP_HORIZONTAL:
		temp = gdk_pixbuf_flip(iv->priv->orig_pixbuf, TRUE);
		break;
	}
	gtk_image_set_from_pixbuf(GTK_IMAGE(iv->priv->imagedisp), temp) ;
	g_object_unref(iv->priv->orig_pixbuf) ;
	iv->priv->orig_pixbuf = temp ;
}

static void gc_iv_set_display_mode(GnomeCmdInternalViewer *iv, DISPLAYMODE newmode)
{
	iv->priv->dispmode = newmode;

	
	
	switch (newmode)
	{
	case DISP_FIXED:
		gc_iv_set_image_widget(iv,FALSE);
		iv->priv->current_font = iv->priv->state.fixed_font_name ;
	
		iv->priv->dispmode_scroll_to_offset = gc_iv_ascii_scroll_to_offset;
		iv->priv->dispmode_move_lines = gc_iv_ascii_move_lines;
		iv->priv->dispmode_get_text = gc_iv_ascii_get_text_line;
		gc_iv_set_wrap_mode(iv,iv->priv->state.ascii_wrap);
		break;

	case DISP_VARIABLE:
		gc_iv_set_image_widget(iv,FALSE);
		iv->priv->current_font = iv->priv->state.variable_font_name ;
	
		iv->priv->dispmode_scroll_to_offset = gc_iv_ascii_scroll_to_offset;
		iv->priv->dispmode_move_lines = gc_iv_ascii_move_lines;
		iv->priv->dispmode_get_text = gc_iv_ascii_get_text_line;
		gc_iv_set_wrap_mode(iv,iv->priv->state.ascii_wrap);
		break;

	case DISP_BINARY:
		gc_iv_set_image_widget(iv,FALSE);
		iv->priv->current_font = iv->priv->state.fixed_font_name ;

		iv->priv->dispmode_scroll_to_offset = gc_iv_binary_scroll_to_offset;
		iv->priv->dispmode_move_lines = gc_iv_binary_move_lines;
		iv->priv->dispmode_get_text = gc_iv_binary_get_text_line;
		
		/* this will make sure the display offset is a multiple of binmode_bytes_per_line */
		gc_iv_binary_scroll_to_offset(iv,iv->priv->logical_display_offset);
		gc_iv_set_wrap_mode(iv,FALSE);

		/* Binary Display always uses ASCII */
		gc_iv_set_input_mode(iv,"ASCII");
		break;
	
	case DISP_IMAGE:
		if (!iv->priv->imagedisp) {
			GError *err=NULL;
			iv->priv->imagedisp = gtk_image_new() ;
			/*iv->priv->orig_pixbuf = gdk_pixbuf_new_from_file(iv->priv->filename, &err) ;*/
			iv->priv->orig_pixbuf = gdk_pixbuf_new_from_file_at_scale(iv->priv->filename,
					iv->priv->xdisp_width, iv->priv->xdisp_height,TRUE,&err) ;
			gtk_image_set_from_pixbuf(GTK_IMAGE(iv->priv->imagedisp), iv->priv->orig_pixbuf) ;
			/* gc_iv_resize_image(iv); */
			/* see explanation for extra ref count at 'gc_iv_set_image_widget' */
			g_object_ref(iv->priv->imagedisp) ;
			gtk_widget_show(iv->priv->imagedisp);
		}
		if (iv->priv->sb_msg_ctx_id!=-1) {
			gtk_statusbar_pop(GTK_STATUSBAR(iv->priv->statusbar), iv->priv->sb_msg_ctx_id) ;
			iv->priv->sb_msg_ctx_id = -1 ;
		}	
		iv->priv->sb_msg_ctx_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(iv->priv->statusbar), "info") ;
		gtk_statusbar_push(GTK_STATUSBAR(iv->priv->statusbar), iv->priv->sb_msg_ctx_id, "\t\t\t\tDisplay: Image") ;
		
		gc_iv_set_image_widget(iv,TRUE);
		break;
	
	case DISP_HEX:
		gc_iv_set_image_widget(iv,FALSE);
		
		iv->priv->current_font = iv->priv->state.fixed_font_name ;

		iv->priv->dispmode_scroll_to_offset = gc_iv_hex_scroll_to_offset;
		iv->priv->dispmode_move_lines = gc_iv_hex_move_lines;
		iv->priv->dispmode_get_text = gc_iv_hex_get_text_line;
		
		/* this will make sure the display offset is a multiple of HEX_BYTES_PER_LINE */
		gc_iv_hex_scroll_to_offset(iv,iv->priv->logical_display_offset);

		gc_iv_set_wrap_mode(iv,FALSE);

		/* Hex Display always uses ASCII */
		gc_iv_set_input_mode(iv,"ASCII");
		break;
	
	}
	gc_iv_setup_font(iv);

	gc_iv_refresh_display(iv);
}

static void gc_iv_set_input_mode(GnomeCmdInternalViewer *iv, const gchar *from_charset)
{
	int i ;
	
	if (iv->priv->icnv && iv->priv->icnv!=(GIConv)-1)
		g_iconv_close(iv->priv->icnv) ;
	iv->priv->icnv = (GIConv)-1 ;
	
	/* First thing, set ASCII input mode, which will be the default if anything fails*/
	memset(iv->priv->codepage_translation_cache,0,sizeof(iv->priv->codepage_translation_cache));
	for (i=0;i<256;i++) {
		if (is_displayable(i))
			iv->priv->codepage_translation_cache[i] = i ;
		else
			iv->priv->codepage_translation_cache[i] = '.' ;
	}
	iv->priv->input_get_char = gc_iv_ascii_get_char ;
	iv->priv->input_get_next_char_offset = gc_iv_ascii_get_next_char_offset ;
	iv->priv->input_get_prev_char_offset = gc_iv_ascii_get_previous_char_offset ;
	
	g_return_if_fail(from_charset != NULL) ;

	DEBUG('v',"Setting charset conversions to \"%s\"\n", from_charset);
	g_strlcpy(iv->priv->state.charset, from_charset, 
		sizeof(iv->priv->state.charset));
	
	if (g_strcasecmp(from_charset,"ASCII")==0) {
		//we're already in ASCII (the default fallback)
		return ;
	}
	
	if (g_strcasecmp(from_charset,"CP437")==0) {
		for (i=0;i<256;i++) {
			unsigned int unicode = ascii_cp437_to_unicode[i] ;
			unicode2utf8(unicode, (unsigned char*)&iv->priv->codepage_translation_cache[i]) ;
		}
		return ;
	}
	
	if (g_strcasecmp(from_charset,"UTF8")==0) {
		iv->priv->input_get_char = gc_iv_utf8_get_char ;
		iv->priv->input_get_next_char_offset = gc_iv_utf8_get_next_char_offset ;
		iv->priv->input_get_prev_char_offset = gc_iv_utf8_get_previous_char_offset ;
		return ;
	}
	
	/* TODO: Add special handling for other input methods (like HTML,PS,TROFF etc) */
	
	
	
	/* If we got here, the user asked for ASCII input mode, with some special charset conversions.
	    Build the translation table for the current charset */
	iv->priv->icnv = g_iconv_open("UTF8",from_charset) ;
	g_strlcpy(iv->priv->state.charset, from_charset, sizeof(iv->priv->state.charset));
	if (iv->priv->icnv == (GIConv)-1) {
		DEBUG('v',"Failed to load charset conversions, disabling.\n");
		return ;
	}		
	for (i=0;i<256;i++) {
		gchar inbuf[2];
		unsigned char outbuf[5];
		
		gchar *ginbuf ;
		gchar *goutbuf ;
		gsize ginleft ;
		gsize goutleft ;
		size_t result ;
		
		inbuf[0] = i ;
		inbuf[1] = 0 ;
		
		memset(outbuf,0,sizeof(outbuf)) ;
		
		ginbuf = (gchar*)inbuf ;
		goutbuf = (gchar*)outbuf ;
		ginleft = 1;
		goutleft = sizeof(outbuf) ;
		
		result = g_iconv(iv->priv->icnv, 
				&ginbuf, &ginleft,
				&goutbuf, &goutleft ) ;
		if (result != 0) {
			DEBUG('v',"Charset %s: failed to convet character number %d\n", from_charset, i) ;
			iv->priv->codepage_translation_cache[i] = '.';
		}
		else {
			iv->priv->codepage_translation_cache[i] = 
				outbuf[0] + 
				(outbuf[1]<<8) + 
				(outbuf[2]<<16) + 
				(outbuf[3]<<24);
		}
	}
	return ;
}


static GtkWidget* gc_iv_create_menu_seperator(GtkWidget* container)
{
	GtkWidget *separatormenuitem1;

	separatormenuitem1 = gtk_separator_menu_item_new ();
	gtk_widget_show (separatormenuitem1);
	gtk_container_add (GTK_CONTAINER (container), separatormenuitem1);
	gtk_widget_set_sensitive (separatormenuitem1, FALSE);
	
	return separatormenuitem1;
}

static GtkWidget* gc_iv_create_menu_item (MENUITEMTYPE type,
					  const gchar* name,
					  GtkWidget* container,
					  GtkAccelGroup* accel,
					  guint keyval,
					  guint modifier,
					  GCallback callback,
					  gpointer userdata)
{
	GtkWidget *menuitem;
	
	switch(type)
	{
	case MI_CHECK:
		menuitem = gtk_check_menu_item_new_with_mnemonic (_(name));
		break;
	case MI_NORMAL:
	default:
		menuitem = gtk_menu_item_new_with_mnemonic (_(name));
		break;
	}
		
	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER(container), menuitem);
	
	gtk_widget_add_accelerator (menuitem, "activate", accel,
			      keyval, modifier, GTK_ACCEL_VISIBLE);  
	
	g_signal_connect ( G_OBJECT(menuitem), "activate", 
			callback, userdata);
	
	return menuitem;	
}

static GtkWidget* gc_iv_create_radio_menu_item (GSList **group,
					  const gchar* name,
					  GtkWidget* container,
					  GtkAccelGroup* accel,
					  guint keyval,
					  guint modifier,
					  GCallback callback,
					  gpointer userdata)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_radio_menu_item_new_with_mnemonic (*group,_(name));
	*group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
	
	if (accel && keyval) 
		gtk_widget_add_accelerator (menuitem, "activate", accel,
			      keyval, modifier, GTK_ACCEL_VISIBLE);  
	
	g_signal_connect ( G_OBJECT(menuitem), "activate", 
			callback, userdata);
	
	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER(container), menuitem);
	
	return menuitem;	
}

static GtkWidget* gc_iv_create_sub_menu(const gchar *name, GtkWidget* container)
{
	GtkWidget* menuitem4;
	GtkWidget* menu4 ;
	
	menuitem4 = gtk_menu_item_new_with_mnemonic (_(name));
	gtk_widget_show (menuitem4);
	gtk_container_add (GTK_CONTAINER (container), menuitem4);

	menu4 = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem4), menu4);
	
	return menu4;
}

typedef struct {
	const char *display_name;
	const char *charset_name;
	const char mnemonic ;
} CHARSET ;

CHARSET gc_iv_charsets[] = {
 { "Terminal (CP437)", 		"CP437", 'Q'},
 { "English (US-ASCII)", 	"ASCII", 'A'},
 { "Arabic (ISO-8859-6)",	"ISO-8859-6" },
 { "Arabic (Windows, CP1256)", 	"ARABIC" },
 { "Arabic (Dos, CP864)",	"CP864" },
 { "Baltic (ISO-8859-4)",	"ISO-8859-4" },
 { "Central European (ISO-8859-2)","ISO-8859-2" },
 { "Central European (CP1250)",	"CP1250" },
 { "Cyrillic (ISO-8859-5)",	"ISO-8859-5" },
 { "Cyrillic (CP1251)",		"CP1251" },
 { "Greek (ISO-8859-7)",	"ISO-8859-7" },
 { "Greek (CP1253)",		"CP1253" },
 { "Hebrew (Windows, CP1255)", 	"HEBREW" },
 { "Hebrew (Dos, CP862)",	"CP862" },
 { "Hebrew (ISO-8859-8)",	"ISO-8859-8" },
 { "Latin 9 (ISO-8859-15)",	"ISO-8859-15" },
 { "Maltese (ISO-8859-3)",	"ISO-8859-3" },
 { "Turkish (ISO-8859-9)",	"ISO-8859-9" },
 { "Turkish (CP1254)",		"CP1254" },
 { "Western (CP1252)",		"CP1252" },
 { "Western (ISO-8859-1)",	"ISO-8859-1" },
 { NULL, NULL} };
 
 
static GtkWidget* internal_viewer_create_menu(GnomeCmdInternalViewer *iv)
{
	GtkWidget *int_viewer_menu;
	GtkWidget *menuitem;
	GtkWidget *submenu;
	GtkWidget *submenu2;
	GSList	  *mode_radioitems_list = NULL;
	GSList	  *input_mode = NULL ;
	GSList	  *binmode_width = NULL;
	GSList	  *charset_list = NULL ;
	CHARSET   *charset ;
	
	int_viewer_menu = gtk_menu_bar_new ();

	iv->priv->accel_group = gtk_accel_group_new ();

	/* File Menu */
	submenu = gc_iv_create_sub_menu("_File", int_viewer_menu) ;
	gc_iv_create_menu_item( MI_NORMAL,"_Close", submenu,
				iv->priv->accel_group,GDK_Escape, 0,
				G_CALLBACK(gc_iv_menu_file_close), iv ) ;
		
	/* View Menu */
	submenu = gc_iv_create_sub_menu("_View", int_viewer_menu) ;
	menuitem = gc_iv_create_menu_item( MI_CHECK,"_Wrap lines", submenu,
				iv->priv->accel_group,GDK_W, 0,
				G_CALLBACK(gc_iv_menu_view_wrap), iv ) ;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), FALSE);

	gc_iv_create_menu_seperator(submenu) ;
	menuitem = gc_iv_create_radio_menu_item( &input_mode,"_ASCII (w/ encodings)", submenu,
				iv->priv->accel_group,GDK_A, 0,
				G_CALLBACK(gc_iv_menu_view_input_ascii), iv ) ;
	if (g_ascii_strcasecmp(iv->priv->state.charset,"ASCII")==0)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	iv->priv->ascii_menu_item = menuitem ;
	
	menuitem = gc_iv_create_radio_menu_item( &input_mode,"_UTF-8", submenu,
				iv->priv->accel_group,GDK_U, 0,
				G_CALLBACK(gc_iv_menu_view_input_utf8), iv ) ;
	if (g_ascii_strcasecmp(iv->priv->state.charset,"UTF8")==0)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	gc_iv_create_menu_seperator(submenu) ;

	/* Character set menu */
	submenu2 = gc_iv_create_sub_menu("Character Encodings", submenu);
	charset = gc_iv_charsets ;
	while (charset->display_name) {
		menuitem = gc_iv_create_radio_menu_item( &charset_list,charset->display_name, submenu2,
				charset->mnemonic?iv->priv->accel_group:NULL, 
				charset->mnemonic?charset->mnemonic:0, 
				charset->mnemonic?GDK_CONTROL_MASK:0,
				G_CALLBACK(gc_iv_menu_view_set_charset), iv ) ;
		g_object_set_data(G_OBJECT(menuitem),G_OBJ_CHARSET_KEY,(char*)charset->charset_name);
	
		if (g_ascii_strcasecmp(iv->priv->state.charset,(char*)charset->charset_name)==0)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
		
		charset++ ;
	}
	gc_iv_create_menu_seperator(submenu) ;
	menuitem = gc_iv_create_radio_menu_item( &mode_radioitems_list,"Text - _Fixed Width", submenu,
				iv->priv->accel_group,GDK_1, 0,
				G_CALLBACK(gc_iv_menu_view_mode_ascii), iv ) ;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),TRUE); 
	menuitem = gc_iv_create_radio_menu_item( &mode_radioitems_list,"Text - _Variable Width", submenu,
				iv->priv->accel_group,GDK_2, 0,
				G_CALLBACK(gc_iv_menu_view_mode_pango), iv ) ;
	menuitem = gc_iv_create_radio_menu_item( &mode_radioitems_list,"_Binary", submenu,
				iv->priv->accel_group,GDK_3, 0,
				G_CALLBACK(gc_iv_menu_view_mode_binary), iv ) ;
	menuitem = gc_iv_create_radio_menu_item( &mode_radioitems_list,"_Hex", submenu,
				iv->priv->accel_group,GDK_4, 0,
				G_CALLBACK(gc_iv_menu_view_mode_hex), iv ) ;
	menuitem = gc_iv_create_radio_menu_item( &mode_radioitems_list,"_Image", submenu,
				iv->priv->accel_group,GDK_5, 0,
				G_CALLBACK(gc_iv_menu_view_mode_image), iv ) ;
				

	gc_iv_create_menu_seperator(submenu) ;
	gc_iv_create_menu_item( MI_NORMAL,"_Larger Font", submenu,
				iv->priv->accel_group,GDK_equal, 0,
				G_CALLBACK(gc_iv_menu_view_larger_font), iv ) ;
	gc_iv_create_menu_item( MI_NORMAL,"_Smaller Font", submenu,
				iv->priv->accel_group,GDK_minus, 0,
				G_CALLBACK(gc_iv_menu_view_smaller_font), iv ) ;
				
	/* Image Menu */
	submenu = gc_iv_create_sub_menu("_Image", int_viewer_menu) ;
	gc_iv_create_menu_item( MI_NORMAL,"Rotate Clockwise", submenu,
				iv->priv->accel_group,GDK_C, GDK_CONTROL_MASK,
				G_CALLBACK(gc_iv_menu_image_rotate_cw), iv ) ;
	gc_iv_create_menu_item( MI_NORMAL,"Rotate Counter Clockwise", submenu,
				iv->priv->accel_group,GDK_C, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(gc_iv_menu_image_rotate_ccw), iv ) ;
	gc_iv_create_menu_item( MI_NORMAL,"Rotate Up-side-down", submenu,
				iv->priv->accel_group,GDK_U, GDK_CONTROL_MASK,
				G_CALLBACK(gc_iv_menu_image_rotate_usd), iv ) ;
	gc_iv_create_menu_item( MI_NORMAL,"Flip Vertically", submenu,
				iv->priv->accel_group,GDK_V, GDK_CONTROL_MASK,
				G_CALLBACK(gc_iv_menu_image_flip_vert), iv ) ;
	gc_iv_create_menu_item( MI_NORMAL,"Flip Horizontally", submenu,
				iv->priv->accel_group,GDK_H, GDK_CONTROL_MASK,
				G_CALLBACK(gc_iv_menu_image_flip_horz), iv ) ;

	/* Settings Menu */
	submenu = gc_iv_create_sub_menu("_Settings", int_viewer_menu) ;

	submenu2 = gc_iv_create_sub_menu("_Binary Mode", submenu) ;
	menuitem = gc_iv_create_radio_menu_item( &binmode_width,"_20 chars/line", submenu2,
				iv->priv->accel_group,GDK_2, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(gc_iv_menu_view_mode_bin_20_chars), iv ) ;
	if (iv->priv->state.binary_bytes_per_line==20)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),TRUE);
	
	menuitem = gc_iv_create_radio_menu_item( &binmode_width,"_40 chars/line", submenu2,
				iv->priv->accel_group,GDK_4, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(gc_iv_menu_view_mode_bin_40_chars), iv ) ;
	if (iv->priv->state.binary_bytes_per_line==40)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),TRUE);
		
	menuitem = gc_iv_create_radio_menu_item( &binmode_width,"_80 chars/line", submenu2,
				iv->priv->accel_group,GDK_8, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(gc_iv_menu_view_mode_bin_80_chars), iv ) ;
	if (iv->priv->state.binary_bytes_per_line==80)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),TRUE);

	submenu2 = gc_iv_create_sub_menu("_Hex Mode", submenu) ;
	menuitem = gc_iv_create_menu_item( MI_CHECK,"Decimal Offset Display", submenu2,
				iv->priv->accel_group,GDK_D, GDK_CONTROL_MASK,
				G_CALLBACK(gc_iv_menu_settings_hex_decimal_offset), iv ) ;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), 
		iv->priv->state.hex_decimal_offset);
	
	gc_iv_create_menu_seperator(submenu) ;
	gc_iv_create_menu_item( MI_NORMAL,"Save as Default Settings", submenu,
				iv->priv->accel_group,GDK_S, GDK_CONTROL_MASK,
				G_CALLBACK(gc_iv_menu_settings_save_settings), iv ) ;
	

	/* Help Menu */
	submenu = gc_iv_create_sub_menu("_Help", int_viewer_menu) ;
	gc_iv_create_menu_item( MI_NORMAL,"_Quick Help", submenu,
				iv->priv->accel_group,GDK_F1, 0,
				G_CALLBACK(gc_iv_menu_help_quick_help), iv ) ;
	gc_iv_create_menu_seperator(submenu) ;
	gc_iv_create_menu_item( MI_NORMAL,"_About", submenu,
				iv->priv->accel_group,GDK_F1, GDK_CONTROL_MASK,
				G_CALLBACK(gc_iv_menu_help_about), iv ) ;

	gtk_window_add_accel_group (GTK_WINDOW(iv), iv->priv->accel_group);
	return int_viewer_menu;
}



static void gc_iv_init_data(GnomeCmdInternalViewer *iv)
{
	/* Load iv->priv->state variables */
	load_settings(&iv->priv->state);

	iv->priv->sb_msg_ctx_id = -1 ;
	iv->priv->state.tab_size = DEFAULT_TAB_SIZE;
	
	/* Set up the state */
	iv->priv->block_ptr = 0;
	iv->priv->data = NULL;
	iv->priv->growing_buffer = 0;
	iv->priv->mmapping = 0;
	iv->priv->blocks = 0;
	iv->priv->block_ptr = 0;
	iv->priv->file = -1;
	iv->priv->first = iv->priv->bytes_read = 0;
	iv->priv->last_byte = 0;
	iv->priv->filename = NULL;
	iv->priv->last = iv->priv->first + ((LINES - 2) * iv->priv->bytes_per_line);
	
	iv->priv->line_buffer = NULL ;
	iv->priv->line_work_buffer = NULL ;
	iv->priv->line_buffer_alloc_len = 0 ;
	iv->priv->line_buffer_content_len = 0 ;
	
	iv->priv->dispmode = DISP_FIXED ;
	iv->priv->logical_display_offset = 0 ;

	iv->priv->current_col = 0 ;
	iv->priv->image_displayed = FALSE ;

	iv->priv->foreground_color.red = 0 ;
	iv->priv->foreground_color.blue = 0 ;
	iv->priv->foreground_color.green = 0 ;

	iv->priv->background_color.red = 65535 ;
	iv->priv->background_color.blue = 65535;
	iv->priv->background_color.green = 65535;
	
	gc_iv_set_input_mode(iv,iv->priv->state.charset);

	/* we start in ASCII display mode */
	iv->priv->current_font = iv->priv->state.fixed_font_name ;
	iv->priv->dispmode_scroll_to_offset = gc_iv_ascii_scroll_to_offset;
	iv->priv->dispmode_move_lines = gc_iv_ascii_move_lines;
	iv->priv->dispmode_get_text = gc_iv_ascii_get_text_line;
	
	/* we start in ASCII input mode */
	iv->priv->input_get_char = gc_iv_ascii_get_char ;
	iv->priv->input_get_next_char_offset = gc_iv_ascii_get_next_char_offset ;
	iv->priv->input_get_prev_char_offset = gc_iv_ascii_get_previous_char_offset ;
}

static void gc_iv_recalc_display(GnomeCmdInternalViewer* iv)
{
	GdkRectangle rect;
	int width ;
	int height ;

	if (gc_iv_gtk_ready(iv)) {
		//gdk_drawable_get_size(GDK_DRAWABLE(iv->priv->xdisp->window), &width, &height);
		width = GTK_WIDGET(iv->priv->xdisp)->allocation.width ;
		height = GTK_WIDGET(iv->priv->xdisp)->allocation.height ;
	} else {
		width = 1 ;
		height = 1 ;
	}
	
	iv->priv->xdisp_width = width ;
	iv->priv->xdisp_height = height ;
	
	if (iv->priv->char_width)
		iv->priv->bytes_per_line = width / iv->priv->char_width ;
	else
		iv->priv->bytes_per_line = 80 ;
		
	if (iv->priv->char_height)
		iv->priv->lines_displayed = height / iv->priv->char_height;
	else
		iv->priv->lines_displayed = 25 ;
		
	DEBUG('v', "reclac_display: width=%d, char_width=%d, bytes/line=%d\n", 
		width, iv->priv->char_width,iv->priv->bytes_per_line) ;
	DEBUG('v', "reclac_display: height=%d, char_height=%d, lines displayed=%d\n", 
		height, iv->priv->char_height,iv->priv->lines_displayed) ;

	rect.x = 0 ;
	rect.y = 0 ;
	rect.width = width;
	rect.height = height ;
	
	gc_iv_refresh_display(iv);
}

static void
gc_iv_setup_font(GnomeCmdInternalViewer *iv)
{
	gchar *font ;
	
	font = g_strdup_printf("%s %d", iv->priv->current_font, iv->priv->state.font_size);
	DEBUG('v',"Setup_font(%s)\n", font) ;
	
	if (iv->priv->disp_font_metrics)
		pango_font_metrics_unref(iv->priv->disp_font_metrics);
	
	iv->priv->disp_font_metrics = load_font (font);
	
	if (iv->priv->font_desc)
		pango_font_description_free(iv->priv->font_desc);
		
	iv->priv->font_desc = pango_font_description_from_string (font);
	
	gtk_widget_modify_font (iv->priv->xdisp, iv->priv->font_desc);

	iv->priv->char_width = get_max_char_width(iv->priv->xdisp, iv->priv->font_desc, 
			iv->priv->disp_font_metrics);
	
	iv->priv->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (iv->priv->disp_font_metrics)) + 
			PANGO_PIXELS(pango_font_metrics_get_descent (iv->priv->disp_font_metrics)) + 2;
	
	gc_iv_recalc_display(iv);
	
	gc_iv_set_wrap_mode(iv,iv->priv->state.ascii_wrap);
	
	gc_iv_refresh_display(iv);
	
	g_free(font);
}

/* see "http://en.wikipedia.org/wiki/CP437" */
static unsigned int ascii_cp437_to_unicode[256] = {
0x2E, /* NULL will be shown as a dot */
0x263A,
0x263B,
0x2665,
0x2666,
0x2663,
0x2660,
0x2022,
0x25D8,
0x25CB,
0x25D9,
0x2642,
0x2640,
0x266A,
0x266B,
0x263C,
0x25BA,
0x25C4,
0x2195,
0x203C,
0xB6,
0xA7,
0x25AC,
0x21A8,
0x2191,
0x2193,
0x2192,
0x2190,
0x221F,
0x2194,
0x25B2,
0x25BC,
0x20,
0x21,
0x22,
0x23,
0x24,
0x25,
0x26,
0x27,
0x28,
0x29,
0x2A,
0x2B,
0x2C,
0x2D,
0x2E,
0x2F,
0x30,
0x31,
0x32,
0x33,
0x34,
0x35,
0x36,
0x37,
0x38,
0x39,
0x3A,
0x3B,
0x3C,
0x3D,
0x3E,
0x3F,
0x40,
0x41,
0x42,
0x43,
0x44,
0x45,
0x46,
0x47,
0x48,
0x49,
0x4A,
0x4B,
0x4C,
0x4D,
0x4E,
0x4F,
0x50,
0x51,
0x52,
0x53,
0x54,
0x55,
0x56,
0x57,
0x58,
0x59,
0x5A,
0x5B,
0x5C,
0x5D,
0x5E,
0x5F,
0x60,
0x61,
0x62,
0x63,
0x64,
0x65,
0x66,
0x67,
0x68,
0x69,
0x6A,
0x6B,
0x6C,
0x6D,
0x6E,
0x6F,
0x70,
0x71,
0x72,
0x73,
0x74,
0x75,
0x76,
0x77,
0x78,
0x79,
0x7A,
0x7B,
0x7C,
0x7D,
0x7E,
0x2302,
0xC7,
0xFC,
0xE9,
0xE2,
0xE4,
0xE0,
0xE5,
0xE7,
0xEA,
0xEB,
0xE8,
0xEF,
0xEE,
0xEC,
0xC4,
0xC5,
0xC9,
0xE6,
0xC6,
0xF4,
0xF6,
0xF2,
0xFB,
0xF9,
0xFF,
0xD6,
0xDC,
0xA2,
0xA3,
0xA5,
0x20A7,
0x192,
0xE1,
0xED,
0xF3,
0xFA,
0xF1,
0xD1,
0xAA,
0xBA,
0xBF,
0x2310,
0xAC,
0xBD,
0xBC,
0xA1,
0xAB,
0xBB,
0x2591,
0x2592,
0x2593,
0x2502,
0x2524,
0x2561,
0x2562,
0x2556,
0x2555,
0x2563,
0x2551,
0x2557,
0x255D,
0x255C,
0x255B,
0x2510,
0x2514,
0x2534,
0x252C,
0x251C,
0x2500,
0x253C,
0x255E,
0x255F,
0x255A,
0x2554,
0x2569,
0x2566,
0x2560,
0x2550,
0x256C,
0x2567,
0x2568,
0x2564,
0x2565,
0x2559,
0x2558,
0x2552,
0x2553,
0x256B,
0x256A,
0x2518,
0x250C,
0x2588,
0x2584,
0x258C,
0x2590,
0x2580,
0x3B1,
0xDF,
0x393,
0x3C0,
0x3A3,
0x3C3,
0xB5,
0x3C4,
0x3A6,
0x398,
0x3A9,
0x3B4,
0x221E,
0x3C6,
0x3B5,
0x2229,
0x2261,
0xB1,
0x2265,
0x2264,
0x2320,
0x2321,
0xF7,
0x2248,
0xB0,
0x2219,
0xB7,
0x221A,
0x207F,
0xB2,
0x25A0,
0xA0
};

static int unicode2utf8(unsigned int unicode, unsigned char*out)
{
	int bytes_needed = 0 ;
	if (unicode<0x80) {
		bytes_needed = 1 ;
		out[0] = (unsigned char)(unicode&0xFF) ;
	}
	else
	if (unicode<0x0800) {
		bytes_needed = 2 ;
		out[0] = (unsigned char)(unicode>>6 | 0xC0) ;
		out[1] = (unsigned char)((unicode&0x3F)| 0x80) ;
	}
	else
	if (unicode<0x10000) {
		bytes_needed = 3 ;
		out[0] = (unsigned char)((unicode>>12) | 0xE0) ;
		out[1] = (unsigned char)(((unicode>>6) & 0x3F) | 0x80) ;
		out[2] = (unsigned char)((unicode & 0x3F) | 0x80) ;
	}
	else {
		bytes_needed = 4 ;
		out[0] = (unsigned char)((unicode>>18) | 0xE0) ;
		out[1] = (unsigned char)(((unicode>>12) & 0x3F) | 0x80) ;
		out[2] = (unsigned char)(((unicode>>6) & 0x3F) | 0x80) ;
		out[3] = (unsigned char)((unicode & 0x3F) | 0x80) ;
	}
	
	return bytes_needed ;
}

/*
static void gc_iv_resize_image(GnomeCmdInternalViewer *iv) 
{
	GdkPixbuf *scaled_pixbuf = NULL ;
	
	if (iv->priv->dispmode != DISP_IMAGE)
		return ;
	DBG;
	scaled_pixbuf = gdk_pixbuf_scale_simple(iv->priv->orig_pixbuf,
			iv->priv->xdisp_width, iv->priv->xdisp_height,
			GDK_INTERP_BILINEAR)  ;
	
	gtk_image_set_from_pixbuf(GTK_IMAGE(iv->priv->imagedisp), scaled_pixbuf) ;
}
*/

static void gc_iv_set_auto_display_mode(GnomeCmdInternalViewer *iv)
{
	#define DETECTION_BUF_LEN 100
	int i ;
	int count ;
	unsigned char temp[DETECTION_BUF_LEN] ;
	
	count = DETECTION_BUF_LEN ;
	if (iv->priv->s.st_size < count)
		count = iv->priv->s.st_size ;
	for (i=0;i<count;i++)
		temp[i] = gc_iv_file_get_byte(iv,i) ;
	
	switch(guess_display_mode(temp,count))
	{
	case DISP_IMAGE:
		DEBUG('v',"guessed display mode: Image\n") ;
		gc_iv_menu_view_mode_image(NULL,iv);
		break ;
	case DISP_FIXED:
		DEBUG('v',"guessed display mode: FIXED\n") ;
		gc_iv_menu_view_mode_ascii(NULL,iv);
		break ;
	case DISP_BINARY:
		DEBUG('v',"guessed display mode: Binary\n") ;
		gc_iv_menu_view_mode_binary(NULL,iv);
		break ;
	case DISP_HEX:
		DEBUG('v',"guessed display mode: HEX\n") ;
		gc_iv_menu_view_mode_hex(NULL,iv);
		break ;
	default:
		g_warning("display mode guess failed\n");			
	}
}

static void gc_iv_set_image_widget(GnomeCmdInternalViewer *iv, gboolean showimage)
{
	/* note:
	  when removing a widget from a container, it might be destroyed if its ref count is ZERO.
	  see documentation for "gtk_container_remove()".
	  to avoid this, an extra ref count is added to 'xdisp' and 'imagedisp' when they are created.
	  
	 the extra ref count is decremented at the 'destroy' function
	*/

	if (iv->priv->image_displayed==showimage)
		return ;

	if (showimage) {
		g_return_if_fail(iv->priv->imagedisp!=NULL) ;
		
		gtk_container_remove(GTK_CONTAINER(iv->priv->table1), iv->priv->xdisp);
		
		gtk_table_attach (GTK_TABLE (iv->priv->table1),iv->priv->imagedisp , 0, 1, 0, 1,
			    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	} else {
		gtk_container_remove(GTK_CONTAINER(iv->priv->table1), iv->priv->imagedisp);
		
		gtk_table_attach (GTK_TABLE (iv->priv->table1),iv->priv->xdisp , 0, 1, 0, 1,
			    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	}
	iv->priv->image_displayed = showimage ;
}


static void gc_iv_set_wrap_mode(GnomeCmdInternalViewer *iv, gboolean wrap) 
{
	iv->priv->state.ascii_wrap = wrap ;
	int page = iv->priv->s.st_size / 100 ;
	
	if (!iv->priv->hadj)
		return ;
	if (!iv->priv->vadj)
		return ;
	
	if (page==0)
		page = 5 ;
	
	if (iv->priv->dispmode==DISP_BINARY)
		wrap = FALSE ;
	
	if (wrap) {
		iv->priv->current_col = 0 ;
	
		/* Horz Scrollbar is disabled in WRAP mode */
		iv->priv->hadj->lower = 0 ;
		iv->priv->hadj->upper = 1 ;
		iv->priv->hadj->step_increment = 0;
		iv->priv->hadj->page_increment = 0 ;
		iv->priv->hadj->page_size = 1 ;
				
		iv->priv->vadj->lower = 0 ;
		iv->priv->vadj->upper = iv->priv->s.st_size ;
		iv->priv->vadj->step_increment = 1 ;
		iv->priv->vadj->page_increment = page ;
		iv->priv->vadj->page_size = page;
		
	} else {
		iv->priv->hadj->lower = 0 ;
		iv->priv->hadj->upper = iv->priv->hscroll_upper ;
		iv->priv->hadj->step_increment = 1 ;
		iv->priv->hadj->page_increment = 5 ;
		iv->priv->hadj->page_size = iv->priv->bytes_per_line  ;
				
		iv->priv->vadj->lower = 0 ;
		iv->priv->vadj->upper = iv->priv->s.st_size ;
		iv->priv->vadj->step_increment = 1 ;
		iv->priv->vadj->page_increment = page ;
		iv->priv->vadj->page_size = page;
	}
	
	gtk_adjustment_changed(iv->priv->hadj);
	gtk_adjustment_changed(iv->priv->vadj);
	gc_iv_refresh_display(iv);
}


/**********************************************
 File Handling Functions
**********************************************/
#define mc_close close
#define mc_munmap munmap
#define mc_mmap mmap
#define mc_read read
#define mc_lseek lseek
#define mc_open open
#define mc_fstat fstat


static int gc_iv_file_open(GnomeCmdInternalViewer *iv, const gchar* _file)
{
	char *error = NULL;
	char tmp[BUF_MEDIUM];
	int fd;
	
	DEBUG('v',"file_open(file=%s)\n",_file);
	
	g_free(iv->priv->filename);
	iv->priv->filename = g_strdup(_file);

	gtk_window_set_title(GTK_WINDOW(iv),_file);
	

	if (_file[0]) {
		int cntlflags;
		
		/* Open the file */
		if ((fd = mc_open (_file, O_RDONLY | O_NONBLOCK)) == -1) {
			g_snprintf (tmp, sizeof (tmp), _(" Cannot open \"%s\"\n %s "),
				_file, unix_error_string (errno));
			error = tmp;
			goto finish;
		}
		
		/* Make sure we are working with a regular file */
		if (mc_fstat (fd, &iv->priv->s) == -1) {
			mc_close (fd);
			g_snprintf (tmp, sizeof (tmp), _(" Cannot stat \"%s\"\n %s "),
				_file, unix_error_string (errno));
			error = tmp;
			goto finish;
		}
		
		if (!S_ISREG (iv->priv->s.st_mode)) {
			mc_close (fd);
			g_snprintf (tmp, sizeof (tmp),
				_(" Cannot view: not a regular file "));
			error = tmp;
			goto finish;
		}

		/* We don't need O_NONBLOCK after opening the file, unset it */
		cntlflags = fcntl (fd, F_GETFL, 0);
		if (cntlflags != -1) {
			cntlflags &= ~O_NONBLOCK;
			fcntl (fd, F_SETFL, cntlflags);
		}

		error = gc_iv_file_load(iv, fd);
	}

finish:
	if (error) {
		/* TODO: display a message to the user */
		DEBUG('v',"file_open failed: %s\n",error);
		return -1;
	}

	iv->priv->last_byte = iv->priv->first + iv->priv->s.st_size;

	DEBUG('v',"file_open succeeded\n");
	return 0;
}

static void
gc_iv_file_close (GnomeCmdInternalViewer *iv)
{
	g_return_if_fail (GNOME_CMD_IS_INTERNAL_VIEWER(iv));
	
	DEBUG('v',"file_close(fd = %d)\n",iv->priv->file);
	
	if (iv->priv->file != -1) {
		mc_close (iv->priv->file);
		iv->priv->file = -1;
	}
}

/* return values: NULL for success, else points to error message */
static char *
gc_iv_file_init_growing_view (GnomeCmdInternalViewer *iv, const char *name, const char *filename)
{
	DEBUG('v',"init_growing_buffer on %s\n",filename);

	iv->priv->growing_buffer = 1;

	if ((iv->priv->file = mc_open (filename, O_RDONLY)) == -1)
		return "init_growing_view: cannot open file";
	
	return NULL;
}

/* Load filename into core */
/* returns:
   -1 on failure.
   if (have_frame), we return success, but data points to a
   error message instead of the file buffer (quick_view feature).
*/
static char *
gc_iv_file_load(GnomeCmdInternalViewer *iv, int fd)
{
	DEBUG('v',"file_load(fd = %d)\n",fd);

	iv->priv->file = fd;

	if (iv->priv->s.st_size == 0) {
		/* Must be one of those nice files that grow (/proc) */
		gc_iv_file_close (iv);
		return gc_iv_file_init_growing_view (iv, 0, iv->priv->filename);
	}
#ifdef HAVE_MMAP
	if ((size_t) iv->priv->s.st_size == iv->priv->s.st_size)
		iv->priv->data = mc_mmap (0, iv->priv->s.st_size, PROT_READ,
			      MAP_FILE | MAP_SHARED, iv->priv->file, 0);
	else
		iv->priv->data = MAP_FAILED;
	
	if (iv->priv->data != MAP_FAILED) {
		DEBUG('v',"file_load, using mmap\n");
		/* mmap worked */
		iv->priv->first = 0;
		iv->priv->bytes_read = iv->priv->s.st_size;
		iv->priv->mmapping = 1;
		return NULL;
	}
	
	DEBUG('v',"file_load, mmap failed\n");
#endif				/* HAVE_MMAP */

	/* For the OSes that don't provide mmap call, try to load all the
	* file into memory (alex@bcs.zaporizhzhe.ua).  Also, mmap can fail
	* for any reason, so we use this as fallback (pavel@ucw.cz) */

	/* Make sure view->s.st_size is not truncated when passed to g_malloc */
	if ((gulong) iv->priv->s.st_size == iv->priv->s.st_size)
		iv->priv->data = (unsigned char *) g_try_malloc ((gulong) iv->priv->s.st_size);
	else
		iv->priv->data = NULL;

	if (iv->priv->data == NULL || 
		mc_lseek (iv->priv->file, 0, SEEK_SET) != 0 ||
		mc_read (iv->priv->file, (char*)iv->priv->data,
		    iv->priv->s.st_size) != iv->priv->s.st_size) {
		DEBUG('v',"file_load, using growing_buffer\n");
		g_free (iv->priv->data);
		gc_iv_file_close (iv);
		return gc_iv_file_init_growing_view (iv, 0, iv->priv->filename);
	}
	
	DEBUG('v',"file_load, whole file loaded into memory\n");
	iv->priv->first = 0;
	iv->priv->bytes_read = iv->priv->s.st_size;
	return NULL;
}

static int
gc_iv_file_get_byte (GnomeCmdInternalViewer *iv, unsigned int byte_index)
{
    int page = byte_index / VIEW_PAGE_SIZE + 1;
    int offset = byte_index % VIEW_PAGE_SIZE;
    int i, n;

    if (iv->priv->growing_buffer) {
	if (page > iv->priv->blocks) {
	    iv->priv->block_ptr = g_realloc (iv->priv->block_ptr,
					 page * sizeof (char *));
	    for (i = iv->priv->blocks; i < page; i++) {
		char *p = g_try_malloc (VIEW_PAGE_SIZE);
		iv->priv->block_ptr[i] = p;
		if (!p)
		    return '\n';
		n = mc_read (iv->priv->file, p, VIEW_PAGE_SIZE);
/*
 * FIXME: Errors are ignored at this point
 * Also should report preliminary EOF
 */
		if (n != -1)
		    iv->priv->bytes_read += n;
		if (iv->priv->s.st_size < iv->priv->bytes_read) {
		    iv->priv->bottom_first = INVALID_OFFSET; /* Invalidate cache */
		    iv->priv->s.st_size = iv->priv->bytes_read;
		    iv->priv->last_byte = iv->priv->bytes_read;
		}
	    }
	    iv->priv->blocks = page;
	}
	if (byte_index >= iv->priv->bytes_read) {
//	    DEBUG('v',"get_byte(%d) overflow [last = %d]\n",byte_index,iv->priv->last_byte);
	    return -1;
	} else
	    return iv->priv->block_ptr[page - 1][offset];
    } else {
	if (byte_index >= iv->priv->last_byte) {
//	    DEBUG('v',"get_byte(%d) overflow [last = %d]\n",byte_index,iv->priv->last_byte);
	    return -1;
	} else
	    return iv->priv->data[byte_index];
    }
}


/* based on MC's view.c "free_file" */
static void
gc_iv_file_free(GnomeCmdInternalViewer *iv)
{
	int i;

	g_return_if_fail (GNOME_CMD_IS_INTERNAL_VIEWER(iv));
	DEBUG('v',"free_file\n");
#ifdef HAVE_MMAP
	if (iv->priv->mmapping) {
		mc_munmap (iv->priv->data, iv->priv->s.st_size);
		gc_iv_file_close (iv);
	} else
#endif				/* HAVE_MMAP */
	{
		gc_iv_file_close (iv);
	}
	/* Block_ptr may be zero if the file was a file with 0 bytes */
	if (iv->priv->growing_buffer && iv->priv->block_ptr) {
		for (i = 0; i < iv->priv->blocks; i++) {
		    g_free (iv->priv->block_ptr[i]);
		}
		g_free (iv->priv->block_ptr);
	}
}



/************************************************* 
 Utility Functions
*************************************************/
static DISPLAYMODE guess_display_mode(const unsigned char* data, int len)
{
	int i ;
	gboolean control_chars = FALSE ; /* True if found ASCII < 0x20 */
	gboolean ascii_chars = FALSE ;
	gboolean extended_chars = FALSE; /* True if found ASCII >= 0x80 */
	const char* mime=NULL;
	
	mime = gnome_vfs_get_mime_type_for_data(data,len) ;
	
	DEBUG('v',"mime type = \"%s\"\n", mime ) ;

	if (g_strncasecmp(mime,"image/",6)==0)
		return DISP_IMAGE ;
	
	/* Hex File ?  */
	for (i=0;i<len;i++) {
		if (data[i]<0x20 && data[i]!=10 && data[i]!=13 && data[i]!=9)
			control_chars = TRUE ;
		if (data[i]>=0x80)
			extended_chars = TRUE ;
		if (data[i]>=0x20 && data[i]<=0x7F)
			ascii_chars = TRUE ;
		/* TODO: add UTF-8 detection */
	}

	if (control_chars)
		return DISP_BINARY ;
	
	return DISP_FIXED ;	
}

static guint get_max_char_width(GtkWidget *widget, PangoFontDescription *font_desc, PangoFontMetrics *font_metrics) {
	/* this is, I guess, a rather dirty trick, but
	   right now i can't think of anything better */
	guint i;
	guint maxwidth = 0;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	gchar str[2]; 

	layout = gtk_widget_create_pango_layout (widget, "");
	pango_layout_set_font_description (layout, font_desc);

	for(i = 1; i < 0x100; i++) {
		logical_rect.width = 0;
		/* Check if the char is displayable. Caused trouble to pango */
		if (is_displayable((guchar)i)) {
			sprintf (str, "%c", (gchar)i);
			pango_layout_set_text(layout, str, -1);
			pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
		}
		maxwidth = MAX(maxwidth, logical_rect.width);
	}

	g_object_unref (G_OBJECT (layout));
	return maxwidth;
}

static PangoFontMetrics* load_font (const char *font_name)
{
	PangoContext *context;
	PangoFont *new_font;
	PangoFontDescription *new_desc;
	PangoFontMetrics *new_metrics;

	new_desc = pango_font_description_from_string (font_name);

	context = gdk_pango_context_get();

	new_font = pango_context_load_font (context, new_desc);

	new_metrics = pango_font_get_metrics (new_font, pango_context_get_language (context));

	pango_font_description_free (new_desc);
	g_object_unref (G_OBJECT (context));
	g_object_unref (G_OBJECT (new_font));

	return new_metrics;
}

const char *unix_error_string (int error_num)
{
    static char buffer [BUF_LARGE];
#if GLIB_MAJOR_VERSION >= 2
    gchar *strerror_currentlocale;
	
    strerror_currentlocale = g_locale_from_utf8(g_strerror (error_num), -1, NULL, NULL, NULL);
    g_snprintf (buffer, sizeof (buffer), "%s (%d)",
		strerror_currentlocale, error_num);
    g_free(strerror_currentlocale);
#else
    g_snprintf (buffer, sizeof (buffer), "%s (%d)",
		g_strerror (error_num), error_num);
#endif
    return buffer;
}

static void load_settings(GnomeCmdStateVariables *state) 
{
	gchar *temp;
	
	g_return_if_fail(state!=NULL);
	
#ifdef HAVE_GNOME_CMD
	state->rect.width = gnome_cmd_data_get_int("internalviewer/width", 500 ) ;
	state->rect.height = gnome_cmd_data_get_int("internalviewer/height", 300 ) ;
	state->rect.x = gnome_cmd_data_get_int("internalviewer/x", 10 ) ;
	state->rect.y = gnome_cmd_data_get_int("internalviewer/y", 10 ) ;
	
	state->font_size = gnome_cmd_data_get_int("internalviewer/font_size",DEFAULT_FONT_SIZE);
	state->tab_size = gnome_cmd_data_get_int("internalviewer/tab_size",DEFAULT_TAB_SIZE);
	state->binary_bytes_per_line = gnome_cmd_data_get_int("internalviewer/bin_bytes_per_line",80);
	
	state->ascii_wrap = gnome_cmd_data_get_bool("internalviewer/ascii_wrap",TRUE);
	state->hex_decimal_offset= gnome_cmd_data_get_bool("internalviewer/hex_dec_offset",FALSE);
	
	temp = gnome_cmd_data_get_string("internalviewer/charset","CP437");
	g_strlcpy(state->charset,temp,sizeof(state->charset));
	g_free(temp);
	
	temp = gnome_cmd_data_get_string("internalviewer/fixed_font_name",DEFAULT_FIXED_FONT_NAME);
	g_strlcpy(state->fixed_font_name,temp,sizeof(state->fixed_font_name));
	g_free(temp);

	temp = gnome_cmd_data_get_string("internalviewer/variable_font_name",DEFAULT_VARIABLE_FONT_NAME);
	g_strlcpy(state->variable_font_name,temp,sizeof(state->variable_font_name));
	g_free(temp);

#else
	state->rect.width = 500; 
	state->rect.height = 500; 
	state->rect.x = 40 ;
	state->rect.y = 40 ;
	
	state->font_size = DEFAULT_FONT_SIZE;
	g_strlcpy(state->fixed_font_name,DEFAULT_FIXED_FONT_NAME,sizeof(state->fixed_font_name));
	g_strlcpy(state->fixed_font_name,DEFAULT_VARIABLE_FONT_NAME,sizeof(state->variable_font_name));
	
	state->tab_size = DEFAULT_TAB_SIZE;
	state->binary_bytes_per_line = 80; 
	state->ascii_wrap = FALSE ;
	state->hex_decimal_offset = TRUE ;
	
	g_strlcpy(state->charset,"CP437",sizeof(state->charset));
#endif

	DEBUG('v',"Load_settings: charset = \"%s\"\n", state->charset);
}

static void save_settings(GnomeCmdStateVariables *state) 
{
	g_return_if_fail(state!=NULL);
	
#ifdef HAVE_GNOME_CMD
	gnome_cmd_data_set_int("internalviewer/width", state->rect.width ) ;
	gnome_cmd_data_set_int("internalviewer/height", state->rect.height ) ;
	gnome_cmd_data_set_int("internalviewer/x", state->rect.x ) ;
	gnome_cmd_data_set_int("internalviewer/y", state->rect.y ) ;
	
	gnome_cmd_data_set_int("internalviewer/font_size",state->font_size );
	gnome_cmd_data_set_int("internalviewer/tab_size",state->tab_size );
	gnome_cmd_data_set_int("internalviewer/bin_bytes_per_line",state->binary_bytes_per_line );
	
	gnome_cmd_data_set_bool("internalviewer/ascii_wrap",state->ascii_wrap );
	gnome_cmd_data_set_bool("internalviewer/hex_dec_offset",state->hex_decimal_offset);
	
	gnome_cmd_data_set_string("internalviewer/charset",state->charset);
	
	gnome_cmd_data_set_string("internalviewer/fixed_font_name",state->fixed_font_name);
	gnome_cmd_data_set_string("internalviewer/variable_font_name",state->variable_font_name);
#else
#endif
}


static void gc_iv_grow_line_buffer(GnomeCmdInternalViewer *iv, int newsize) 
{
	if (iv->priv->line_buffer_alloc_len < newsize) {
		iv->priv->line_buffer_alloc_len = newsize * 2 ;
		
		iv->priv->hscroll_upper = iv->priv->line_buffer_alloc_len ;
		
		/* called to re-adjust the Hscroll bar acoording to 'hscroll_upper' */
		gc_iv_set_wrap_mode(iv, iv->priv->state.ascii_wrap); 
			
		iv->priv->line_buffer = (char_type*)g_realloc( iv->priv->line_buffer, 
			iv->priv->line_buffer_alloc_len* sizeof(char_type)) ;
		
		/* 4 time larger, because UTF-8 strings might have 4 bytes for each character */
		iv->priv->line_work_buffer = g_realloc( iv->priv->line_work_buffer, 
			iv->priv->line_buffer_alloc_len*4);
		
		iv->priv->line_buffer_content_len = 0 ;
		
		if (!iv->priv->line_buffer) 
			g_error("grow_line_buffer: memory allocation failed\n");
		if (!iv->priv->line_work_buffer) 
			g_error("grow_line_buffer: memory allocation failed\n");
	}
}

static void gc_iv_clear_line_buffer(GnomeCmdInternalViewer *iv) 
{
	if (iv->priv->line_buffer_alloc_len) {
		memset(iv->priv->line_buffer,0,iv->priv->line_buffer_alloc_len*sizeof(gchar)) ;
	}
	iv->priv->line_buffer_content_len = 0 ;
}


/***************************************************
  Generic Display Functions
****************************************************/
static gboolean gc_iv_gtk_ready(GnomeCmdInternalViewer* iv)
{
	if (!iv->priv->xdisp)
		return FALSE;

	if (!iv->priv->xdisp->window)
		return FALSE;
		
	if (!GTK_WIDGET_REALIZED(GTK_WIDGET(iv->priv->xdisp)))
		return FALSE;
		
	return TRUE;
}

static void gc_iv_refresh_display(GnomeCmdInternalViewer *iv) 
{
	int size;
	GdkRectangle rect;
	const gchar *mode_name;
	gchar position[50];
	
	rect.x = 0 ;
	rect.y = 0 ;
	
	if (!gc_iv_gtk_ready(iv))
		return ;

	if (iv->priv->xdisp->window) {
		gdk_drawable_get_size(GDK_DRAWABLE(iv->priv->xdisp->window), &rect.width, &rect.height);
		gdk_window_invalidate_rect(iv->priv->xdisp->window, &rect, FALSE);
	}
	
	gtk_adjustment_set_value(iv->priv->vadj, iv->priv->logical_display_offset);
	gtk_adjustment_set_value(iv->priv->hadj, iv->priv->current_col);
	
	if (iv->priv->sb_msg_ctx_id!=-1) {
		gtk_statusbar_pop(GTK_STATUSBAR(iv->priv->statusbar), iv->priv->sb_msg_ctx_id) ;
		iv->priv->sb_msg_ctx_id = -1 ;
	}	
	
	size = iv->priv->s.st_size ;
	if (size==0)
		size = 1 ;
	
	switch(iv->priv->dispmode)
	{
	case DISP_FIXED: mode_name = "Text-Fixed";break ;
	case DISP_BINARY: mode_name = "Binary";break ;
	case DISP_HEX: mode_name = "Hex";break ;
	case DISP_VARIABLE: mode_name = "Text-Variable";break ;
	case DISP_IMAGE: mode_name = "Image";break;
	default: mode_name = "" ; break ;
	}
	
	if (iv->priv->dispmode==DISP_HEX && !iv->priv->state.hex_decimal_offset) {
			g_snprintf(position,sizeof(position),
				"%08lx / %08lx", iv->priv->logical_display_offset, 
				iv->priv->s.st_size ) ;
	} else {
			g_snprintf(position,sizeof(position),
				"%lu / %lu", iv->priv->logical_display_offset, 
				iv->priv->s.st_size ) ;
	}
	snprintf(iv->priv->statusbar_msg,sizeof(iv->priv->statusbar_msg),
		"%d%%    %s    Display: %s    %s wrap",
		(int)((100*iv->priv->logical_display_offset)/size), 
		position,
		mode_name,
		iv->priv->state.ascii_wrap?"":"no " ) ;
	
	iv->priv->sb_msg_ctx_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(iv->priv->statusbar), "info") ;
	gtk_statusbar_push(GTK_STATUSBAR(iv->priv->statusbar), iv->priv->sb_msg_ctx_id, iv->priv->statusbar_msg) ;

}


/************************************************
* Ascii Mode functions
************************************************/

/*
      read a line of text from the file (starting at offset 'start'), into iv->priv->line_buffer.
      
      upon return:
	iv->priv->line_buffer is filled with the text (each element is IVCHAR, not gchar).
	iv->priv->line_buffer_content_len is the number of elements in 'line_buffer'.

	return value is the new offset, at the end of the line.

     ASCII mode special handling:
	TAB characters are converted into 'tab_size' spaces.
	Line stops at CR / LF / CR+LF charactes,
	  OR
	'bytes_per_line' characeters (if state->ascii_wrap is TRUE)
*/
static offset_type gc_iv_ascii_get_text_line(GnomeCmdInternalViewer *iv, offset_type start)
{
	offset_type offset ;
	char_type value ;
	int len = 0 ;
	int force_len = -1 ;
	
	gc_iv_clear_line_buffer(iv);
	
	offset = start ;
	
	while (1) {
		value = iv->priv->input_get_char(iv, offset);
		
		if (value==(char_type)-1)
			break ;
	
		/* Ignore LF after CR (window's style text files) */
		if (value=='\n' && (offset>1) &&
		    (iv->priv->input_get_char(iv, iv->priv->input_get_prev_char_offset(iv,offset))=='\r' ) ) {
			offset = iv->priv->input_get_next_char_offset(iv,offset);
		    	continue ;
		}
		
		offset = iv->priv->input_get_next_char_offset(iv,offset);
		
		/* break upon end of line */
		if (value=='\n' || value=='\r') {
			break ;
		}
			
		/* special handling for TAB in ASCII mode */
		if (value==9) {
			int i;
			int add ;
			
			add = iv->priv->state.tab_size - (len % iv->priv->state.tab_size);
			
			if (len + add >= iv->priv->line_buffer_alloc_len)
				gc_iv_grow_line_buffer(iv, len+add);
			
			for (i=0;i<add;i++)
				iv->priv->line_buffer[len+i] = ' ' ;
				
			len+= add;
		}
		else {
			if (len + 1 >= iv->priv->line_buffer_alloc_len)
				gc_iv_grow_line_buffer(iv, len+1);
			iv->priv->line_buffer[len] = value ;
			len++ ;
		}
	
		if (iv->priv->state.ascii_wrap && len>=iv->priv->bytes_per_line)
			break ;
		
		/* even when not in state->ascii_wrap, stop after we're reached the end of the screen */
		if (!iv->priv->state.ascii_wrap && force_len==-1) {
			if (len - iv->priv->current_col >= iv->priv->bytes_per_line)
				force_len = len;
		}
	}
	
	if (force_len !=-1)
		len = force_len;
	iv->priv->line_buffer_content_len = len ;
	
	return offset ;
}

/*
 scans the file from offset "start" backwards, until a LF,CR or CR+LF is found.
 returns the offset of the previous CR/LF, or 0 (if we've reached the start of the file)
*/
static offset_type gc_iv_find_previous_crlf(GnomeCmdInternalViewer *iv, offset_type start)
{
	int offset ;
	int value ;
	

	offset = start;
	
	while (1) {
		if (offset<=0)
			return 0;
			
		offset = iv->priv->input_get_prev_char_offset(iv,offset);
			
		value = iv->priv->input_get_char(iv, offset);
		if (value==(char_type)-1)
			break ;
			
		if (value=='\n' && (offset>0) &&
		    (gc_iv_ascii_get_char(iv,
		    	iv->priv->input_get_prev_char_offset(iv,offset))=='\r') )
		    continue ;
		
		/* break upon end of line */
		if (value=='\n' || value=='\r')
			break ;
	}
	
	return offset;
}

/*
	returns the start offset of the previous line, 
	with special handling for wrap mode.
*/
static offset_type gc_iv_find_previous_text_line(GnomeCmdInternalViewer *iv, offset_type start)
{
	offset_type offset ;
	

	gc_iv_clear_line_buffer(iv);
	
	offset = start ;
	
	/* step 1: 
		find TWO previous CR/LF = start offset of previous text line
	*/
	offset = gc_iv_find_previous_crlf(iv,offset);
	offset = gc_iv_find_previous_crlf(iv,offset);
	if (offset>0)
		offset++;
	
	/* 
	  step 2: 
	  in Wrap mode, a single text line might be broken into several displayable lines 
	  we want to find the previous displayable line.	
	*/
	if (!iv->priv->state.ascii_wrap)
		return offset;
	
	if (!offset)
		return offset;
		
	while (1) {
		offset_type next_line_offset; 
		
		next_line_offset = gc_iv_ascii_get_text_line(iv, offset) ;
		
		/*
			this is the line we want:
			When the next line's offset is the current offset ('start' parameter),
			'offset' will point to the previous displayable line
		*/
		if (next_line_offset>=start)
			return offset;
		
		offset = next_line_offset;
	}
	
	/* should never get here */
	return 0;	
}

static void gc_iv_ascii_move_lines(GnomeCmdInternalViewer *iv, int delta) 
{
	offset_type prev_offset;
	int count ;

	if (!delta)
		return ;
		
	count = abs(delta);
	
	while (count--)
		if (delta>0) {
			prev_offset = iv->priv->logical_display_offset;
			iv->priv->logical_display_offset = gc_iv_ascii_get_text_line(iv,iv->priv->logical_display_offset);
			
			if (prev_offset == iv->priv->logical_display_offset) 
				break; 
		}
		else  {
			prev_offset = iv->priv->logical_display_offset;
			iv->priv->logical_display_offset = gc_iv_find_previous_text_line(iv,
						iv->priv->logical_display_offset);
						
			if (prev_offset == iv->priv->logical_display_offset) 
				break; 
		}
		
	gc_iv_refresh_display(iv);
}

static void gc_iv_ascii_scroll_to_offset(GnomeCmdInternalViewer *iv, offset_type offset) 
{
	offset_type ofs ;
	
	ofs = gc_iv_find_previous_text_line(iv,offset);
	
	iv->priv->logical_display_offset = ofs ;
	
	gc_iv_refresh_display(iv);
	gc_iv_set_wrap_mode(iv,iv->priv->state.ascii_wrap);
}

static void gc_iv_ascii_draw_line(GnomeCmdInternalViewer *iv, int x, int y) 
{
	int count ;
	int in_index;
	int out_index;
	
	if (!iv->priv->line_buffer_content_len) 
		return ;
		
	count = iv->priv->line_buffer_content_len - iv->priv->current_col;

	if (count<=0)
		return ;

	/*outcount = gc_iv_convert_line_buffer_charset( iv,
		&iv->priv->line_buffer[iv->priv->current_col], count ,
		iv->priv->line_work_buffer, iv->priv->line_buffer_alloc_len*4);*/
	
	/* 
	  Covnert the ARRAY of "char_type" to a UTF-8 string.
	  
	  Each element in the "line_buffer" is already UTF-8, but it occupies the entire 32 bits
	  (With the unused bits zeroed-out).
	  
	  This loop will compact the array into a UTF-8 string (with only a terminating NULL)	  
	*/
	out_index = 0 ;
	for (in_index=0;in_index<count;in_index++) {
		char_type value = iv->priv->line_buffer[iv->priv->current_col+in_index] ;
		
		/* Always get the LSByte */
		iv->priv->line_work_buffer[out_index] = FIRST_BYTE(value) ;	
		out_index++ ;
		
		/* Get the other bytes, if this is a UTF-8 characeter */
		if (SECOND_BYTE(value)) {
			iv->priv->line_work_buffer[out_index] = SECOND_BYTE(value);
			out_index++ ;
			
			if (THIRD_BYTE(value)) {
				iv->priv->line_work_buffer[out_index] = THIRD_BYTE(value) ;
				out_index++ ;
				
				if (FOURTH_BYTE(value)) {
					iv->priv->line_work_buffer[out_index] = FOURTH_BYTE(value) ;
					out_index++ ;
				}
			}
		}
	}

	pango_layout_set_text (iv->priv->xlayout, (gchar*)iv->priv->line_work_buffer, out_index);

#if 0
	gdk_draw_layout_with_colors (iv->priv->xdisp->window, iv->priv->gc, 0, y, iv->priv->xlayout,
				&iv->priv->foreground_color,
				&iv->priv->background_color);
#else
	gdk_draw_layout(iv->priv->xdisp->window, iv->priv->gc, 0, y, iv->priv->xlayout);
#endif
}


static void gc_iv_ascii_draw_screen(GnomeCmdInternalViewer *iv)
{
	int x = 0 ;
	int y = 0 ;
	int row = 0 ;
	
	offset_type offset  = iv->priv->logical_display_offset;
	
		while (row<iv->priv->lines_displayed) {
		
		offset = iv->priv->dispmode_get_text(iv,offset);

		gc_iv_ascii_draw_line(iv,x - iv->priv->current_col*iv->priv->char_width,y);
		
		y += iv->priv->char_height ;
		row++;
	}
}

/**************************************************************
 Binary Mode Function
**************************************************************/
static offset_type gc_iv_binary_get_text_line(GnomeCmdInternalViewer *iv, offset_type start)
{
	offset_type offset ;
	char_type value ;
	int len = 0 ;
	
	gc_iv_clear_line_buffer(iv);
	
	offset = start ;
	
	while (1) {
		value = gc_iv_ascii_get_char(iv, offset);
		
		offset = gc_iv_ascii_get_next_char_offset(iv,offset) ;
		
		if (value==(char_type)-1)
			break ;
			
		if (len + 1 >= iv->priv->line_buffer_alloc_len)
			gc_iv_grow_line_buffer(iv, len+1);
		iv->priv->line_buffer[len] = value;
		len++ ;
	
		if (len>=iv->priv->state.binary_bytes_per_line)
			break ;
			
	}
	
	iv->priv->line_buffer_content_len = len ;
	return offset ;
}

static void gc_iv_binary_move_lines(GnomeCmdInternalViewer *iv, int delta)
{
	if (!delta)
		return ;
	
	if (delta<0) {
		if(iv->priv->logical_display_offset < 
		     abs(delta)*iv->priv->state.binary_bytes_per_line)
			iv->priv->logical_display_offset = 0 ;
		else
			iv->priv->logical_display_offset -= abs(delta)*iv->priv->state.binary_bytes_per_line;
	} else {
		if(iv->priv->logical_display_offset+delta*iv->priv->state.binary_bytes_per_line > 
			iv->priv->s.st_size )
			iv->priv->logical_display_offset = MAX(iv->priv->s.st_size-iv->priv->state.binary_bytes_per_line,0) ;
		else
			iv->priv->logical_display_offset += delta*iv->priv->state.binary_bytes_per_line;
	}
	gc_iv_refresh_display(iv);
}

static void gc_iv_binary_scroll_to_offset(GnomeCmdInternalViewer *iv, offset_type offset)
{
	iv->priv->logical_display_offset = ((guint)(offset/iv->priv->state.binary_bytes_per_line)) 
						* iv->priv->state.binary_bytes_per_line ;
	
	gc_iv_refresh_display(iv);
	gc_iv_set_wrap_mode(iv,iv->priv->state.ascii_wrap);
}

/**************************************************************
 Hex Mode Function
**************************************************************/
#define HEX_BYTES_PER_LINE 16

static offset_type gc_iv_hex_get_text_line(GnomeCmdInternalViewer *iv, offset_type start)
{
	offset_type offset ;
	char_type value ;
	int i ;
	int count = 0 ;
	unsigned char text[12];
	
	memset(text,' ',sizeof(text));
	
	gc_iv_clear_line_buffer(iv);
	
	offset = start ;
	
	/* Step 1: calculate number of bytes to display */
	while (1) {
		value = gc_iv_ascii_get_char(iv, offset+count);
		if (value==-1)
			break ;
		count++ ;
		if (count>=HEX_BYTES_PER_LINE)
			break ;
	}
	
	if (count==0) {
		iv->priv->line_buffer_content_len = 0 ;
		return offset ;
	}
	
	/* Step 2: Generate Hex dump */
	if (count*4+10+5 >= iv->priv->line_buffer_alloc_len)
			gc_iv_grow_line_buffer(iv, count*4+10+5);

	if (iv->priv->state.hex_decimal_offset)
		g_snprintf((char*)text,sizeof(text),"%09d ", (unsigned int)start);
	else
		g_snprintf((char*)text,sizeof(text),"%08x  ", (unsigned int)start);
		
	for (i=0;i<strlen((char*)text);i++)
		iv->priv->line_buffer[i] = text[i] ;	
	
	iv->priv->line_buffer[10+3*HEX_BYTES_PER_LINE] = ' ';
	iv->priv->line_buffer[10+3*HEX_BYTES_PER_LINE+1] = ' ';
	for (i=0;i<count;i++) {
		guint low,high;
		unsigned char byte_val ;
		char_type char_val; 
		
		/* byte_val = without charset translation, actuall byte as in the file */
		byte_val = gc_iv_file_get_byte(iv,offset+i);
		
		/* char_val = the byte_val after charset translation, in UTF-8 coding*/
		char_val = gc_iv_ascii_get_char(iv,offset+i);
		
		low = byte_val & 0x0F;
		high = (byte_val & 0xF0) >> 4;
	
		iv->priv->line_buffer[10 + 3*i] = ((high < 10)?(high + '0'):(high - 10 + 'A'));
		iv->priv->line_buffer[10 + 3*i+1] = ((low < 10)?(low + '0'):(low - 10 + 'A'));
		iv->priv->line_buffer[10 + 3*i+2] = ' ';
		iv->priv->line_buffer[10 + 3*HEX_BYTES_PER_LINE + 2 + i ] = char_val;
	}
	
	iv->priv->line_buffer_content_len = 10 + count*4+2;
	return offset+count ;
}

static void gc_iv_hex_move_lines(GnomeCmdInternalViewer *iv, int delta)
{
	if (!delta)
		return ;
	
	if (delta<0) {
		if(iv->priv->logical_display_offset < 
		     abs(delta)*HEX_BYTES_PER_LINE)
			iv->priv->logical_display_offset = 0 ;
		else
			iv->priv->logical_display_offset -= abs(delta)*HEX_BYTES_PER_LINE;
	} else {
		if(iv->priv->logical_display_offset+delta*HEX_BYTES_PER_LINE > 
			iv->priv->s.st_size )
			iv->priv->logical_display_offset = MAX(iv->priv->s.st_size-HEX_BYTES_PER_LINE,0) ;
		else
			iv->priv->logical_display_offset += delta*HEX_BYTES_PER_LINE;
	}
	gc_iv_refresh_display(iv);
}

static void gc_iv_hex_scroll_to_offset(GnomeCmdInternalViewer *iv, offset_type offset)
{
	iv->priv->logical_display_offset = ((guint)(offset/HEX_BYTES_PER_LINE)) 
						* HEX_BYTES_PER_LINE ;
	
	gc_iv_refresh_display(iv);
	gc_iv_set_wrap_mode(iv,iv->priv->state.ascii_wrap);
}


/*************************************************************************************************
 Data Input Methods
*************************************************************************************************/
static char_type gc_iv_ascii_get_char(GnomeCmdInternalViewer *iv, offset_type offset)
{
	int value ;
	
	value = gc_iv_file_get_byte(iv,offset);
	
	if (value<0)
		return (char_type)-1;
	
	if (value>255) {
		g_warning("Got BYTE>255 (%d) ?!\n", value);
		value = ' ' ;
	}
	
	/* ugly hack: in ascii,variant display modes, we don't translate CF,LF,TAB */
	if ((iv->priv->dispmode==DISP_FIXED ||
	    iv->priv->dispmode==DISP_VARIABLE) &&
	    (value=='\r' || value=='\n' || value=='\t') )
	    return (char_type)value;
		
	return (char_type)iv->priv->codepage_translation_cache[value] ;
}

static offset_type gc_iv_ascii_get_previous_char_offset(GnomeCmdInternalViewer *iv, offset_type offset)
{
	if (offset>0)
		return offset-1;
	
	return 0 ;
}

static offset_type gc_iv_ascii_get_next_char_offset(GnomeCmdInternalViewer *iv, offset_type offset)
{
	return offset+1;
}


#define UTF8_SINGLE_CHAR(c) (((c)&0x80)==0)
#define UTF8_HEADER_CHAR(c) (((c)&0xC0)==0xC0)
#define UTF8_HEADER_2BYTES(c) (((c)&0xE0)==0xC0)
#define UTF8_HEADER_3BYTES(c) (((c)&0xF0)==0xE0)
#define UTF8_HEADER_4BYTES(c) (((c)&0xF8)==0xF0)
#define UTF8_TRAILER_CHAR(c) (((c)&0xC0)==0x80)

static guint gc_iv_utf8_get_char_len(GnomeCmdInternalViewer *iv, offset_type offset)
{
	int value ;
	
	value = gc_iv_file_get_byte(iv,offset);
	if (value<0)
		return 0 ;
	if (value>255) 
		return 0;
	
	if (UTF8_SINGLE_CHAR(value))
		return 1;
	
	if (UTF8_HEADER_CHAR(value)) {
		if (UTF8_HEADER_2BYTES(value)) 
			return 2;
		if (UTF8_HEADER_3BYTES(value))
			return 3;
		if (UTF8_HEADER_4BYTES(value))
			return 4;
	}
	
	/* fall back: this is an invalid UTF8 characeter */
	return 0;
}

static gboolean gc_iv_utf8_is_valid_char(GnomeCmdInternalViewer *iv, offset_type offset)
{
	int len;
	len = gc_iv_utf8_get_char_len(iv,offset);
	if (len==0 || (offset+len)>iv->priv->s.st_size) {
		return FALSE ;
	}
	
	if (len==1)
		return TRUE;
	
	if (!UTF8_TRAILER_CHAR(gc_iv_file_get_byte(iv,offset+1))) {
		return FALSE;
	}
	
	if (len==2)
		return TRUE;

	if (!UTF8_TRAILER_CHAR(gc_iv_file_get_byte(iv,offset+2))) {
		return FALSE;
	}
	
	if (len==3)
		return TRUE;
	
	if (!UTF8_TRAILER_CHAR(gc_iv_file_get_byte(iv,offset+3))) {
		return FALSE;
	}
	
	if (len==4)
		return TRUE ;
		
	return FALSE;
}

static char_type gc_iv_utf8_get_char(GnomeCmdInternalViewer *iv, offset_type offset)
{
	int value ;
	int len;
	
	value = gc_iv_file_get_byte(iv,offset);
	if (value<0)
		return (char_type)-1 ;
		
	if (!gc_iv_utf8_is_valid_char(iv,offset)) {
		DEBUG('v',"invalid UTF characeter at offset %d (%02x)\n",offset, 
			(unsigned char )gc_iv_file_get_byte(iv,offset));
		return '.' ;
	}
		
	len = gc_iv_utf8_get_char_len(iv,offset);
	
	if (len==1) 
		return gc_iv_file_get_byte(iv,offset);
		
	if (len==2)
		return (char_type)gc_iv_file_get_byte(iv,offset) + 
		       (char_type)(gc_iv_file_get_byte(iv,offset+1)<<8);
	if (len==3)
		return (char_type)gc_iv_file_get_byte(iv,offset) + 
		       (char_type)(gc_iv_file_get_byte(iv,offset+1)<<8)+
		       (char_type)(gc_iv_file_get_byte(iv,offset+2)<<16);
	if (len==4)
		return (char_type)gc_iv_file_get_byte(iv,offset) + 
		       (char_type)(gc_iv_file_get_byte(iv,offset+1)<<8)+
		       (char_type)(gc_iv_file_get_byte(iv,offset+2)<<16)+
		       (char_type)(gc_iv_file_get_byte(iv,offset+3)<<24);
		       
	return -1;
}

static offset_type gc_iv_utf8_get_previous_char_offset(GnomeCmdInternalViewer *iv, offset_type offset)
{
	if (offset==0)
		return 0;
		
	if (offset>0 && gc_iv_utf8_is_valid_char(iv,offset-1))
		return offset-1 ;
	
	if (offset>1 && gc_iv_utf8_is_valid_char(iv,offset-2))
		return offset-2 ;
	
	if (offset>2 && gc_iv_utf8_is_valid_char(iv,offset-3))
		return offset-3 ;
		
	if (offset>3 && gc_iv_utf8_is_valid_char(iv,offset-4))
		return offset-4 ;

	return offset-1;
}

static offset_type gc_iv_utf8_get_next_char_offset(GnomeCmdInternalViewer *iv, offset_type offset)
{
	int len;

	if (!gc_iv_utf8_is_valid_char(iv,offset))
		return offset+1 ;
		
	len = gc_iv_utf8_get_char_len(iv,offset);
	if (len==0)
	 	len=1;
	
	return offset+len;
}
