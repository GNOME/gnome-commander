/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyright (C) 2010-2010 Guillaume Wardavoir

	---------------------------------------------------------------------------

	C++

	Contain variadic macros

	---------------------------------------------------------------------------

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

#include	<config.h>

#include	<stdlib.h>

#include	<gtk/gtksignal.h>
#include	<gtk/gtkvbox.h>

#include	<libgnomevfs/gnome-vfs.h>
#include	<libgnomevfs/gnome-vfs-utils.h>

#include	"gnome-cmd-includes.h"
#include	"gnome-cmd-combo.h"
#include	"gnome-cmd-main-win.h"
#include	"gnome-cmd-style.h"
#include	"gnome-cmd-data.h"

#include	"gnome-cmd-foldview-private.h"
#include	"gnome-cmd-foldview-gvfs.h"

//  ***************************************************************************
//  *																		  *
//  *								Defines								      *
//  *																		  *
//  ***************************************************************************

#define GVFS_MAX_ASYNC_OPS					10000
#define GVFS_ITEMS_PER_NOTIFICATION			30

//  ***************************************************************************
//  *																		  *
//  *								Helpers								      *
//  *																		  *
//  ***************************************************************************

//=============================================================================
//  Common vars
//=============================================================================
static  GnomeVFSResult  sVFSResult		= GNOME_VFS_OK;	 // for sync operations

extern  GnomeCmdMainWin *main_win;

//=============================================================================
//  Logger
//=============================================================================
/*
	<ESC>[{attr};{fg};{bg}m

{attr} needs to be one of the following:
	0 Reset All Attributes (return to normal mode)
	1 Bright (usually turns on BOLD)
	2 Dim
	3 Underline
	5 Blink
	7 Reverse
	8 Hidden

{fg} needs to be one of the following:
      30 Black
      31 Red
      32 Green
      33 Yellow
      34 Blue
      35 Magenta
      36 Cyan
      37 White

{bg} needs to be one of the following:
      40 Black
      41 Red
      42 Green
      43 Yellow
      44 Blue
      45 Magenta
      46 Cyan
      47 White
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Core logging
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static  char			sLogStr		[1024];
static  char			sLogStrFinal[1024];

void gwr_print(const char* str)
{
	sprintf(sLogStrFinal,"%s%s", str,sLogStr);
	printf("%s\n",sLogStrFinal);
}
void gwr_inf(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_INF
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	gwr_print("\033[0;32mINF:\033[0;30m");
}
void gwr_wng(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_WNG
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	gwr_print("\033[0;35mWNG:\033[0;30m");
}
void gwr_err(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_ERR
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	gwr_print("\033[0;31mERR:\033[0;30m");
}
void gwr_inf_vfs(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_VFS
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	strcat(sLogStr, " [VFS-INF:");
	strcat(sLogStr,   gnome_vfs_result_to_string(sVFSResult));
	strcat(sLogStr, "]");
	gwr_print("\033[0;31mERR:\033[0;30m");
}
void gwr_err_vfs(const char* fmt, ...)
{
	#ifndef DEBUG_SHOW_VFS
		return;
	#endif
	va_list val; va_start(val, fmt); vsprintf(sLogStr, fmt, val); va_end(val);
	strcat(sLogStr, " [VFS-ERR:");
	strcat(sLogStr,   gnome_vfs_result_to_string(sVFSResult));
	strcat(sLogStr, "]");
	gwr_print("\033[0;31mERR:\033[0;30m");
}


//  ***************************************************************************
//  *																		  *
//  *								Divers									  *
//  *																		  *
//  ***************************************************************************
GcmdGtkFoldview::eFileAccess GcmdGtkFoldview::Access_from_permissions(
	GnomeVFSFilePermissions permissions)
{
	eFileAccess		access  = eAccessUnknown;
	gboolean		b_read	= FALSE;
	gboolean		b_write	= FALSE;
	if ( ( permissions & GNOME_VFS_PERM_ACCESS_READABLE ) != 0 )	b_read  = TRUE;
	if ( ( permissions & GNOME_VFS_PERM_ACCESS_WRITABLE ) != 0 )	b_write = TRUE;
	if ( b_read )
	{
		access = ( b_write ? eAccessReadWrite : eAccessReadOnly );
	}
	else
	{
		access = ( b_write ? eAccessUnknown : eAccessForbidden );
	}
	return access;
}

GcmdGtkFoldview::View::eIcon GcmdGtkFoldview::View::Icon_from_type_permissions(
	GnomeVFSFileType		type, 
	GnomeVFSFilePermissions permissions)
{  
	eFileAccess	access  = eAccessUnknown;

	access = Access_from_permissions(permissions);

	return Icon_from_type_access(type, access);
}

GcmdGtkFoldview::View::eIcon GcmdGtkFoldview::View::Icon_from_type_access(
	GnomeVFSFileType				type, 
	GcmdGtkFoldview::eFileAccess	access)
{  
	if ( type == GNOME_VFS_FILE_TYPE_DIRECTORY )
	{
		switch ( access )
		{
			case	eAccessReadWrite: return GcmdGtkFoldview::View::eIconDirReadWrite; break;
			case	eAccessReadOnly : return GcmdGtkFoldview::View::eIconDirReadOnly;  break;
			case	eAccessForbidden: return GcmdGtkFoldview::View::eIconDirForbidden; break;
			default					: return GcmdGtkFoldview::View::eIconUnknown;	   break;
		}
	}

	if ( type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK )
	{
		switch ( access )
		{
			case	eAccessReadWrite: return GcmdGtkFoldview::View::eIconSymlinkToDirReadWrite;	 break;
			case	eAccessReadOnly : return GcmdGtkFoldview::View::eIconSymlinkToDirReadOnly;	 break;
			case	eAccessForbidden: return GcmdGtkFoldview::View::eIconSymlinkToDirForbidden;	 break;
			default					: return GcmdGtkFoldview::View::eIconUnknown;				 break;
		}
	}

	return View::eIconUnknown;
}

//  ***************************************************************************
//  *																		  *
//  *						widget showing / hiding							  *
//  *																		  *
//  ***************************************************************************
static  GtkWidget*	GcmdGtkFoldviewSingleton	= NULL;

//  ***************************************************************************
//  *																		  *
//  *						public interface								  *
//  *																		  *
//  ***************************************************************************

//=============================================================================
//  Singleton
//=============================================================================

GtkWidget* GcmdWidget()
{
	if ( GcmdGtkFoldviewSingleton != NULL )
		return GcmdGtkFoldviewSingleton;

	GcmdGtkFoldviewSingleton = gcmdgtkfoldview_new();

	return GcmdGtkFoldviewSingleton;
}

GcmdGtkFoldview* GcmdFoldview()
{
	return (GcmdGtkFoldview*)( GcmdWidget() );
}

void	gnome_cmd_foldview_update_style(GtkWidget *widget)
{
	g_return_if_fail( widget != NULL );

	(GCMDGTKFOLDVIEW(widget))->view.update_style();
}

GtkWidget* gnome_cmd_foldview_get_instance()
{
	return GcmdWidget();
}

//=============================================================================
//  Init
//=============================================================================

//=============================================================================
//  Root element
//=============================================================================
gboolean GcmdGtkFoldview::root_uri_set_1(gchar * text)
{
	GnomeVFSURI *uri	= GVFS_uri_new(text);
	control_root_uri_set(uri);
	gnome_vfs_uri_unref(uri);
	return TRUE;
}

gboolean GcmdGtkFoldview::root_uri_set_2(GnomeVFSURI *uri)
{
	return control_root_uri_set(uri);
}



//  ***************************************************************************
//  *																		  *
//  *						gtk implementation								  *
//  *																		  *
//  ***************************************************************************

enum
{
  GCMDGTKFOLDVIEW_SIGNAL,
  LAST_SIGNAL
};

static void gcmdgtkfoldview_class_init          (GcmdGtkFoldviewClass   *klass);
static void gcmdgtkfoldview_init                (GcmdGtkFoldview  		*ttt);
//static void init								(GcmdGtkFoldview		*foldview);

static guint gcmdgtkfoldview_signals[LAST_SIGNAL] = { 0 };



//-----------------------------------------------------------------------------
//	GcmdGtkFoldview GType implementation
//-----------------------------------------------------------------------------
GType
gcmdgtkfoldview_get_type (void)
{
	static GType fv_type = 0;

	if (!fv_type)
	{
		const GTypeInfo fv_info =
		{
			sizeof (GcmdGtkFoldviewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gcmdgtkfoldview_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GcmdGtkFoldview),
			0,
			(GInstanceInitFunc) gcmdgtkfoldview_init,
		};

	fv_type = g_type_register_static(GTK_TYPE_VBOX, "gtkGcmdFoldview", &fv_info, (GTypeFlags)0);
	}

	return fv_type;
}

//-----------------------------------------------------------------------------
//	GcmdGtkFoldview class initialization
//-----------------------------------------------------------------------------
static void
gcmdgtkfoldview_class_init (GcmdGtkFoldviewClass *klass)
{
	gcmdgtkfoldview_signals[GCMDGTKFOLDVIEW_SIGNAL] = g_signal_new(
		"gcmdgtkfoldview",
		G_TYPE_FROM_CLASS (klass),
		(GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
		G_STRUCT_OFFSET (GcmdGtkFoldviewClass, gcmdgtkfoldview),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

//-----------------------------------------------------------------------------
//	GcmdGtkFoldview instance initialization
//-----------------------------------------------------------------------------
static void
gcmdgtkfoldview_init (GcmdGtkFoldview *foldview)
{
	foldview->control_init_instance();
}

GtkWidget*
gcmdgtkfoldview_new ()
{
    return GTK_WIDGET (g_object_new (GCMDGTKFOLDVIEW_TYPE, NULL));
}

void
gcmdgtkfoldview_clear (GcmdGtkFoldview *foldview)
{
}

