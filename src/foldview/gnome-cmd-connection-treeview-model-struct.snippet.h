/*
    ###########################################################################

    gnome-cmd-connection-treeview-model-struct.snippet.h

    ---------------------------------------------------------------------------

    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak
    Copyright (C) 2010-2011 Guillaume Wardavoir

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

    Struct  : Model

    Parent  : GnomeCmdConnectionTreeView

    This file is directly inluded in gnome-cmd-connection-treeview-model.snippet.h

    ###########################################################################
*/


//  ###########################################################################
//
//  IterInfo ( public because operator )
//
//  ###########################################################################
public:
struct IterInfo
{
    public:
    enum eFields
    {
        eNothing    = 0         ,

        eGtkPath    = 0x00000001,

        eExp        = 0x00010000,
        eRead       = 0x00100000,
        eChildren   = 0x01000000,
        eFCID       = 0x11000001
    };

    public:
    friend eFields  operator | (eFields a, eFields b);

    //.................................................................
    private:
    Model           *   a_model;

    GtkTreeIter         a_iter;
    TreestorePath   *   d_path;
    GtkTreePath     *   d_gtk_path;

    Row             *   a_row;
    gboolean            a_expanded;
    gboolean            a_readable;
    gint                a_children;
    gboolean            a_first_child_is_dummy;

    //eError              a_error;

    //.................................................................
    public:
    GtkTreeIter     *   iter()      { return &a_iter;       }
    TreestorePath   *   path()      { return d_path;        }
    GtkTreePath     *   gtk_path()  { return d_gtk_path;    }

    Row             *   row()                   { return a_row;                     }
    gboolean            expanded()              { return a_expanded;                }
    gboolean            collapsed()             { return ! a_expanded;              }
    gboolean            readable()              { return a_readable;                }
    gint32              children()              { return a_children;                }
    gboolean            first_child_is_dummy()  { return a_first_child_is_dummy;    }

    public:
    //eError      error()                         { return a_error;                   }
    //gboolean    error_logical()                 { return ( a_error & 0x01000000);   }
    void        reset();
    gboolean    gather(Model*, GtkTreeIter*,      eFields);
    gboolean    gather(Model*, TreestorePath*,    eFields);
    //.................................................................
    private:
    void*   operator new(size_t t)      { return (void*)0;  }
    void    operator delete(void* p)    {                   }

    public:
    IterInfo();
    ~IterInfo();


};
//  ###########################################################################
//  Filesystem access structs : File, Directory,  Symlink
//
//  GFile is cool, but I dont need all that stuff inside it. So I define
//  custom little structs.
//  ###########################################################################
public:
struct File
{
    private:
    gchar           *       d_ck_utf8_name; // for refresh only

    protected:
    gchar           *		d_disk_name;
    gchar           *		d_utf8_name;
    eFileAccess		        a_access;
    eFileType		        a_type;

    public:
                 File(const gchar*, eFileAccess, eFileType);
    virtual		~File() = 0;

    public:
    void                compute_ck_utf8_name()  { d_ck_utf8_name = g_utf8_collate_key_for_filename(name_utf8(), -1); }
    const gchar     *   ck_utf8_name()          { return d_ck_utf8_name;        }

    const gchar		*	name_disk()		{ return d_disk_name;	        }
    const gchar		*	name_utf8()		{ return d_utf8_name;	        }
    eFileAccess		    access()		{ return a_access;				}
    eFileType		    type()			{ return a_type;				}

    gboolean			is_symlink()	{ return ( a_type == eTypeSymlink	);  }
    gboolean			is_dir()		{ return ( a_type == eTypeDirectory	);  }

    virtual File    *   dup() = 0;

};
//.....................................................................
struct Directory : public File
{
    public:
    void		*   operator new		(size_t size);
    void			operator delete		(void* p);

    public:
                     Directory(const gchar *_utf8_display_name, eFileAccess);
    virtual         ~Directory();

    virtual File*                       dup();
};
//.....................................................................
struct Symlink : public File
{
    private:
    gchar		*   d_utf8_target_uri;

    public:
    void		*   operator new		(size_t size);
    void			operator delete		(void* p);

                     Symlink(const gchar *_utf8_display_name, const gchar *_utf8_target_uri, eFileAccess);
    virtual         ~Symlink();

    public:
    const gchar *	target_uri()		{ return d_utf8_target_uri;	}

    virtual File*                       dup();
};
//  ###########################################################################
//  TreeRow stuff
//  ###########################################################################

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  TreeRowInterface
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
private:
struct TreeRowInterface
{
    public:
    virtual ~TreeRowInterface() {}

    // flags
    public:
    virtual eIcon				    icon()				=   0;
    virtual void				    icon(eIcon)	        =   0;
    virtual gboolean				is_link()			=   0;
    virtual eFileAccess				access()			=   0;
    virtual eRowStatus				row_status()		=   0;
    virtual eRowType				rowtype()			=   0;
    virtual gboolean				is_std()			=   0;
    virtual gboolean				is_root()			=   0;
    virtual gboolean				is_dummy()			=   0;
    //..............................
    virtual gboolean				is_samba()			=   0;
    virtual gboolean				is_local()			=   0;
    virtual gboolean				host_redmond()		=   0;
    //..............................
    virtual gboolean				readable()			=   0;
    virtual void					readable(gboolean)	=   0;
    // uris, names, ...
    public:
    virtual const   gchar	*		utf8_name_display()			=   0;
    virtual const   Uri				utf8_symlink_target_uri()   =   0;
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  TreeRowStd
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
private:
struct TreeRowStd : public TreeRowInterface
{
    //.................................................................
    //  Info flags
    //  I use one gint32 to store every numeric info about a TreeRow.
    //  Thus we save memory.
    private: enum
    {
        e_ICON_SHIFT	= 0												,
        e_ICON_BITS		= GCMD_B32(00000000,00000000,00000000,00001111) ,
        e_ICON_MASK		= ~e_ICON_BITS									,

        e_LINK_SHIFT	= 4												,
        e_LINK_BITS		= GCMD_B32(00000000,10000000,00000000,00010000) ,
        e_LINK_MASK		= ~e_LINK_BITS									,

        e_ACCESS_SHIFT	= 5												,
        e_ACCESS_BITS	= GCMD_B32(00000000,00000000,00000000,11100000) ,
        e_ACCESS_MASK	= ~e_ACCESS_BITS								,

        e_STATUS_SHIFT	= 8												,
        e_STATUS_BITS	= GCMD_B32(00000000,00000000,00000111,00000000) ,
        e_STATUS_MASK	= ~e_STATUS_BITS								,

        e_TYPE_SHIFT	= 11											,
        e_TYPE_BITS		= GCMD_B32(00000000,00000000,00011000,00000000) ,
        e_TYPE_MASK		= ~e_TYPE_BITS									,

        //.............................................................
        e_SAMBA_SHIFT	= 16											,
        e_SAMBA_BITS	= GCMD_B32(00000000,00000001,00000000,00000000) ,
        e_SAMBA_MASK	= ~e_SAMBA_BITS									,

        e_LOCAL_SHIFT	= 17											,
        e_LOCAL_BITS	= GCMD_B32(00000000,00000010,00000000,00000000) ,
        e_LOCAL_MASK	= ~e_LOCAL_BITS									,

        e_REDMOND_SHIFT	= 18											,
        e_REDMOND_BITS	= GCMD_B32(00000000,00000100,00000000,00000000) ,
        e_REDMOND_MASK	= ~e_REDMOND_BITS
    };
    guint32 a_info_1;

    //.................................................................
    // other members
    protected:
    gchar								*   d_utf8_name_display;
    gchar								*   d_utf8_symlink_target_uri;

    private:
    gchar								*   d_utf8_collate_keys[eCollateKeyCard];

    //.................................................................
    // ctor, dtor
    protected:
    void	init_flags_and_collate_keys(eFileAccess, gboolean _is_link, gboolean _is_dummy, eRowStatus, eRowType, gboolean _is_samba, gboolean _is_local, gboolean _host_redmond);

    public:
             TreeRowStd();
             TreeRowStd(Row* _row, Row *_row_parent, File *_file);
    virtual ~TreeRowStd();

    //  ...............................................................
    //  partial impl ( GnomeCmdFoldviewTreestore::DataInterface )
    public:
    const   gchar						*	utf8_collate_key(gint collate_key_to_use);

    //  ...............................................................
    //  impl ( TreeRowInterace )
    public:
    virtual eIcon			    icon();
    virtual void                icon(eIcon);
    virtual gboolean			is_link();
    virtual eFileAccess			access();
    virtual eRowStatus			row_status();
    virtual eRowType			rowtype();
    virtual gboolean			is_std();
    virtual gboolean			is_root();
    virtual gboolean			is_dummy();
    //..............................
    virtual gboolean			is_samba();
    virtual gboolean			is_local();
    virtual gboolean			host_redmond();
    //..............................
    virtual gboolean			readable();
    virtual void				readable(gboolean);

    virtual const   gchar	*	utf8_name_display();
    virtual const   Uri			utf8_symlink_target_uri();
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  TreeRowRoot
//
//  Just like the struct above, but for root nodes : store a CnomeCmdCon
//  and a GnomeCmdPath so the model can retrive them from any iter
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct TreeRowRoot : public TreeRowStd
{
    private:
    ConnectionMethodID  a_con_id;

    public:
                 TreeRowRoot(
                    Uri _utf8_uri, const gchar* _utf8_display_name, Uri _utf8_symlink_target_uri,
                    eFileAccess _access, gboolean _is_symlink,
                    gboolean _is_samba, gboolean _is_local, gboolean _host_redmond);
    virtual		~TreeRowRoot();

    public:
    ConnectionMethodID	connection_id()	{ return a_con_id;	 }
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  TreeRowDummy
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct TreeRowDummy : public TreeRowStd
{
    public:
                 TreeRowDummy(Row* _row, Row *_row_parent);
    virtual		~TreeRowDummy();
};

//  ###########################################################################
//    Filesystem access structs : Core ( GIO & GnomeVFS independant )
//  ###########################################################################
struct  AsyncCallerData
{
    private:
    Model					*   a_model;
    AsyncCallerCallback			a_callback;
    TreestorePath			*   d_path;

    public:
    void*		operator new	(size_t size);
    void		operator delete (void *p);
                 AsyncCallerData(Model*, AsyncCallerCallback, TreestorePath*);
    virtual     ~AsyncCallerData();

    public:
    TreestorePath           *   path()		{ return d_path;	    }
    Model					*   model()		{ return a_model;		}
    AsyncCallerCallback			callback()	{ return a_callback;	}
};
//.............................................................................
struct AsyncCore
{
    private:
    eFileError              a_error;
    gchar               *   d_error_str;

    protected:
    AsyncCallerData		*   a_caller_data;
    gboolean                a_check_client_perm;
    gboolean                a_check_owner_perm;

    protected:
                AsyncCore(AsyncCallerData*);
    virtual		~AsyncCore();

    public:
    AsyncCallerData		*	caller_data()   { return a_caller_data; }

    public:
    void            error_set(eFileError, const gchar*);
    gboolean        error()                 { return ( a_error != eErrorNone ); }
    const gchar*    error_str()             { return d_error_str;               }

    inline  gboolean        check_client_perm() { return a_check_client_perm;   }
    inline  gboolean        check_owner_perm()  { return a_check_owner_perm;    }
};
//.............................................................................
struct AsyncGet : public AsyncCore
{
    private:
    eAccessCheckMode    a_access_check_mode;

    protected:
                AsyncGet(AsyncCallerData*);
    virtual		~AsyncGet();

    public:
    inline  void                set_access_check_mode(eAccessCheckMode _m)  { a_access_check_mode = _m;     }
    inline  eAccessCheckMode    get_access_check_mode()                     { return a_access_check_mode;   }
};
//.............................................................................
struct AsyncSet : public AsyncCore
{
};
//  ###########################################################################
//  Filesystem access structs : Get file info
//  ###########################################################################
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    Filesystem access structs : Get file info : Core ( GIO & GnomeVFS independant )
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct AsyncGetFileInfo : AsyncGet
{
    friend struct GnomeVFS;
    friend struct GIO;
    private:
    Uri                     d_uri;
    gchar           *       a_name;
    eFileType               a_type;
    eFileAccess             a_access;
    gboolean                a_is_symlink;
    Uri                     a_symlink_name;

    gboolean                b_file_not_found;

    protected:
                 AsyncGetFileInfo(AsyncCallerData*, const Uri);
    virtual	    ~AsyncGetFileInfo();

    public:
    const Uri               uri()                   { return d_uri;                 }
    const gchar *           name()                  { return a_name;                }
    eFileType               type()                  { return a_type;                }
    eFileAccess             access()                { return a_access;              }
    gboolean                is_symlink()            { return a_is_symlink;          }
    const Uri               uri_symlink_target()    { return a_symlink_name;        }
};
//  ###########################################################################
//  Filesystem access structs : enumerate children
//  ###########################################################################
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    Filesystem access structs : Enumerate : Core ( GIO & GnomeVFS independant )
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct AsyncEnumerateChildren : AsyncGet
{
    private:
    Uri                     d_uri;
    gint					a_max_result;
    gboolean				a_follow_links;

    GList				*	d_list;
    gint					a_list_card;

    protected:
                 AsyncEnumerateChildren(AsyncCallerData*, const Uri, gint _max_result, gboolean _follow_links);
    virtual	    ~AsyncEnumerateChildren();

    public:
    const Uri		uri()		            { return d_uri;			    }
    gint			max_result()		    { return a_max_result;		}
    gboolean		follow_links()		    { return a_follow_links;	}

    public:
    GList     *		list()			    { return d_list;			}
    gint			list_card()			{ return a_list_card;	    }
    void			list_append(File*);
    void			list_remove(GList*);
};
//  ###########################################################################
//   Filesystem access struct : GIO
//  ###########################################################################
private:
struct GIO
{
    public:
    static  void		Iter_enumerate_children_callback_1(GObject* , GAsyncResult *, gpointer);
    static  void		Iter_enumerate_children_callback_2(GObject* , GAsyncResult *, gpointer);

            void		iter_enumerate_children	(AsyncCallerData*, const Uri);
            void		iter_check_if_empty     (AsyncCallerData*, const Uri);
            void        iter_get_file_info      (AsyncCallerData*, const Uri);
    //.................................................................
    static  void		Monitor_callback					(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent, gpointer);
    static  gboolean	Monitor_callback_helper_query_info  (const gchar*, GObject*, GAsyncResult*, gpointer, MonitorData*, GioMonitorHelper*);
    static  void		Monitor_callback_del				(MonitorData*);
    static  void		Monitor_callback_child_del			(MonitorData*);
    static  void		Monitor_callback_child_new			(GObject* ,GAsyncResult*, gpointer);
    static  void		Monitor_callback_acc				(GObject* ,GAsyncResult*, gpointer);
    static  void		Monitor_callback_child_acc			(GObject* ,GAsyncResult*, gpointer);
};
//  ###########################################################################
//   Filesystem access struct : GnomeVFS
//  ###########################################################################
private:
struct GnomeVFS
{
    friend struct GnomeVFSMonitor;

    private:
    static  eFileAccess		Access_from_GnomeVFSFilePermissions (GnomeVFSFilePermissions, eAccessCheckMode);
    static  eFileType		Type_from_GnomeVFSFileType          (GnomeVFSFileType);
    static  eFileError      Error_from_GnomeVFSResult           (GnomeVFSResult);

    //.................................................................
    static  void        Iter_get_file_info_callback(GnomeVFSAsyncHandle*, GList*,gpointer);

    static  void		Iter_enumerate_children_callback(GnomeVFSAsyncHandle*, GnomeVFSResult, GList*, guint, gpointer);

    public:
            void		iter_enumerate_children	(AsyncCallerData*, const Uri, eAccessCheckMode);
            void		iter_check_if_empty	    (AsyncCallerData*, const Uri, eAccessCheckMode);
            void        iter_get_file_info      (AsyncCallerData*, const Uri, eAccessCheckMode);
            // no need for this one for instant
            //void        iter_get_file_info        (AsyncCallerData*, const Uri);
    //.................................................................
    private:
    static void			Monitor_callback(GnomeVFSMonitorHandle* , const gchar*, const gchar*, GnomeVFSMonitorEventType,	gpointer);
    static  void		Monitor_callback_del				(MonitorData*);
    static  void		Monitor_callback_child_del			(MonitorData*, const Uri);
    static  void		Monitor_callback_child_new			(GnomeVFSAsyncHandle*, GList*, gpointer);
    //static  void		Monitor_callback_acc				(GnomeVFSAsyncHandle*, GList*, gpointer);
    static  void		Monitor_callback_child_acc			(GnomeVFSAsyncHandle*, GList*, gpointer);
};
//  ===================================================================
//  Filesystem access structs : Monitoring
//  ===================================================================
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Filesystem access structs : Monitoring : Core ( GIO & GnomeVFS independant )
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
private:
struct MonitorInterface
{
    public:
    virtual			~MonitorInterface()					{};
    virtual gboolean monitoring_start(Model*, Row*)		= 0;
    virtual gboolean monitoring_stop()					= 0;
    virtual gboolean monitoring_started()				= 0;
};
//.....................................................................
private:
struct Monitor : public MonitorInterface
{
    protected:
    gboolean								a_started;

    public:
             Monitor();
    virtual ~Monitor();

    virtual gboolean	monitoring_start(Model*, Row*)  = 0;
    virtual gboolean	monitoring_stop()				= 0;
    virtual	gboolean	monitoring_started();
};
//.....................................................................
private:
struct MonitorData
{
    public:
    Model		*   a_model;
    Row			*   a_row;

    //MonitorData(Row*);
    //~MonitorData();
};
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Filesystem access structs : Monitoring : GnomeVFS
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct GnomeVFSMonitor : public Monitor
{
    public:
    void		operator delete (void   *   p);
    void	*   operator new	(size_t		size);

                 GnomeVFSMonitor();
    virtual		~GnomeVFSMonitor();

    private:
    GnomeVFSMonitorHandle				*   a_monitor_handle;

    public:
    GnomeVFSMonitorHandle				*   monitor_handle()		{ return a_monitor_handle;	}
    GnomeVFSMonitorHandle				**  monitor_handle_ptr()	{ return &a_monitor_handle;	}

    gboolean	monitoring_start(Model*, Row*);
    gboolean	monitoring_stop();
};
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Filesystem access structs : Monitoring : GIO
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct GioMonitor : public Monitor
{
    public:
    void		operator delete (void   *   p);
    void	*   operator new	(size_t		size);

                 GioMonitor();
    virtual		~GioMonitor();

    private:
    GFile								*   d_file;			// we store this only for exiting
    GFileMonitor						*   d_monitor;		// gcmd properly, it is a mess
    gulong									a_signal_handle;

    public:
    GFile								*	gfile()		{ return d_file;			}
    GFileMonitor						*   monitor()   { return d_monitor;			}

    gboolean	monitoring_start(Model*, Row*);
    gboolean	monitoring_stop();
};
//  ===================================================================
//  Row
//  ===================================================================
public: // public becoz of View::Treeview
struct Row :	public  GnomeCmdFoldviewTreestore::DataInterface,
                public  TreeRowInterface,
                public  MonitorInterface
{
    private:
    TreestorePath				*   d_path;
    Uri								d_utf8_uri;

    TreeRowInterface			*   d_treerow;
    MonitorInterface			*   d_monitor;

    public:
     Row(eRowType _rowtype, Row *_row_parent, File *_file);
     Row(eRowType _rowtype, Uri _utf8_uri, const gchar* _utf8_display_name, Uri _utf8_symlink_target_uri,
        eFileAccess _access, gboolean _is_symlink,
        gboolean _is_samba, gboolean _is_local, gboolean _host_redmond);
     Row(eRowType _rowtype, Row *_row_parent);
    ~Row();

    //  ...............................................................
    // Accessors
    public:
    inline			TreeRowInterface	*   treerow()		{ return (TreeRowInterface*)d_treerow;	    }

    inline			TreeRowStd			*	treerow_std()   { return (TreeRowStd*)d_treerow;			}

    inline			TreeRowRoot			*	treerow_root()
                                            {
                                                g_return_val_if_fail( treerow()->is_root(), NULL );
                                                return (TreeRowRoot*)d_treerow;
                                            }
    inline			TreeRowDummy		*	treerow_dummy()
                                            {
                                                g_return_val_if_fail( treerow()->is_dummy(), NULL );
                                                return (TreeRowDummy*)d_treerow;
                                            }
    inline			MonitorInterface	*   monitor()		{ return (MonitorInterface*)d_monitor;	    }

    inline  const	Uri						uri_utf8()		{ return d_utf8_uri;						}
    inline  const   gchar			    *	utf8_collate_key(gint collate_key_to_use)
                                            {
                                                return ((TreeRowStd*)treerow())->utf8_collate_key(collate_key_to_use);
                                            }
    //  ...............................................................
    //  impl ( GnomeCmdFoldviewTreestore::DataInterface )
    public:
            virtual         gint				compare     (DataInterface*);
            virtual         gint				compare_str (const gchar*);

    inline  virtual			TreestorePath	*	path()      { return d_path;                            }
            virtual			void				set_path_from_treestore(GnomeCmdFoldviewTreestore::Path*);
    //  ...............................................................
    //  impl ( TreeRowInterface ) -> aggregation wrappers
    //  -> flags
    public:
    virtual eIcon		    icon()						{ return treerow()->icon();				}
    virtual void            icon(eIcon _icon)           { treerow_std()->icon(_icon);           }
    virtual gboolean		is_link()					{ return treerow()->is_link();			}
    virtual eFileAccess		access()					{ return treerow()->access();			}
    virtual eRowStatus		row_status()				{ return treerow()->row_status();		}
    virtual eRowType		rowtype()					{ return treerow()->rowtype();			}
    virtual gboolean		is_std()					{ return treerow()->is_std();			}
    virtual gboolean		is_root()					{ return treerow()->is_root();			}
    virtual gboolean		is_dummy()					{ return treerow()->is_dummy();			}
    //..............................
    virtual gboolean		is_samba()					{ return treerow()->is_samba();			}
    virtual gboolean		is_local()					{ return treerow()->is_local();			}
    virtual gboolean		host_redmond()				{ return treerow()->host_redmond();		}
    //..............................
    virtual gboolean		readable()					{ return treerow()->readable();			}
    virtual void			readable(gboolean _b)		{	     treerow()->readable(_b);		}
    //  -> uris, names, ...
    public:
    virtual const gchar	*	utf8_name_display()			{ return treerow()->utf8_name_display();		}
    virtual const Uri		utf8_symlink_target_uri()   { return treerow()->utf8_symlink_target_uri();  }

    //  ...............................................................
    //  impl ( MonitorInterface  -> aggregation wrappers
    virtual gboolean		monitoring_start(Model*, Row*);
    virtual gboolean		monitoring_stop();
    virtual gboolean		monitoring_started();
};
//  ===================================================================
//                      Messages structs
//  ===================================================================
public:
enum eMsgType
{
    eAddDummyChild      =    1,
    eAddChild           =    2,
    eAddFirstTree       =    3,

    eDel                =   10,

    eSetReadable        =   20,
    eSetNotReadable     =   21,

    eSort               =   30,

    eAsyncMismatch          =   100,
    eAsyncMismatchIEFUC     =   101,
    eAsyncMismatchICIEC     =   102,
    eAsyncMismatchAFT       =   103
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgCore
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgCore
{
    private:
    TreestorePath	    *   d_path;

    public:
    virtual eMsgType                type()  = 0;
            TreestorePath       *   path()      { return d_path;        }

    public:
                        MsgCore(TreestorePath* _path)
                        {
                            d_path = _path ? _path->dup() : NULL;
                        }
    virtual             ~MsgCore()
                        {
                            if ( d_path )
                                delete d_path;
                        }
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgAddDummyChild
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgAddDummyChild         : public MsgCore
{
    public:
    virtual eMsgType                type()  { return eAddDummyChild;     }

    public:
                         MsgAddDummyChild(TreestorePath* _path) : MsgCore(_path)    {}
    virtual             ~MsgAddDummyChild()                                         {}
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgAddChild
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgAddChild              : public MsgCore
{
    File            *   d_file;

    public:
    virtual eMsgType                type()  { return eAddChild;     }
            File        *           file()  { return d_file;        }
    public:
                         MsgAddChild(TreestorePath* _path, File* _file) : MsgCore(_path)
                         {
                            d_file = _file->dup();
                         }
    virtual             ~MsgAddChild()
    {
        delete d_file;
    }
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgAddFirstTree
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgAddFirstTree          : public MsgCore
{
    private:
    File            *   d_file;

    public:
    virtual eMsgType                type()  { return eAddFirstTree; }
            File        *           file()  { return d_file;        }
    public:
                        MsgAddFirstTree(TreestorePath* _path, File* _file) : MsgCore(_path)
                        {
                           d_file = _file->dup();
                        }
    virtual             ~MsgAddFirstTree()
                        {
                           delete d_file;
                        }
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgDel
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgDel                   : public MsgCore
{
    public:
    virtual eMsgType                type()  { return eDel;    }

    public:
                         MsgDel(TreestorePath* _path) : MsgCore(_path)  {}
     virtual            ~MsgDel()                                       {}
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgSetReadable
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgSetReadable           : public MsgCore
{
    public:
    virtual eMsgType                type()  { return eSetReadable;  }

    public:
                         MsgSetReadable(TreestorePath * _path) : MsgCore(_path) {}
    virtual             ~MsgSetReadable()                                       {}
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgSetReadable
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgSetNotReadable        : public MsgCore
{
    public:
    virtual eMsgType                type()  { return eSetNotReadable;  }

    public:
                         MsgSetNotReadable(TreestorePath * _path) : MsgCore(_path)  {}
    virtual             ~MsgSetNotReadable()                                        {}
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgAsyncMismatchIEFUC
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgAsyncMismatchIEFUC    : public MsgCore
{
    public:
    virtual eMsgType                type()  { return eAsyncMismatchIEFUC;  }

    public:
                         MsgAsyncMismatchIEFUC(TreestorePath * _path) : MsgCore(_path)  {}
    virtual             ~MsgAsyncMismatchIEFUC()                                        {}
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgAsyncMismatchICIEC
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgAsyncMismatchICIEC    : public MsgCore
{
    public:
    virtual eMsgType                type()  { return eAsyncMismatchICIEC;  }

    public:
                         MsgAsyncMismatchICIEC(TreestorePath * _path) : MsgCore(_path)  {}
    virtual             ~MsgAsyncMismatchICIEC()                                        {}
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgAsyncMismatchAFT
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgAsyncMismatchAFT    : public MsgCore
{
    public:
    virtual eMsgType                type()  { return eAsyncMismatchAFT;  }

    public:
                         MsgAsyncMismatchAFT(TreestorePath * _path) : MsgCore(_path)  {}
    virtual             ~MsgAsyncMismatchAFT()                                        {}
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  IMsgSort
//
//  Interface that must implement any sort msg struct
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct IMsgSort                 : MsgCore
{
    //.................................................................
    // automat
    //
    private:
    enum eStep
    {
        eInit       =  0,
        eStart      = 10,
        eRun        = 20,
        eRestart    = 30,
        eDone       = 40,
        eAbort      = 50,
        eNop        = 60
    };

    private:
    eStep           a_step;
    gint            a_iteration;
    gboolean        a_initialized;
    gboolean        a_error;
    gboolean        a_done;

    public:
            gboolean    run();
            gboolean    spread();
    inline  gboolean    done()      { return a_done;    }
    inline  gboolean    error()     { return a_error;   }
    //.................................................................
    // lists
    //
    private:
    GList       *   d_list_original;
    guint           a_list_card;

    protected:
    GList       *   d_list_actual;
    //.................................................................
    private:
    gboolean            list_get(GList ** _list);
    gboolean            list_changed();

    protected:
    inline  guint       list_card()         { return a_list_card;       }
            void        list_delete(GList**);
            void        list_original_copy(GList** _dest);

    //.................................................................
    // divers
    //
    private:
    Model       *   a_model;
    //.................................................................
    private:
    inline  Model   *   model()         { return a_model;       }

    public:
    virtual eMsgType    type()          { return eSort;         }

    public:
    virtual     gboolean        init()     = 0;
    virtual     gboolean        step()     = 0;
    virtual     gboolean        restart()  = 0;

    public:
                         IMsgSort(TreestorePath* _path, Model*);
     virtual            ~IMsgSort();
};
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  MsgSort_Insertion
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MsgSort_Insertion : public IMsgSort
{
    private:
    guint           a_i0;
    GList       *   d_list_work;


    public:
    virtual     gboolean        init();
    virtual     gboolean        step();
    virtual     gboolean        restart();

    public:
                         MsgSort_Insertion(TreestorePath*, Model*);
     virtual            ~MsgSort_Insertion();
};

