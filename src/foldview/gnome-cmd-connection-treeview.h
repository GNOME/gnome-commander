/*
    ###########################################################################

    gnome-cmd-connection-treeview.h

    ---------------------------------------------------------------------------

    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyright (C) 2010-2010 Guillaume Wardavoir

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

    ---------------------------------------------------------------------------

    Struct  : GnomeCmdConnectionTreeView

    ###########################################################################
*/
#ifndef __GNOME_CMD_CONNECTION_TREEVIEW_H__
#define __GNOME_CMD_CONNECTION_TREEVIEW_H__

#include	<config.h>

#include    <unistd.h>
#include    <sys/syscall.h>
#include    <sys/types.h>
//  ...........................................................................
#include	<glib.h>
#include	<glib-object.h>
#include	<gio/gio.h>

#include	<gtk/gtkvbox.h>
#include	<gtk/gtklabel.h>
//  ...........................................................................
#include	"gnome-cmd-includes.h"
#include	"gnome-cmd-con.h"
#include	"gnome-cmd-con-device.h"
//  ...........................................................................
#include    "gnome-cmd-foldview-utils.h"
#include    "gnome-cmd-foldview-logger.h"
#include    "gnome-cmd-foldview-quickstack.h"
#include    "gnome-cmd-foldview-treestore.h"
#include    "gnome-cmd-foldview-private.h"
//  ***************************************************************************
//								#define
//  ***************************************************************************
//  ===========================================================================
//	#define : Foldview config
//  ===========================================================================
#define GCMDGTKFOLDVIEW_ALLOW_MONITORING                    FALSE
#define GCMDGTKFOLDVIEW_USE_GIO								FALSE

#define GCMDGTKFOLDVIEW_GVFS_ITEMS_PER_NOTIFICATION			30

#define GCMDGTKFOLDVIEW_REFRESH_CYCLE_N_MESSAGES            10
#define GCMDGTKFOLDVIEW_REFRESH_CYCLE_PERIOD_MS		        200

#define GCMDGTKFOLDVIEW_SORTING_CYCLE_PERIOD_MS             60


//  ###########################################################################
//
//                          GnomeCmdConnectionTreeview
//
//  ###########################################################################
struct GnomeCmdConnectionTreeview
{
    //  ***********************************************************************
	//  *																	  *
	//  *							Enums, ...    							  *
	//  *																	  *
	//  ***********************************************************************
    public  : enum eFilePerm
	{
		// these permissions are user-relatives ( the user that launched gcmd )

		ePermNone		=   0   ,
		ePermRead		=   1   ,
		ePermWrite		=   2   ,
	};

    public:
	enum eFileAccess
	{
		eAccessFB		=   0   ,
		eAccessRO		=   1	,
		eAccessWO		=   2	,
		eAccessRW		=   3   ,

		eAccessUN		=   4
	};

    public:
    enum eAccessCheckMode
    {
        eAccessCheckNone          = (guint32)(GCMD_B8(00000000)),

        eAccessCheckClientPerm    = (guint32)(GCMD_B8(00000000)),
        eAccessCheckOwnerPerm     = (guint32)(GCMD_B8(00000000)),

        eAccessCheckAll           = (guint32)((eAccessCheckClientPerm + eAccessCheckOwnerPerm))
    };

    public  : enum eFileType
	{
		eTypeUnknown		=   0   ,

		eTypeDirectory		=   1   ,
		eTypeSymlink		=   2
	};

    public  : enum eFileError
    {
        eErrorNone                  = 0,
        // inspired from GnomeVFS
        eErrorFileNotFound          = 1,
        eErrorAccessDenied          = 2,
        eErrorCancelled             = 3,
        eErrorTimeout               = 4,
        // added from GIO


        eErrorOther                 = 1024
    };

    public  : enum eRowState
    {
        eRowStateUnknown			= 0,

        eRowStateCollapsed			= 1,
        eRowStateExpanded			= 2
    };

	typedef gchar *Uri;

    //  =======================================================================
    //  Icons
    //  =======================================================================
    public:
    enum eIcon
    {
        eIconUnknown					=  0,

        eIconDirReadWrite				=  1,
        eIconDirReadOnly				=  2,
        eIconDirForbidden				=  3,

        eIconSymlinkToDirReadWrite		=  4,
        eIconSymlinkToDirReadOnly		=  5,
        eIconSymlinkToDirForbidden		=  6,

        eIconDummy						=  7,
        eIconWarning					=  8,

        eIconGtkRefresh					=  9,

        eIconGtkSortAscending			= 10,
        eIconGtkSortDescending			= 11,

        eIconFolderOpened               = 12,
        eIconFolderClosed               = 13,

        eIconGtkClose					= 14,

        eIconCard                       = 15
    };
    //  ***********************************************************************
	//  *																	  *
	//  *							Structs     							  *
	//  *																	  *
	//  ***********************************************************************
    struct  Model;
    struct  View;
    struct  Control;

    #include    "gnome-cmd-connection-treeview-model.snippet.h"
    #include    "gnome-cmd-connection-treeview-view.snippet.h"
    #include    "gnome-cmd-connection-treeview-control.snippet.h"

    //  ***********************************************************************
	//  *																	  *
	//  *							Members     							  *
	//  *																	  *
	//  ***********************************************************************
    public:
    static  gchar   *   Collate_key_dot;			// ck of "."
    static  gchar   *   Collate_key_dotdot;			// ck of ".."
    static  gchar   *   Collate_key_uri_01;			// ck of "file:///"

    private:
    static  GdkPixbuf       *   s_gdk_pixbuf[eIconCard];
    static  gboolean            s_gdk_pixbuf_loaded;

    GnomeCmdCon             *   a_connection;
    gchar                   *   a_con_device_mount_point;
    gint                        a_con_device_mount_point_len;

    GcmdGtkFoldview         *   a_foldview;

    eAccessCheckMode            a_access_check_mode;

    GcmdStruct<Model>       *   d_model;
    GcmdStruct<View>        *   d_view;
    GcmdStruct<Control>     *   d_control;

    //  ***********************************************************************
	//  *																	  *
	//  *							Methods     							  *
	//  *																	  *
	//  ***********************************************************************
    //  =======================================================================
	//  new, ...
    //  =======================================================================
    private:
    void    *   operator new    (size_t);
    void        operator delete (void*);

    public:
     GnomeCmdConnectionTreeview(GcmdGtkFoldview*, GnomeCmdCon*);
    ~GnomeCmdConnectionTreeview();
    //  =======================================================================
	//  accessors
    //  =======================================================================
    public:
    inline  GcmdGtkFoldview *   foldview()          { return a_foldview;    }
    inline  GnomeCmdCon     *   connection()        { return a_connection;  }
    inline  Model           *   model()             { return d_model;       }
    inline  View            *   view()              { return d_view;        }
    inline  Control         *   control()           { return d_control;     }
    inline  eAccessCheckMode    access_check_mode() { return a_access_check_mode;   }
    //  =======================================================================
	//  wrappers
    //  =======================================================================
    public:
    inline  GtkWidget   *   widget()        { return d_view->widget();  }
    inline  void            show()          { d_view->show();           }
    inline  void            hide()          { d_view->hide();           }
    inline  void            update_style()  { d_view->update_style();   }
    //  =======================================================================
	//  icons
    //  =======================================================================
    private:
    static  gboolean    Pixbuf_load(GnomeCmdConnectionTreeview::eIcon, const gchar*);
    static  gboolean    Pixbuf_load_all();
    static  void        Pixbuf_unload_all();
    //  =======================================================================
	//  connection infos
    //  =======================================================================
    public:
    gboolean		is_con_device();
    gboolean		is_con_samba();
    gboolean		is_con_local();
    gboolean		host_redmond();

    GnomeCmdConDevice   *   con_device();
    const gchar*            con_device_mount_point()        { return a_con_device_mount_point;      }
          gint              con_device_mount_point_len()    { return a_con_device_mount_point_len;  }
    //  =======================================================================
	//  some stuff
    //  =======================================================================
    void            set_packing_expansion(gboolean);
    void            close();
    //  =======================================================================
	// divers
    //  =======================================================================
	static  eFileAccess		Access_from_permissions					(eFilePerm _p1, eFilePerm _p2);
	static  eFileAccess		Access_from_read_write					(gboolean _read, gboolean _write);
	static  gboolean		Access_readable							(eFileAccess);

    static  inline  void    Error_Msg_Abort()
    {
        MSG_WNG("  * Async mismatch *");
    }

    static  inline  void    Error_Msg_Failure()
    {
        MSG_WNG("  * Failure * ( Please note that failure may be normal async change, ");
        MSG_WNG("                error handling has to be improved in foldview code )");
    }


};

#endif  // __GNOME_CMD_CONNECTION_TREEVIEW_H__












