/*
    LibGViewer - GTK+ File Viewer library 
    Copyright (C) 2006 Assaf Gordon
    
    Part of
        GNOME Commander - A GNOME based file manager
        Copyright (C) 2001-2006 Marcus Bjurman

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include <config.h>

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gtk/gtktable.h>

#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>

#include "libgviewer.h"

#define G_OBJ_CHARSET_KEY "charset"
#define G_OBJ_DISPMODE_KEY "dispmode"
#define G_OBJ_BYTES_PER_LINE_KEY "bytesperline"
#define G_OBJ_IMAGE_OP_KEY "imageop"
#define G_OBJ_EXTERNAL_TOOL_KEY "exttool"

#undef HAVE_HELP_ABOUT

#define D fprintf(stderr,"%s:%d - %s\n", __FILE__,__LINE__,__FUNCTION__);

/* EXTERNAL TOOLS DISABLED in coming stable release */
#undef EXTERNAL_TOOLS

static GtkWindowClass *parent_class = NULL ;

#define MAX_SCALE_FACTOR_INDEX 13
static double image_scale_factors[MAX_SCALE_FACTOR_INDEX] = {
	0.25,0.5,0.75,1,1.25,1.50,2,2.5,3,3.5,4,4.5,5};

#ifdef EXTERNAL_TOOLS
typedef struct _GViewerWindowExternalTool GViewerWindowExternalTool ;
struct _GViewerWindowExternalTool
{
	gchar *name;
	gchar *command ;
	int   attached_fd ;
};
#endif

struct _GViewerWindowPrivate
{
	/* Gtk User Interface */
	GtkWidget *vbox;
	GViewer *viewer;
	GtkWidget *menubar;
	GtkWidget *statusbar;
	
	GtkAccelGroup *accel_group;
	GtkWidget *ascii_menu_item;
	GtkWidget *wrap_mode_menu_item;
	GtkWidget *hex_offset_menu_item;
	GtkWidget *show_exif_menu_item;
	GtkWidget *fixed_limit_menu_items[3];
	GViewerWindowSettings state;

	GViewer *exif_viewer;
	int 	 exit_data_fd ;
	gboolean exif_active;
	
	GViewer	*active_viewer;
	
	int 	current_scale_index;

	gchar *filename;
	guint 	statusbar_ctx_id ;
	gboolean status_bar_msg;

#ifdef EXTERNAL_TOOLS
	GHashTable *external_tools;
	GViewerWindowExternalTool *active_external_tool;
#endif
};

static void gviewer_window_init(GViewerWindow *w);
static void gviewer_window_class_init (GViewerWindowClass *class);
static void gviewer_window_destroy(GtkObject *widget);

static void gviewer_window_status_line_changed(GViewer *obj, const gchar* status_line, GViewerWindow* wnd);

static gboolean gviewer_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

static GtkWidget* gviewer_window_create_menus(GViewerWindow *obj);

#ifdef EXTERNAL_TOOLS
static void gviewer_window_add_external_tool(GViewerWindow *obj, const gchar *name, const gchar* command );
static void gviewer_window_activate_external_tool(GViewerWindow *obj, const gchar *name);
static void gviewer_window_activate_internal_viewer(GViewerWindow *obj);
#endif

static void gviewer_window_show_exif_viewer(GViewerWindow *obj);
static void gviewer_window_hide_exif_viewer(GViewerWindow *obj);

/* Event Handlers */
static void menu_file_close (GtkMenuItem *item, GViewerWindow *obj) ;

#ifdef EXTERNAL_TOOLS
static void menu_view_external_tool(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_view_internal_viewer(GtkMenuItem *item, GViewerWindow *obj) ;
#endif

static void menu_view_exif_information(GtkMenuItem *item, GViewerWindow *obj) ;

static void menu_edit_copy(GtkMenuItem *item, GViewerWindow *obj) ;

static void menu_view_wrap(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_view_display_mode(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_view_set_charset(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_view_zoom_in(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_view_zoom_out(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_view_zoom_normal(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_view_zoom_best_fit(GtkMenuItem *item, GViewerWindow *obj) ;

static void menu_image_operation(GtkMenuItem *item, GViewerWindow *obj) ;

static void menu_settings_binary_bytes_per_line(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_settings_hex_decimal_offset(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_settings_save_settings(GtkMenuItem *item, GViewerWindow *obj) ;

#ifdef HAVE_HELP_ABOUT
static void menu_help_quick_help(GtkMenuItem *item, GViewerWindow *obj) ;
static void menu_help_about(GtkMenuItem *item, GViewerWindow *obj) ;
#endif


static void set_zoom_in(GViewerWindow *obj) ;
static void set_zoom_out(GViewerWindow *obj) ;
static void set_zoom_normal(GViewerWindow *obj) ;
static void set_zoom_best_fit(GViewerWindow *obj) ;


/*****************************************
	public functions
	(defined in the header file)
*****************************************/
GtkWidget* gviewer_window_file_view   (const gchar * filename,GViewerWindowSettings *initial_settings)
{
	GViewerWindowSettings set;
	
	GtkWidget *w ;
	
	if (initial_settings==NULL) {
		gviewer_window_load_settings(&set);
		initial_settings = &set ;
	}
	
	w = gviewer_window_new(initial_settings) ;
	
	gviewer_window_load_file(GVIEWER_WINDOW(w), filename);	
	
	if (initial_settings)
		gviewer_window_set_settings(GVIEWER_WINDOW(w),initial_settings);
	
	return w;
}

void gviewer_window_load_file	(GViewerWindow *obj, const gchar *filename)
{
	g_return_if_fail(obj);
	g_return_if_fail(filename);
	
	if (obj->priv->filename)
		g_free(obj->priv->filename);
	
	obj->priv->filename = g_strdup(filename);
	
	gviewer_load_file(obj->priv->viewer, filename);
	
	gtk_window_set_title(GTK_WINDOW(obj), obj->priv->filename);
}


GtkType gviewer_window_get_type (void)
{
	static GtkType type = 0;
	if (type == 0)	{
		GTypeInfo info =
		{
			sizeof (GViewerWindowClass),
			NULL,
			NULL,
			(GClassInitFunc) gviewer_window_class_init,
			NULL,
			NULL,
			sizeof(GViewerWindow),
			0,
			(GInstanceInitFunc) gviewer_window_init
		};
		type = g_type_register_static(GTK_TYPE_WINDOW, "gviewerwindow", &info,0);
	}
	return type;
}

GtkWidget*
gviewer_window_new (GViewerWindowSettings *initial_settings)
{
	GViewerWindow *w;
	
	w = gtk_type_new (gviewer_window_get_type ());
	
	return GTK_WIDGET (w);
}

static void gviewer_window_map (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
		GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
gviewer_window_class_init (GViewerWindowClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class ;

	object_class = GTK_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
		
	parent_class = gtk_type_class(gtk_window_get_type ());

	object_class->destroy = gviewer_window_destroy ;
	widget_class->map = gviewer_window_map ;
}

#ifdef EXTERNAL_TOOLS
static void gviewer_window_destroy_key(gchar *string)
{
	if (string)
		g_free(string);
}

static void gviewer_window_destroy_external_tool(GViewerWindowExternalTool *tool)
{
	if (!tool)
		return;

	g_free(tool->name);
	g_free(tool->command);
	if (tool->attached_fd!=-1)
		close(tool->attached_fd);
	
	g_free(tool);
}
#endif

static void
gviewer_window_init (GViewerWindow *w)
{
	GtkWindow *win ;
	
	
	w->priv = g_new0(GViewerWindowPrivate,1);
	
	w->priv->status_bar_msg = FALSE ;
	w->priv->filename = NULL ;
#ifdef EXTERNAL_TOOLS
	w->priv->active_external_tool = NULL ;
#endif
	w->priv->exit_data_fd = -1 ;
	w->priv->exif_active = FALSE ;
	w->priv->current_scale_index = 3;
	
#ifdef EXTERNAL_TOOLS
	w->priv->external_tools = g_hash_table_new_full(g_str_hash,g_str_equal,
					(GDestroyNotify)gviewer_window_destroy_key,
					(GDestroyNotify)gviewer_window_destroy_external_tool) ;
#endif
	
	win = GTK_WINDOW(w);
	gtk_window_set_title(win, "GViewer");

	g_signal_connect(G_OBJECT(w),"key_press_event", G_CALLBACK(gviewer_window_key_pressed),NULL);

	w->priv->vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (w->priv->vbox);

	w->priv->menubar = gviewer_window_create_menus(w);
  	gtk_widget_show (w->priv->menubar);
  	gtk_box_pack_start (GTK_BOX (w->priv->vbox), w->priv->menubar, FALSE, FALSE, 0);
	
	w->priv->viewer = (GViewer*)gviewer_new();
	g_object_ref(G_OBJECT(w->priv->viewer)) ;
	gtk_widget_show(GTK_WIDGET(w->priv->viewer)) ;
  	gtk_box_pack_start (GTK_BOX (w->priv->vbox), GTK_WIDGET(w->priv->viewer), TRUE, TRUE, 0);
	
	w->priv->exif_viewer = (GViewer*)gviewer_new();
	g_object_ref(G_OBJECT(w->priv->exif_viewer)) ;

	g_signal_connect(G_OBJECT(w->priv->viewer),"status_line_changed",
		G_CALLBACK(gviewer_window_status_line_changed), (gpointer)w) ;

	
	w->priv->statusbar = gtk_statusbar_new ();
  	gtk_widget_show (w->priv->statusbar);
  	gtk_box_pack_start (GTK_BOX (w->priv->vbox), w->priv->statusbar, FALSE, FALSE, 0);
	
	w->priv->statusbar_ctx_id  = gtk_statusbar_get_context_id(
		GTK_STATUSBAR(w->priv->statusbar), "info") ;
		
	
	gtk_widget_grab_focus(GTK_WIDGET(w->priv->viewer));
	
	gtk_container_add(GTK_CONTAINER(w), w->priv->vbox);
	
	w->priv->active_viewer = w->priv->viewer;

#ifdef EXTERNAL_TOOLS
	gviewer_window_add_external_tool(w,"html","html2text -nobs '%s'");
	gviewer_window_add_external_tool(w,"pspdf","ps2ascii '%s'");
#endif
}

static void gviewer_window_status_line_changed(GViewer *obj, const gchar* status_line, GViewerWindow* wnd)
{
	GViewerWindow *w;

	

	g_return_if_fail (wnd!= NULL);
	g_return_if_fail (IS_GVIEWER_WINDOW (wnd));
	
	w = GVIEWER_WINDOW (wnd);
	
	if (w->priv->status_bar_msg) {
		gtk_statusbar_pop(
			GTK_STATUSBAR(w->priv->statusbar), w->priv->statusbar_ctx_id ) ;
		w->priv->status_bar_msg = FALSE ;
	}

	if (status_line) {
		gtk_statusbar_push(
			GTK_STATUSBAR(w->priv->statusbar), w->priv->statusbar_ctx_id,
			status_line) ;
		w->priv->status_bar_msg = TRUE;
	}
}

void gviewer_window_set_settings(GViewerWindow *obj, /*in*/ GViewerWindowSettings *settings)
{
	g_return_if_fail(obj!=NULL);
	g_return_if_fail(IS_GVIEWER_WINDOW(obj));
	g_return_if_fail(settings!=NULL);
	g_return_if_fail(obj->priv->viewer!=NULL);
	
	

	gviewer_set_font_size(obj->priv->viewer,settings->font_size);
	gviewer_set_tab_size(obj->priv->viewer,settings->tab_size);
	
	gviewer_set_fixed_limit(obj->priv->viewer,settings->binary_bytes_per_line);
	switch(settings->binary_bytes_per_line)
	{
	case 20:
		gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(obj->priv->fixed_limit_menu_items[0]),TRUE);
		break;
	case 40:
		gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(obj->priv->fixed_limit_menu_items[1]),TRUE);
		break;
	case 80:
		gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(obj->priv->fixed_limit_menu_items[2]),TRUE);
		break;
	}

	gviewer_set_wrap_mode(obj->priv->viewer,settings->wrap_mode);
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(obj->priv->wrap_mode_menu_item),
			settings->wrap_mode);
	
	gviewer_set_hex_offset_display(obj->priv->viewer,settings->hex_decimal_offset);
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(obj->priv->hex_offset_menu_item),
			settings->hex_decimal_offset);

	gviewer_set_encoding(obj->priv->viewer,settings->charset);
	
	gtk_window_resize(GTK_WINDOW(obj),
		settings->rect.width, settings->rect.height);
	
#if 0
	/* This doesn't work because the window is not shown yet */
	if (GTK_WIDGET(obj)->window)
		gdk_window_move(GTK_WIDGET(obj)->window,
			settings->rect.x, settings->rect.y);
#endif
	gtk_window_set_position(GTK_WINDOW(obj),GTK_WIN_POS_CENTER);
}

void gviewer_window_get_current_settings(GViewerWindow *obj, /* out */ GViewerWindowSettings *settings)
{
	g_return_if_fail(obj!=NULL);
	g_return_if_fail(IS_GVIEWER_WINDOW(obj));
	g_return_if_fail(settings!=NULL);
	g_return_if_fail(obj->priv->viewer!=NULL);
	
	memset(settings,0,sizeof(GViewerWindowSettings));

	if (GTK_WIDGET(obj)->window) {
		settings->rect.width = GTK_WIDGET(obj)->allocation.width;
		settings->rect.height = GTK_WIDGET(obj)->allocation.height;
		gdk_window_get_position(GTK_WIDGET(obj)->window,
			&settings->rect.x,
			&settings->rect.y);
	} else {
		settings->rect.x = 0 ;
		settings->rect.y = 0 ;
		settings->rect.width = 100 ;
		settings->rect.height = 100 ;
	}
	settings->font_size = gviewer_get_font_size(obj->priv->viewer);
	settings->wrap_mode = gviewer_get_wrap_mode(obj->priv->viewer);
	settings->binary_bytes_per_line = gviewer_get_fixed_limit(obj->priv->viewer);
	strncpy(settings->charset, gviewer_get_encoding(obj->priv->viewer),sizeof(settings->charset));
	settings->hex_decimal_offset = gviewer_get_hex_offset_display(obj->priv->viewer);
	settings->tab_size = gviewer_get_tab_size(obj->priv->viewer);
}

static void
gviewer_window_destroy (GtkObject *widget)
{
	GViewerWindow *w;

	g_return_if_fail (widget!= NULL);
	g_return_if_fail (IS_GVIEWER_WINDOW (widget));
	
	w = GVIEWER_WINDOW (widget);

	if (w->priv) {
		g_object_unref(G_OBJECT(w->priv->viewer)) ;
		g_object_unref(G_OBJECT(w->priv->exif_viewer)) ;

#ifdef EXTERNAL_TOOLS
		g_hash_table_destroy(w->priv->external_tools);
#endif
		
		if (w->priv->filename)
			g_free(w->priv->filename);
		w->priv->filename = NULL ;
		
		if (w->priv->exit_data_fd!=-1)
			close(w->priv->exit_data_fd);
		w->priv->exit_data_fd = -1;

		g_free(w->priv);
		w->priv = NULL;
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(widget);
}

static gboolean gviewer_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GViewerWindow *w;
	
	g_return_val_if_fail (widget!= NULL,FALSE);
	g_return_val_if_fail (IS_GVIEWER_WINDOW (widget),FALSE);
	w = GVIEWER_WINDOW (widget);
	
	if (event->state & GDK_CONTROL_MASK) {
		switch (event->keyval)
		{
		case GDK_w:
		case GDK_W:
			gtk_widget_destroy(GTK_WIDGET(w));
			return TRUE ;
		}
	}
	
	switch (event->keyval) {
	case GDK_plus:
	case GDK_KP_Add:
	case GDK_equal:
	   set_zoom_in(w);
	   return TRUE;

	case GDK_minus:
	case GDK_KP_Subtract:
	   set_zoom_out(w);
	   return TRUE;
	}
	
	return FALSE;
}

static GtkWidget* create_menu_seperator(GtkWidget* container)
{
	GtkWidget *separatormenuitem1;

	separatormenuitem1 = gtk_separator_menu_item_new ();
	gtk_widget_show (separatormenuitem1);
	gtk_container_add (GTK_CONTAINER (container), separatormenuitem1);
	gtk_widget_set_sensitive (separatormenuitem1, FALSE);

	return separatormenuitem1;
}

typedef enum
{
	MI_NONE,
	MI_NORMAL,
	MI_CHECK,
	MI_RADIO,
	MI_SEPERATOR,
	MI_SUBMENU
} MENUITEMTYPE;

static GtkWidget *
create_ui_pixmap (GtkWidget *window,
				  GnomeUIPixmapType pixmap_type,
				  gconstpointer pixmap_info,
				  GtkIconSize size)
{
	GtkWidget *pixmap;
	char *name;

	pixmap = NULL;

	switch (pixmap_type) {
	case GNOME_APP_PIXMAP_STOCK:
		pixmap = gtk_image_new_from_stock (pixmap_info, size);
		break;

	case GNOME_APP_PIXMAP_DATA:
		if (pixmap_info)
			pixmap = gnome_pixmap_new_from_xpm_d ((const char**)pixmap_info);

		break;

	case GNOME_APP_PIXMAP_NONE:
		break;

	case GNOME_APP_PIXMAP_FILENAME:
		name = gnome_pixmap_file (pixmap_info);

		if (!name)
			g_warning ("Could not find GNOME pixmap file %s",
					(char *) pixmap_info);
		else {
			pixmap = gnome_pixmap_new_from_file (name);
			g_free (name);
		}

		break;

	default:
		g_assert_not_reached ();
		g_warning("Invalid pixmap_type %d", (int) pixmap_type);
	}

	return pixmap;
}

typedef struct {
	MENUITEMTYPE menutype ;
	const gchar* label ;
	
	guint keyval;
	guint modifier;
	
	GCallback callback ;
	
	GnomeUIPixmapType pixmap_type;
	gconstpointer pixmap_info;
	
	const gchar* gobj_key;
	gpointer* gobj_val;
	
	GtkWidget **menu_item_widget;

	GSList **radio_list;
} MENU_ITEM_DATA ;

static GtkWidget* create_menu_item (MENUITEMTYPE type,
					  const gchar* name,
					  GtkWidget* container,
					  GtkAccelGroup* accel,
					  guint keyval,
					  guint modifier,
					  GCallback callback,
					  GnomeUIPixmapType pixmap_type,
					  gconstpointer pixmap_info,
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
		menuitem = gtk_image_menu_item_new_with_mnemonic (_(name));
		break;
	}
	
	if (pixmap_type != GNOME_APP_PIXMAP_NONE &&
	    pixmap_info != NULL) {
		GtkWidget *pixmap = NULL;
		pixmap = create_ui_pixmap (NULL,
					   pixmap_type,
					   pixmap_info,
					   GTK_ICON_SIZE_MENU);
		if (pixmap) {
			gtk_widget_show (pixmap);
			gtk_image_menu_item_set_image (
				GTK_IMAGE_MENU_ITEM (menuitem), pixmap);
		}
		
	    }

	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER(container), menuitem);

	if (accel && keyval)
		gtk_widget_add_accelerator (menuitem, "activate", accel,
			      keyval, modifier, GTK_ACCEL_VISIBLE);

	g_signal_connect ( G_OBJECT(menuitem), "activate",
			callback, userdata);

	return menuitem;
}

static GtkWidget* create_radio_menu_item (GSList **group,
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

static GtkWidget* create_sub_menu(const gchar *name, GtkWidget* container)
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

static void create_menu_items ( GtkWidget *container, GtkAccelGroup* accel,
			gpointer user_data, MENU_ITEM_DATA *menudata )
{
	g_return_if_fail(menudata!=NULL);
	g_return_if_fail(container!=NULL);
	
	while (menudata!=NULL && menudata->menutype!=MI_NONE) {
		GtkWidget *item = NULL ;
		
		switch(menudata->menutype)
		{
		case MI_NONE:
			break ;
		
		case MI_SUBMENU:
			item = create_sub_menu(menudata->label, container) ;
			break ;
		
		case MI_SEPERATOR:
			item = 	create_menu_seperator(container) ;
			break;
		
		case MI_NORMAL:
		case MI_CHECK:
			item = create_menu_item( menudata->menutype,
				menudata->label, container,
				menudata->keyval ? accel : NULL,
				menudata->keyval,
				menudata->modifier,
				menudata->callback,
				menudata->pixmap_type, menudata->pixmap_info, user_data );
			break;
		
		case MI_RADIO:
			if (menudata->radio_list==NULL) {
				g_warning("radio_list field is NULL in \"%s\" menu item", menudata->label );
			} else {
				item = create_radio_menu_item( menudata->radio_list,
					menudata->label, container,
					menudata->keyval ? accel : NULL,
					menudata->keyval,
					menudata->modifier,
					menudata->callback,user_data );	
			}
			break;
		}
		
		if (menudata->gobj_key)
			g_object_set_data(G_OBJECT(item), menudata->gobj_key,menudata->gobj_val);
		
		if (menudata->menu_item_widget)
			*menudata->menu_item_widget = item ;
		
		menudata++ ;
	}
}
		
#define NO_KEYVAL 0
#define NO_MODIFIER 0
#define NO_GOBJ_KEY NULL
#define NO_GOBJ_VAL NULL
#define NO_MENU_ITEM NULL
#define NO_GSLIST NULL
#define NO_PIXMAP_TYPE 0
#define NO_PIXMAP_INFO 0

 static GtkWidget* gviewer_window_create_menus(GViewerWindow *obj)
{
	GtkWidget *int_viewer_menu;
	GtkWidget *submenu;

	MENU_ITEM_DATA file_menu_items[] = {
		{MI_NORMAL,_("_Close"),GDK_Escape,NO_MODIFIER,
				G_CALLBACK(menu_file_close),
				GNOME_APP_PIXMAP_STOCK,GNOME_STOCK_MENU_CLOSE,
				NO_GOBJ_KEY,NO_GOBJ_VAL,NO_MENU_ITEM,NO_GSLIST},
		{MI_NONE}
	};

	GSList *view_mode_list = NULL ;
	MENU_ITEM_DATA view_menu_items[] = {
		{MI_RADIO,_("_Text"),GDK_1,NO_MODIFIER,G_CALLBACK(menu_view_display_mode),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_DISPMODE_KEY,(gpointer)DISP_MODE_TEXT_FIXED,
				NO_MENU_ITEM,&view_mode_list},
		{MI_RADIO,_("_Binary"),GDK_2,NO_MODIFIER,G_CALLBACK(menu_view_display_mode),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_DISPMODE_KEY,(gpointer)DISP_MODE_BINARY,
				NO_MENU_ITEM,&view_mode_list},
		{MI_RADIO,_("_Hex dump"),GDK_3,NO_MODIFIER,G_CALLBACK(menu_view_display_mode),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_DISPMODE_KEY,(gpointer)DISP_MODE_HEXDUMP,
				NO_MENU_ITEM,&view_mode_list},
		{MI_RADIO,_("_Image"),GDK_4,NO_MODIFIER,G_CALLBACK(menu_view_display_mode),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_DISPMODE_KEY,(gpointer)DISP_MODE_IMAGE,
				NO_MENU_ITEM,&view_mode_list},
		{MI_SEPERATOR},
		{MI_NORMAL,_("_Zoom In"),GDK_plus,GDK_CONTROL_MASK,
				G_CALLBACK(menu_view_zoom_in),
				GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_IN,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("Zoom _Out"),GDK_minus,GDK_CONTROL_MASK,
				G_CALLBACK(menu_view_zoom_out),
				GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_OUT,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("_Normal Size"),GDK_0,GDK_CONTROL_MASK,
				G_CALLBACK(menu_view_zoom_normal),
				GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_100,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("Best _Fit"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_view_zoom_best_fit),
				GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_FIT,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NONE}
	};
	
	
	GtkWidget *encoding_submenu = NULL;
#ifdef EXTERNAL_TOOLS				
	GSList *text_parser_list = NULL ;
#endif
	MENU_ITEM_DATA text_menu_items[] = {
		{MI_NORMAL,_("_Copy Text Selection"),GDK_C,GDK_CONTROL_MASK,
				G_CALLBACK(menu_edit_copy),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
#ifdef EXTERNAL_TOOLS				
		{MI_SEPERATOR},
		{MI_RADIO,_("_No Parsing (original file)"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_view_internal_viewer),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,&text_parser_list},
		{MI_RADIO,_("_HTML Parser"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_view_external_tool),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_EXTERNAL_TOOL_KEY,(gpointer)"html",
				NO_MENU_ITEM,&text_parser_list},
		{MI_RADIO,_("_PS/PDF Parser"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_view_external_tool),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_EXTERNAL_TOOL_KEY,(gpointer)"pspdf",
				NO_MENU_ITEM,&text_parser_list},
#endif
		{MI_SEPERATOR},
		{MI_CHECK,_("_Wrap lines"),GDK_W,NO_MODIFIER,
				G_CALLBACK(menu_view_wrap),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				&obj->priv->wrap_mode_menu_item,NO_GSLIST},
		{MI_SEPERATOR},
		{MI_SUBMENU, _("_Encoding"), NO_KEYVAL, NO_MODIFIER,
				G_CALLBACK(NULL),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				&encoding_submenu,NO_GSLIST},
		{MI_NONE}
	};

#define ENCODING_MENU_ITEM(label,keyval,value) {MI_RADIO,_(label), \
	keyval, NO_MODIFIER, G_CALLBACK(menu_view_set_charset), \
	NO_PIXMAP_TYPE,NO_PIXMAP_INFO, \
	G_OBJ_CHARSET_KEY, (gpointer)value, \
	NO_MENU_ITEM, &text_encoding_list}
	
	GSList *text_encoding_list = NULL ;
	MENU_ITEM_DATA encoding_menu_items[] = {
		ENCODING_MENU_ITEM("_UTF-8", 			GDK_u,"UTF8"),
		ENCODING_MENU_ITEM("English (US-_ASCII)",	GDK_a,"ASCII"),
		ENCODING_MENU_ITEM("Terminal (CP437)", 	        GDK_q,"CP437"),
		ENCODING_MENU_ITEM("Arabic (ISO-8859-6)",	NO_KEYVAL,"ISO-8859-6" ),
		ENCODING_MENU_ITEM("Arabic (Windows, CP1256)", 	NO_KEYVAL,"ARABIC" ),
		ENCODING_MENU_ITEM("Arabic (Dos, CP864)",	NO_KEYVAL,"CP864" ),
		ENCODING_MENU_ITEM("Baltic (ISO-8859-4)",	NO_KEYVAL,"ISO-8859-4"),
		ENCODING_MENU_ITEM("Central European (ISO-8859-2)",NO_KEYVAL,"ISO-8859-2"),
		ENCODING_MENU_ITEM("Central European (CP1250)",	NO_KEYVAL,"CP1250" ),
		ENCODING_MENU_ITEM("Cyrillic (ISO-8859-5)",	NO_KEYVAL,"ISO-8859-5"),
		ENCODING_MENU_ITEM("Cyrillic (CP1251)",	        NO_KEYVAL,"CP1251" ),
		ENCODING_MENU_ITEM("Greek (ISO-8859-7)",	NO_KEYVAL,"ISO-8859-7" ),
		ENCODING_MENU_ITEM("Greek (CP1253)",		NO_KEYVAL,"CP1253" ),
		ENCODING_MENU_ITEM("Hebrew (Windows, CP1255)", 	NO_KEYVAL,"HEBREW" ),
		ENCODING_MENU_ITEM("Hebrew (Dos, CP862)",	NO_KEYVAL,"CP862" ),
		ENCODING_MENU_ITEM("Hebrew (ISO-8859-8)",	NO_KEYVAL,"ISO-8859-8" ),
		ENCODING_MENU_ITEM("Latin 9 (ISO-8859-15))",	NO_KEYVAL,"ISO-8859-15" ),
		ENCODING_MENU_ITEM("Maltese (ISO-8859-3)",	NO_KEYVAL,"ISO-8859-3" ),
		ENCODING_MENU_ITEM("Turkish (ISO-8859-9)",	NO_KEYVAL,"ISO-8859-9" ),
		ENCODING_MENU_ITEM("Turkish (CP1254)",	        NO_KEYVAL,"CP1254" ),
		ENCODING_MENU_ITEM("Western (CP1252)",	        NO_KEYVAL,"CP1252" ),
		ENCODING_MENU_ITEM("Western (ISO-8859-1)",	NO_KEYVAL,"ISO-8859-1" ),
		{MI_NONE}
	};

	MENU_ITEM_DATA image_menu_items[] = {
		{MI_CHECK,_("_Show EXIF/IPTC Information"),GDK_e,NO_MODIFIER,
				G_CALLBACK(menu_view_exif_information),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_SEPERATOR},
		{MI_NORMAL,_("_Rotate Clockwise"),GDK_R,GDK_CONTROL_MASK,
				G_CALLBACK(menu_image_operation),
				GNOME_APP_PIXMAP_FILENAME, "gnome-commander/rotate-90-16.xpm",
				G_OBJ_IMAGE_OP_KEY,(gpointer)ROTATE_CLOCKWISE,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("Rotate Counter Clockwis_e"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_image_operation),
				GNOME_APP_PIXMAP_FILENAME, "gnome-commander/rotate-270-16.xpm",
				G_OBJ_IMAGE_OP_KEY,(gpointer)ROTATE_COUNTERCLOCKWISE,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("_Rotate 180\xC2\xB0"),GDK_r,GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(menu_image_operation),
				GNOME_APP_PIXMAP_FILENAME, "gnome-commander/rotate-180-16.xpm",
				G_OBJ_IMAGE_OP_KEY,(gpointer)ROTATE_UPSIDEDOWN,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("Flip _Vertical"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_image_operation),
				GNOME_APP_PIXMAP_FILENAME, "gnome-commander/flip-vertical-16.xpm",
				G_OBJ_IMAGE_OP_KEY,(gpointer)FLIP_VERTICAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("Flip _Horizontal"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_image_operation),
				GNOME_APP_PIXMAP_FILENAME, "gnome-commander/flip-horizontal-16.xpm",
				G_OBJ_IMAGE_OP_KEY,(gpointer)FLIP_HORIZONTAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NONE}
	};

	GtkWidget* binary_mode_settings_submenu = NULL ;
	MENU_ITEM_DATA settings_menu_items[] = {
		{MI_SUBMENU, _("_Binary Mode"), NO_KEYVAL, NO_MODIFIER,
				G_CALLBACK(NULL),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				&binary_mode_settings_submenu,NO_GSLIST},
				
		{MI_CHECK,_("_Decimal Offset in Hexdump"),GDK_d,GDK_CONTROL_MASK,
				G_CALLBACK(menu_settings_hex_decimal_offset),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				&obj->priv->hex_offset_menu_item,NO_GSLIST},
		{MI_SEPERATOR},
		{MI_NORMAL,_("_Save Current Settings"),GDK_s,GDK_CONTROL_MASK,
				G_CALLBACK(menu_settings_save_settings),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NONE}
	};

	GSList *binmode_chars_per_line_list = NULL ;
	MENU_ITEM_DATA binmode_settings_menu_items[] = {
		{MI_RADIO,_("_20 chars/line"),GDK_2, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(menu_settings_binary_bytes_per_line),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_BYTES_PER_LINE_KEY,(gpointer)20,
				&obj->priv->fixed_limit_menu_items[0],&binmode_chars_per_line_list},
		{MI_RADIO,_("_40 chars/line"),GDK_4, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(menu_settings_binary_bytes_per_line),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_BYTES_PER_LINE_KEY,(gpointer)40,
				&obj->priv->fixed_limit_menu_items[1],&binmode_chars_per_line_list},
		{MI_RADIO,_("_80 chars/line"),GDK_8, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				G_CALLBACK(menu_settings_binary_bytes_per_line),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				G_OBJ_BYTES_PER_LINE_KEY,(gpointer)80,
				&obj->priv->fixed_limit_menu_items[2],&binmode_chars_per_line_list},
		{MI_NONE}
	};
	
#ifdef HAVE_HELP_ABOUT
	MENU_ITEM_DATA help_menu_items[] = {
		{MI_NORMAL,_("Quick Help"),GDK_F1,NO_MODIFIER,
				G_CALLBACK(menu_help_quick_help),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NORMAL,_("About"),NO_KEYVAL,NO_MODIFIER,
				G_CALLBACK(menu_help_about),
				NO_PIXMAP_TYPE,NO_PIXMAP_INFO,
				NO_GOBJ_KEY,NO_GOBJ_VAL,
				NO_MENU_ITEM,NO_GSLIST},
		{MI_NONE}
	};
#endif
	
	int_viewer_menu = gtk_menu_bar_new ();
	obj->priv->accel_group = gtk_accel_group_new ();

	/* File Menu */
	submenu = create_sub_menu(_("_File"), int_viewer_menu) ;
	create_menu_items( submenu, obj->priv->accel_group, obj, file_menu_items );

	submenu = create_sub_menu(_("_View"), int_viewer_menu) ;
	create_menu_items( submenu, obj->priv->accel_group, obj, view_menu_items );

	submenu = create_sub_menu(_("_Text"), int_viewer_menu) ;
	create_menu_items( submenu, obj->priv->accel_group, obj, text_menu_items );
	/* "encoding_submenu" was initialized when "text_menu_items" is usd to create the "text" menu*/
	create_menu_items( encoding_submenu, obj->priv->accel_group, obj, encoding_menu_items );
	
	submenu = create_sub_menu(_("_Image"), int_viewer_menu);
	create_menu_items( submenu, obj->priv->accel_group, obj, image_menu_items );
	
	submenu = create_sub_menu(_("_Settings"), int_viewer_menu) ;
	create_menu_items( submenu, obj->priv->accel_group, obj, settings_menu_items );

	create_menu_items( binary_mode_settings_submenu, obj->priv->accel_group, obj, binmode_settings_menu_items);

#ifdef HAVE_HELP_ABOUT
	submenu = create_sub_menu(_("_Help"), int_viewer_menu) ;
	create_menu_items( submenu, obj->priv->accel_group, obj, help_menu_items );
#endif

	gtk_window_add_accel_group (GTK_WINDOW(obj), obj->priv->accel_group);
	return int_viewer_menu;
}


/* Event Handlers */
static void menu_file_close (GtkMenuItem *item, GViewerWindow *obj)
{
	gtk_widget_destroy(GTK_WIDGET(obj));
}

static void menu_view_exif_information(GtkMenuItem *item, GViewerWindow *obj)
{
	g_return_if_fail(obj!=NULL);
	g_return_if_fail(obj->priv->viewer!=NULL);

	if (gviewer_get_display_mode(obj->priv->viewer) != DISP_MODE_IMAGE)
		return ;
	
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
		gviewer_window_show_exif_viewer(obj);
	} else {
		gviewer_window_hide_exif_viewer(obj);
	}
}

#ifdef EXTERNAL_TOOLS
static void menu_view_external_tool(GtkMenuItem *item, GViewerWindow *obj)
{
	char *tool;
	
	g_return_if_fail(obj!=NULL);
	g_return_if_fail(obj->priv->viewer!=NULL);

	tool = (char*)g_object_get_data(G_OBJECT(item),G_OBJ_EXTERNAL_TOOL_KEY);
	g_return_if_fail(tool);
	
	gviewer_window_activate_external_tool(obj,tool) ;
	gtk_widget_draw(GTK_WIDGET(obj->priv->viewer),NULL);
}

static void menu_view_internal_viewer(GtkMenuItem *item, GViewerWindow *obj)
{
	gviewer_window_activate_internal_viewer(obj);
}
#endif

static void menu_view_display_mode(GtkMenuItem *item, GViewerWindow *obj)
{
	VIEWERDISPLAYMODE dispmode;
	
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		return ;

	dispmode = (VIEWERDISPLAYMODE)g_object_get_data(G_OBJECT(item),G_OBJ_DISPMODE_KEY);

	if (dispmode==DISP_MODE_IMAGE) {
#ifdef EXTERNAL_TOOLS
		gviewer_window_activate_internal_viewer(obj);
#endif	
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(obj->priv->show_exif_menu_item))) {
			gviewer_window_show_exif_viewer(obj);
		} else {
			gviewer_window_hide_exif_viewer(obj);
		}
	} else {
		gviewer_window_hide_exif_viewer(obj);
	}
	
	gviewer_set_display_mode(obj->priv->viewer,dispmode) ;
	gtk_widget_grab_focus(GTK_WIDGET(obj->priv->viewer));
	
	gtk_widget_draw(GTK_WIDGET(obj->priv->viewer),NULL);
}

static void menu_view_set_charset(GtkMenuItem *item, GViewerWindow *obj)
{
	gchar *charset = NULL ;

	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		return ;

	charset = g_object_get_data(G_OBJECT(item),G_OBJ_CHARSET_KEY);
	g_return_if_fail(charset!=NULL);

	/* ASCII is set implicitly when setting a charset */
	//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->priv->ascii_menu_item), TRUE);

	gviewer_set_encoding(obj->priv->viewer,charset);
	gtk_widget_draw(GTK_WIDGET(obj->priv->viewer),NULL);
}

static void menu_image_operation(GtkMenuItem *item, GViewerWindow *obj)
{
	IMAGEOPERATION imageop;
	
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);

	imageop = (IMAGEOPERATION)g_object_get_data(G_OBJECT(item),G_OBJ_IMAGE_OP_KEY);

	gviewer_image_operation(obj->priv->viewer, imageop);
	
	gtk_widget_draw(GTK_WIDGET(obj->priv->viewer),NULL);
}

static void menu_view_zoom_in(GtkMenuItem *item, GViewerWindow *obj)
{
	set_zoom_in(obj);
}

static void menu_view_zoom_out(GtkMenuItem *item, GViewerWindow *obj)
{
	set_zoom_out(obj);
}

static void menu_view_zoom_normal(GtkMenuItem *item, GViewerWindow *obj)
{
	set_zoom_normal(obj);
}

static void menu_view_zoom_best_fit(GtkMenuItem *item, GViewerWindow *obj)
{
	set_zoom_best_fit(obj);
}


static void menu_settings_binary_bytes_per_line(GtkMenuItem *item, GViewerWindow *obj)
{
	int bytes_per_line;
	
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);
	
	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		return ;

	bytes_per_line = (int)g_object_get_data(G_OBJECT(item),G_OBJ_BYTES_PER_LINE_KEY);

	gviewer_set_fixed_limit(obj->priv->viewer,bytes_per_line) ;
	gtk_widget_draw(GTK_WIDGET(obj->priv->viewer),NULL);
}

static void menu_edit_copy(GtkMenuItem *item, GViewerWindow *obj)
{
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->active_viewer);
	
	gviewer_copy_selection(obj->priv->active_viewer);
}

static void menu_view_wrap(GtkMenuItem *item, GViewerWindow *obj)
{
	gboolean wrap;
	
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);

	wrap = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
	
	gviewer_set_wrap_mode(obj->priv->viewer,wrap);
	gtk_widget_draw(GTK_WIDGET(obj->priv->viewer),NULL);
	
	if (obj->priv->exif_active) {
		gviewer_set_wrap_mode(obj->priv->exif_viewer,wrap);
		gtk_widget_draw(GTK_WIDGET(obj->priv->exif_viewer),NULL);
	}
}


static void menu_settings_hex_decimal_offset(GtkMenuItem *item, GViewerWindow *obj)
{
	gboolean hex;
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);

	hex = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
	gviewer_set_hex_offset_display(obj->priv->viewer,hex);
	
	if (obj->priv->exif_active) {
		gviewer_set_hex_offset_display(obj->priv->exif_viewer,hex);
	}
}

static gchar* get_string (const gchar *path, const gchar *def)
{
	gboolean b = FALSE;
	gchar *value = gnome_config_get_string_with_default (path, &b);
	if (b)
		return g_strdup (def);	
	return value;
}

static gint get_int (const gchar *path, int def)
{
	gboolean b = FALSE;
	gint value = gnome_config_get_int_with_default (path, &b);
	if (b)
		return def;
	return value;
}

static gboolean get_bool (const gchar *path, gboolean def)
{
	gboolean b = FALSE;
	gboolean value = gnome_config_get_bool_with_default (path, &b);
	if (b)
		return def;
	return value;
}


#define DEFAULT_PATH_PREFIX "/gnome-commander/internal_viewer/"
void gviewer_window_load_settings(/* out */ GViewerWindowSettings *settings)
{
	gchar *temp;
	g_return_if_fail(settings!=NULL);

	temp = get_string(DEFAULT_PATH_PREFIX "charset","ASCII");
	strncpy(settings->charset,temp,sizeof(settings->charset));
	g_free(temp);
	
	temp = get_string(DEFAULT_PATH_PREFIX "fixed_font_name","Monospace");
	strncpy(settings->fixed_font_name,temp,sizeof(settings->fixed_font_name));
	g_free(temp);

	temp = get_string(DEFAULT_PATH_PREFIX "variable_font_name","Sans");
	strncpy(settings->variable_font_name,temp,sizeof(settings->variable_font_name));
	g_free(temp);
	
	settings->hex_decimal_offset = get_bool(DEFAULT_PATH_PREFIX "hex_offset_display",TRUE);
	settings->wrap_mode = get_bool(DEFAULT_PATH_PREFIX "wrap_mode",TRUE );
	
	settings->font_size = get_int(DEFAULT_PATH_PREFIX "font_size", 12);
	settings->tab_size = get_int(DEFAULT_PATH_PREFIX "tab_size ", 8);
	settings->binary_bytes_per_line = get_int(DEFAULT_PATH_PREFIX "binary_bytes_per_line", 80);
	
	settings->rect.x = get_int(DEFAULT_PATH_PREFIX "x", -2);
	settings->rect.y = get_int(DEFAULT_PATH_PREFIX "y", -2);
	settings->rect.width = get_int(DEFAULT_PATH_PREFIX "width", -1);
	settings->rect.height = get_int(DEFAULT_PATH_PREFIX "height", -1);
}

static void menu_settings_save_settings(GtkMenuItem *item, GViewerWindow *obj)
{
	GViewerWindowSettings settings;
	
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);
	
	gviewer_window_get_current_settings(obj,&settings);
	
	gnome_config_set_string(DEFAULT_PATH_PREFIX "charset",settings.charset);
	gnome_config_set_string(DEFAULT_PATH_PREFIX "fixed_font_name",settings.fixed_font_name);
	gnome_config_set_string(DEFAULT_PATH_PREFIX "variable_font_name",settings.variable_font_name);
	
	gnome_config_set_bool(DEFAULT_PATH_PREFIX "hex_offset_display",settings.hex_decimal_offset);
	gnome_config_set_bool(DEFAULT_PATH_PREFIX "wrap_mode",settings.wrap_mode);
	
	gnome_config_set_int(DEFAULT_PATH_PREFIX "font_size", settings.font_size);
	gnome_config_set_int(DEFAULT_PATH_PREFIX "tab_size ", settings.tab_size );
	gnome_config_set_int(DEFAULT_PATH_PREFIX "binary_bytes_per_line", settings.binary_bytes_per_line);
	
	gnome_config_set_int(DEFAULT_PATH_PREFIX "x", settings.rect.x);
	gnome_config_set_int(DEFAULT_PATH_PREFIX "y", settings.rect.y);
	gnome_config_set_int(DEFAULT_PATH_PREFIX "width", settings.rect.width);
	gnome_config_set_int(DEFAULT_PATH_PREFIX "height", settings.rect.height);
	
	gnome_config_sync();
}

#ifdef HAVE_HELP_ABOUT
static void menu_help_quick_help(GtkMenuItem *item, GViewerWindow *obj)
{
}

static void menu_help_about(GtkMenuItem *item, GViewerWindow *obj)
{
}
#endif

#ifdef EXTERNAL_TOOLS
static void gviewer_window_add_external_tool(GViewerWindow *obj, const gchar *name, const gchar* command )
{
	GViewerWindowExternalTool *tool ;
	
	g_return_if_fail(obj);
	g_return_if_fail(name);
	g_return_if_fail(command);
	
	tool = g_new0(GViewerWindowExternalTool,1);
	g_return_if_fail(tool);
	
	tool->name = g_strdup(name);
	tool->command = g_strdup(command);
	tool->attached_fd = -1 ;
	
	g_hash_table_insert(obj->priv->external_tools,
			g_strdup(name), tool );
}

static int gviewer_window_run_external_tool(GViewerWindow *obj, GViewerWindowExternalTool *tool)
{
	FILE* file;
	int fd = -1;
	int ret ;
	gchar* cmd_with_filename = NULL ;
	gchar* cmd_with_redir = NULL;

	

	g_return_val_if_fail(obj!=NULL,-1);
	g_return_val_if_fail(tool!=NULL,-1);
	g_return_val_if_fail(tool->command!=NULL,-1);
	g_return_val_if_fail(obj->priv->filename!=NULL,-1);
	
	file = tmpfile() ;
	if (file==NULL) {
		g_warning("Failed to create temporary file");
		goto error;
	}
	fd = fileno(file);
	if (fd==-1) {
		fclose(file);
		g_warning("Failed to extract tempfile descriptor");
		goto error;
	}
	
	cmd_with_filename = g_strdup_printf(tool->command, obj->priv->filename);
	
	cmd_with_redir = g_strdup_printf("%s >&%d", cmd_with_filename, fd);
	
	ret = system(cmd_with_redir) ;
	
	if (ret==-1) {
		fd = -1 ;
		g_warning("Program execution (%s) failed", cmd_with_redir);
		goto error ;
	}

	
error:
	if (cmd_with_filename)
		g_free(cmd_with_filename);
	if (cmd_with_redir)
		g_free(cmd_with_redir);
	
	

	return fd ;
}
#endif

static int gviewer_window_run_exif(GViewerWindow *obj)
{
	FILE* file;
	int fd = -1;
	int ret ;
	gchar* cmd_with_redir = NULL;

	g_return_val_if_fail(obj!=NULL,-1);
	g_return_val_if_fail(obj->priv->filename!=NULL,-1);
	
	file = tmpfile() ;
	if (file==NULL) {
		g_warning("Failed to create temporary file");
		goto error;
	}
	fd = fileno(file);
	if (fd==-1) {
		fclose(file);		
		g_warning("Failed to extract tempfile descriptor");
		goto error;
	}
	
	cmd_with_redir = g_strdup_printf("iptc '%s' >&%d", obj->priv->filename, fd);
	ret = system(cmd_with_redir) ;
	if (ret==-1) {
		g_warning("IPTC execution (%s) failed", cmd_with_redir);
	}
	if (cmd_with_redir)
		g_free(cmd_with_redir);

	cmd_with_redir = g_strdup_printf("exif '%s' >&%d", obj->priv->filename, fd);
	ret = system(cmd_with_redir) ;
	if (ret==-1) {
		g_warning("EXIF execution (%s) failed", cmd_with_redir);
	}
	g_free(cmd_with_redir);
	
error:
	return fd ;
}

#ifdef EXTERNAL_TOOLS
static void gviewer_window_activate_external_tool(GViewerWindow *obj, const gchar *name)
{
	GViewerWindowExternalTool *tool ;
	
	g_return_if_fail(obj!=NULL);
	g_return_if_fail(name!=NULL);
	
	tool = g_hash_table_lookup(obj->priv->external_tools, name) ;
	if (tool==NULL) {
		g_warning("Could not find external tool \"%s\"", name);
		return;
	}
	
	if (obj->priv->active_external_tool==tool)
		return ;
	
	if (tool->attached_fd==-1)
		tool->attached_fd = gviewer_window_run_external_tool(obj,tool);
	g_return_if_fail(tool->attached_fd!=-1);
	
	obj->priv->active_external_tool = tool ;
	gviewer_load_filedesc(obj->priv->viewer,tool->attached_fd) ;
}

static void gviewer_window_activate_internal_viewer(GViewerWindow *obj)
{
	g_return_if_fail(obj!=NULL);
	
	/* internal viewer is already active */
	if (obj->priv->active_external_tool==NULL)
		return;
	
	obj->priv->active_external_tool = NULL ;
	gviewer_load_file(obj->priv->viewer,obj->priv->filename) ;
}
#endif

static void gviewer_window_show_exif_viewer(GViewerWindow *obj)
{
	g_return_if_fail(obj!=NULL);
	g_return_if_fail(obj->priv->exif_viewer!=NULL);
	
	if (obj->priv->exif_active)
		return ;

	if (obj->priv->exit_data_fd==-1)
		obj->priv->exit_data_fd = gviewer_window_run_exif(obj);
	
	g_return_if_fail(obj->priv->exit_data_fd!=-1);
	gviewer_load_filedesc(obj->priv->exif_viewer, obj->priv->exit_data_fd);
	gtk_widget_show(GTK_WIDGET(obj->priv->exif_viewer));
	
	obj->priv->exif_active = TRUE ;
	gtk_box_pack_start (GTK_BOX (obj->priv->vbox),
		GTK_WIDGET(obj->priv->exif_viewer), TRUE, TRUE, 0);
	
	obj->priv->active_viewer = obj->priv->exif_viewer;

	gtk_widget_grab_focus(GTK_WIDGET(obj->priv->exif_viewer));
}

static void gviewer_window_hide_exif_viewer(GViewerWindow *obj)
{
	g_return_if_fail(obj!=NULL);
	g_return_if_fail(obj->priv->exif_viewer!=NULL);

	if (!obj->priv->exif_active)
		return ;
	
	obj->priv->exif_active = FALSE ;
	gtk_container_remove(GTK_CONTAINER(obj->priv->vbox),GTK_WIDGET(obj->priv->exif_viewer));
	gtk_widget_grab_focus(GTK_WIDGET(obj->priv->viewer));
	
	obj->priv->active_viewer = obj->priv->viewer;
}


static void set_zoom_in(GViewerWindow *obj)
{
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);
	
    switch (gviewer_get_display_mode(obj->priv->viewer)) {
    case DISP_MODE_TEXT_FIXED:
    case DISP_MODE_BINARY:
    case DISP_MODE_HEXDUMP:
        {
	       int size = gviewer_get_font_size(obj->priv->active_viewer);

           if (size==0 || size>32)  return;
           
	       size++;
       	   gviewer_set_font_size(obj->priv->active_viewer,size);
        }    
        break;
        
    case DISP_MODE_IMAGE:
       {
	       gviewer_set_best_fit(obj->priv->viewer,FALSE);
	
	       if (obj->priv->current_scale_index<MAX_SCALE_FACTOR_INDEX-1)
		      obj->priv->current_scale_index++ ;
	
	       if (gviewer_get_scale_factor(obj->priv->viewer) ==
		       image_scale_factors[obj->priv->current_scale_index])
		      return;
	
	       gviewer_set_scale_factor(obj->priv->viewer,
		      image_scale_factors[obj->priv->current_scale_index] ) ;
       }
	   break;
	   
    default:
        break;
    }
}


static void set_zoom_out(GViewerWindow *obj)
{
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);
	
	
    switch (gviewer_get_display_mode(obj->priv->viewer)) {
    case DISP_MODE_TEXT_FIXED:
    case DISP_MODE_BINARY:
    case DISP_MODE_HEXDUMP:
        {
	       int size = gviewer_get_font_size(obj->priv->active_viewer);

           if (size==0 || size<4)  return;
           
	       size--;
	       gviewer_set_font_size(obj->priv->active_viewer,size);
        }    
        break;
        
    case DISP_MODE_IMAGE:
       {
	       gviewer_set_best_fit(obj->priv->viewer,FALSE);
	
	       if (obj->priv->current_scale_index>0)
		       obj->priv->current_scale_index-- ;
	
	       if (gviewer_get_scale_factor(obj->priv->viewer) ==
		      image_scale_factors[obj->priv->current_scale_index])
		      return;
	
	       gviewer_set_scale_factor(obj->priv->viewer,
		      image_scale_factors[obj->priv->current_scale_index] ) ;
       }
	   break;
	   
    default:
        break;
    }
}


static void set_zoom_normal(GViewerWindow *obj)
{
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);
	
    switch (gviewer_get_display_mode(obj->priv->viewer)) {
    case DISP_MODE_TEXT_FIXED:
    case DISP_MODE_BINARY:
    case DISP_MODE_HEXDUMP:
       /* needs to completed with resetting to default font size */
	   break;
	   
    case DISP_MODE_IMAGE:
	   gviewer_set_best_fit(obj->priv->viewer,FALSE);
	   gviewer_set_scale_factor(obj->priv->viewer,1 ) ;
	   break;
	   
    default:
        break;
    }
}


static void set_zoom_best_fit(GViewerWindow *obj)
{
	g_return_if_fail(obj);
	g_return_if_fail(obj->priv->viewer);
	
	if (gviewer_get_display_mode(obj->priv->viewer)!=DISP_MODE_IMAGE)
	   return;
	
	gviewer_set_best_fit(obj->priv->viewer,TRUE);
}
