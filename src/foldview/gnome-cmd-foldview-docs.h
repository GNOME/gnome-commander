/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak
    Copyleft      2010-2010 Guillaume Wardavoir

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

/*


###############################################################################
								TODO GWR
###############################################################################

		GcmdFileSelector resize automatically resize the main vertical pane
		( FILL / EXPAND gtk settings somewhere ) ?

		Trivial bug as no item is selectionned on foldview 'open in active tab'

        Check all g_list_append / prepend, doc says it works with NULL pointers

        Effective sorting in GtkTreeView

        Test with GIO

        Show broken symlinks

        Broken access for gnome_cmd_con_device connections : seems that is
        a bug in gcmd code

        Is sorting realy useful ?

        Set_connection on active pane fail as it seems there is no active pane
        at the start of the program !

		..................... facultative  / later ............................

		sync file_list when it is not active ( in a hidden tab )

		Bookmarks

		Foldview metadata ( ex : foldview visibility )

        Improve GnomeCmdFoldviewTreestore::Node access by introducing
        depth info in uids ? seems impossible with actual code...

		.......................................................................
DONE    20.12.2010
        fixed pb gnome_cmd_file_get_path in gnome-cmd-file.cc

DONE    20.12.2010
        removed sort button, sort is not yet effective and confuse users

DONE    19.12.2010
        File permissions on ftp connections :
            - owner perms instead of client perms
            - root folder has permissions 0, unable to browse...

DONE    Separators between multiples trees
        -> headband above treeview control

DONE    ReEnable double-click


DONE    _GWR_BUG_#01_

DONE    Notification from filesystem, GIO based ( half coded )
			- GIO bug : no self EVENT_DELETE after async op has been
			  done on a monitored GFile
            - There is another similar bug too
        -> Monitorig development freezed, I want foldview to be lenny-compatible

DONE    AsyncCore errors : detail & display in async callbacks
        -> made with log enhance

DONE    Log enhance

DONE    AsyncCore errors : use eIconWarning

DONE    - corrected stupid & critical bug in GnomeCmdFoldviewTreestore::iter_next
        - refresh seems to be OK
        - monitoring develeoppement stopped ( although quite complete )

DONE    Refresh is implemented, monitoring is on stand-by till a decision
        is taken relatively to GnomeVFS / GIO bugs in Debian Lenny

		.......................................................................
COMMIT  2a1a27202012327d1da9d20e7deb9892a9e9028d

DONE	foldview as a static lib, libfoldview.a - its the only way I found to
		put foldview sources in a separate directory

		.......................................................................
COMMIT  a406809dfa5cbcc1b9928cb45695bb1928df1578

DONE	Notification from filesystem, GnomeVFS based - * it works *

DONE	Focus on read permission, deleted write permission display in foldview

DONE	A little bit of conception for data stored in the treestore

DONE	Split model filesystem access in 2 modules : GIO & GnomeVFS

DONE	model : replaced 'struct Refresh'  by 'struct AsyncCBData'
		( -> removing GcmdFoldview() calls )

DONE	moved paths into GnomeCmdFoldviewTreestore

DONE	moved uids into GnomeCmdFoldviewTreestore

DONE	all pre-requisite code for refresh

DONE	reduce number of gnome_vfs_uri_new() calls

DONE	removed struct UserLoadSubdir

DONE	moved control code in model for expand, collapse, check if empty

DONE	'match' : coded match uid for childs

		.......................................................................
COMMIT  e6eb3e8ab1ed1ea37cdece1e4e835a4a583927e1

DONE	'match' : nearly coded, missing match code in GnomeCmdFoldviewTreestore

DONE	'refresh' : coded until async_ops

DONE	'delete root node' : Finished

		.......................................................................
COMMIT  db15c04d3067ec023bcbe6eee0c86690f4becc86

done	coded 'delete root node', popup menu only

DONE	eIconUnknown on first add_tree, when creating the foldview

DONE	removed GLib warning from gvalue.c, missing a g_value_unset

DONE	code cleanup

		.......................................................................
COMMIT  9ae7289de1ee43f50ef45cdda5d08a2b6ab9f876

DONE	changed copyright to copyleft for me, and verified GPL headers for
		foldview modules

DONE	implemented multiple root nodes ( just like nautilus ), which will
		lead to opening bookmarks in foldview

DONE	removed #define __GTS__ , bye bye GtkTreeStore

		.......................................................................
COMMIT  c3e79b41c22091e3e6452130cff1a04948f428d8

DONE	init / destroy cleanup : g_object_ref on hided widgets

DONE	memory leaks hunt

DONE	memory leak : gnome-cmd-foldview-control.cc::814 forgot g_free(str)

DONE	memory leak : root node

DONE	memory leak : GcmdGtkFldview::Model::destroy implemented

DONE	memory leak : gvfs quickstack

DONE	correct ESC[30m for terminal after each gwr_print

		.......................................................................
COMMIT  05bb32e2cfd8eb56bf64e93bc1785ce52f6a100d

DONE	symlinks icons show little white pixel when selected its ugly
		but gcmd's too

DONE	foldview correct position on show / hide ( cmdline, buttonbar, ... )

DONE	(Finally) gcmd crashes when opening access-denied folder from treeview
		in tab, after showing an alert

DONE	Going crazy with correct theming of treeview with selectionned items,
		focus between widgets,...

DONE	combo connections at startup

DONE	gtk_widget_set_sensitive on non-full-functional items of context menu

DONE	Check index_ff on qstack_push()

DONE	? mutex on handles : no, gvfs_async calls are queued
		and since I use gdk_threads_enter/leave, there is no more
		concurrency between gtk & gvfs

DONE	why GVFS_qstack_ff / GVFS_qstack_size  is always n-2 / n ??? should be n / n
		-> because t' affiches ff _avant_ update et qsize au lieu de qsize - 1,
		stupid, ça fait un décalage de 2

DONE	Comments cleanup

DONE	Private logging for gvfs ops

DONE	Cancel all async ops

DONE	Big Bug in checking type of GnomeVFSFileInfo : GNOME_VFS_FILE_TYPE_...
		are distinct values - it is not a bitfield, triple fuck, spended time
		on that

###############################################################################
								TODO EPIOTR
###############################################################################

DONE	in src/gnome-cmd-foldview.h:
        extern		gboolean			gnome_cmd_foldview_set_root_1(gchar *text);
		...
        extern		GtkWidget		*GcmdWidget();
        'extern' is superfluous here, please remove it

DONE	epiotr.10.07.16.gcmd-foldview
DONE	ua

		epiotr.10.07.22.kill-warnings
DONE	epiotr.10.07.22.gtk_object_ref,unref

DONE	epiotr.10.07.24.000? struct-gvfs_async_caller_data
DONE	epiotr.10.07.24.000? struct-gvfs-async
DONE	epiotr.10.07.24.000? struct-gvfs-async-load-subdirs
DONE	epiotr.10.07.24.000? struct-gvfs-async-load-subdirs-subdirs


DONE	* I've moved view_treeview() to gnome-cmd-user-actions.cc. I'm
		sending you a patch for that.

DONE	* after opening foldview, cmdline is moved from bottom to top -
		that requires fixing

DONE	* move foldview code to src/foldview

DONE	* changing your surname from WARDAVOIR -> Wardavoir (this is GNOME
        standard) ;o)

DONE	* remove singleton

		* foldview should respect global settings for case sensitive
		sorting (gnome_cmd_data.case_sens_sort) and showing hidden dirs
		(gnome_cmd_data.filter_settings.hidden)

		* when folview is opened for the first time, it should initially
		expand to the current dir of active pane:
		main_win->fs(ACTIVE)->get_directory()

		* making your 'earlgrey' repo based on current 'treeview' branch

DONE	* please do not fix bugs in 'core' gcmd in 'earlgrey' repo, they
        may go unnoticed... Please send them directly to me or to
        gcmd-devel list, they will go into into master and then will get
        merged into treeview

###############################################################################
					WIDGET HIERARCHY FOR GcmdGtkFoldview
###############################################################################

	Sample for "horizontal view" ( all widgets are under main_win->priv->... )

	+-----------------------------------------------------------------------+
	|																		|
	|	main_win->priv->vbox												|
	|																		|
	|	+-------------------------------+-------------------------------+	|
	|	|								|								|   |
	|	|	foldview_paned				|	foldview_paned				|   |
	|	|	( pane #1 )					|	( pane #2 )					|   |
	|	|								|								|   |
	|	|	+-----------------------+	|	+-----------------------+	|   |
	|	|	|						|	|	|						|   |   |
	|	|	|	foldview			|	|	|	paned ( pane #1 )	|   |   |
	|	|	|						|	|	|						|   |   |
	|	|	|						|	|	|	+---------------+	|   |   |
	|	|	|						|	|	|	|				|   |   |   |
	|	|	|						|	|	|	| GnomeCmdList  |   |   |   |
	|	|	|						|	|	|	|				|   |   |   |
	|	|	|						|	|	|	+---------------+   |   |   |
	|	|	|						|	|	|						|   |   |
	|	|	|						|	|	+-----------------------+   |   |
	|	|	|						|	|	|						|   |   |
	|	|	|						|	|	|	paned ( pane #2 )	|   |   |
	|	|	|						|	|	|						|   |   |
	|	|	|						|	|	|	+---------------+	|   |   |
	|	|	|						|	|	|	|				|   |   |   |
	|	|	|						|	|	|	| GnomeCmdList  |   |   |   |
	|	|	|						|	|	|	|				|   |   |   |
	|	|	|						|	|	|	+---------------+   |   |   |
	|	|	|						|	|	|						|   |   |
	|	|	+-----------------------+	|	+-----------------------+	|   |
	|	|								|								|   |
	|	|								|								|   |
	|	+-------------------------------+-------------------------------+	|
	|																		|
	+-----------------------------------------------------------------------+


###############################################################################
						GTK+ DRAG & DROP SYNOPSIS
###############################################################################


		"drag-begin"									SRC

		"drag-motion"												DST
			|
			|
			+-----------------------+
			|						|
			|						|
		"drag-drop"				"drag-leave"						DST
			|
			|
		"drag-data-get"									SRC
			|
			|
		"drag-data-received"										DST
			|
			|
		"drag-end"										SRC


	+   "drag-failed"									SRC
		"drag-data-delete"								SRC




###############################################################################
                            TEMPORARY TRASH-CODE
###############################################################################

//  ===========================================================================
//  iter_collapse()
//  ===========================================================================
/*
gboolean
GcmdGtkFoldview::Model::iter_collapse(
	GtkTreeIter *   _iter)
{
	gint					n			= 0;
	View::eRowState			state		= View::eRowStateUnknown;
	GtkTreePath			*   gtk_path	= NULL;
	GtkTreeIter				iter_dummy  = Iter_zero;
	//.........................................................................
	//GCMD_INF("iter_remove");

	// get the # of children
	n = iter_n_children(_iter);

	// get the GtkTreePath
	gtk_path = GnomeCmdFoldviewTreestore::get_path(treemodel(), _iter);
	if ( ! gtk_path )
		return FALSE;

	// get the view state of the parent
	state = view()->row_state(gtk_path);

	//.........................................................................
	// iter is sterile : exit
	if (  n == 0 )
		goto exit_success;


	//.........................................................................
	// iter is fertile, and is expanded
	if ( state == View::eRowStateExpanded )
	{
		// It is a hack ! Let GtkTreeView update its cache by collapsing !
		// We dont have to manually delete each row and signals on them !
		if ( ! view()->row_collapse(gtk_path) )									// __GWR__HACK__
		{
			GCMD_ERR("Model::iter_collapse():iter could not be collapsed");
			goto exit_failure;
		}
		// We dont return here, since the iter is in the following state !
	}

	//.........................................................................
	// iter is fertile, and is collapsed. Silently remove all children.
	iter_collapsed_remove_children(_iter);

	iter_dummy_child_add(_iter, &iter_dummy);

exit_success:
	gtk_tree_path_free(gtk_path);
	return TRUE;

exit_failure:
	gtk_tree_path_free(gtk_path);
	return FALSE;
}
*/

//  ===========================================================================
//  Add a File as a child of an iter, if the file match
//  criterias
//  ===========================================================================
/*
gboolean
GcmdGtkFoldview::Model::iter_files_add_file(
	GtkTreeIter		*   _iter_parent,
	FoldviewFile	*   _file,
	gboolean			_check_for_doubloon,
	gboolean			_replace_first_child)
{
	gchar				*		utf8_collate_key_file   = NULL;
	GtkTreeIter					iter_child				= Iter_zero;
	Row					*		row_parent				= NULL;
	Row					*		row_child				= NULL;
	//.........................................................................
	g_return_val_if_fail( _iter_parent,		FALSE );
	g_return_val_if_fail( _file,			FALSE );

	// filter
	if ( ! iter_files_file_filter(_file) )
		return FALSE;

	// doubloons
	if ( _check_for_doubloon )
	{
		utf8_collate_key_file = collate_key_new_from_utf8(_file->name_utf8());

		if ( treestore()->ext_match_child_collate_key(_iter_parent, &iter_child, eCollateKeyRaw, utf8_collate_key_file) )
		{
			g_free(utf8_collate_key_file);
			return FALSE;
		}

		g_free(utf8_collate_key_file);
	}

	row_parent = iter_get_treerow(_iter_parent);
	if ( ! row_parent )
		return NULL;

	// build the TreeRow
	row_child = new Row(eRowStd, row_parent, _file);

	// add it !
	if ( _replace_first_child && ( GnomeCmdFoldviewTreestore::iter_n_children(treemodel(), _iter_parent) == 1 ) )
		iter_dummy_child_replace(_iter_parent, &iter_child, row_child);
	else
		iter_add_child(_iter_parent, &iter_child, row_child);

	return TRUE;
}
*/

//  ===========================================================================
//  Add files from a GList of Files as childs of iter
//  ===========================================================================
/*
void
GcmdGtkFoldview::Model::iter_files_add_files(
	GtkTreeIter				*   _iter_parent,
	GList					*   _list,
	gboolean					_check_for_doubloon,
	gboolean					_replace_dummy)
{
	GList					*		list			= NULL;
	FoldviewFile			*		file			= NULL;

	GtkTreeIter						iter_child		= GcmdGtkFoldview::Model::Iter_zero;
	//.........................................................................

	// add first children
	list	= g_list_first(_list);
	g_return_if_fail(list);

	file	= (File*)list->data;
	FILES_INF("Model::ifafs:%s", file->name_utf8());
	iter_files_add_file(_iter_parent, file, _check_for_doubloon, _replace_dummy);
	list	= g_list_next(list);

	// add other children
	while ( list )
	{
		file	= (File*)list->data;

		FILES_INF("Model::ifafs:%s", file->name_utf8());
		iter_files_add_file(_iter_parent, file, _check_for_doubloon, FALSE);

		list	= g_list_next(list);
	}
}
*/


		//  ===================================================================
		//  Refresh
		//  ===================================================================
/*
		private:
		struct Refresh
        {

            private:
            TreestorePath   *   d_path;
            Uri                 d_uri;

			public:
			void*		operator new	(size_t size);
			void		operator delete (void *p);
                         Refresh(TreestorePath*, const gchar* = NULL);
                        ~Refresh();

            public:
            const Uri           uri()   { return d_uri;     }
            TreestorePath   *   path()  { return d_path;    }

        };
		//.....................................................................
        struct RefreshList
        {
            private:
            Model   *   a_model;
            GList   *   d_list;
            GList   *   a_current;

			public:
			void*		operator new	(size_t size);
			void		operator delete (void *p);
                         RefreshList(Model*, GList*);
                        ~RefreshList();

            public:
            Model       *   model();
            void            add(Refresh*);
            void            reset();
            Refresh     *   getplusplus();
        };
*/
//=============================================================================
//  Model::iter_refresh_store_R()
//=============================================================================
//
//  Recursive call for getting all subiters of an iter in the form
//  of a GList of Model::Refresh
//
/*
GList*
GcmdGtkFoldview::Model::iter_refresh_store_R(
    GList       *   _list,
    GtkTreeIter *   _iter_parent)
{
    TreestorePath   *   path        = NULL;
	Refresh			*   refresh	    = NULL;
	GtkTreeIter		    iter_child	= Iter_zero;

	GList			*   l			= NULL;

    Row             *   row         = NULL;
	//.........................................................................
    g_return_val_if_fail( _iter_parent, NULL );

	l = _list;

    //
	//  add a new Refresh to the list
    //
    path    = treestore()->ext_path_from_iter(_iter_parent);

    // treerow
    row = iter_get_treerow(_iter_parent);
    if ( ! row )
    {
        REFRESH_ERR("Model::iter_refresh_store_R():Could not get treerow for iter");
        delete path;
        return l;
    }

    if ( ! row->is_dummy() )
    {

        refresh = new Refresh(path, row->uri_utf8());
        l       = g_list_append(l, (gpointer)refresh);

        //
        //  children
        //

        // no children, return
        if ( ! GnomeCmdFoldviewTreestore::iter_children(treemodel(), &iter_child, _iter_parent) )
            return l;

        do
        {
            l = iter_refresh_store_R(l, &iter_child);
        }
        while ( GnomeCmdFoldviewTreestore::iter_next(treemodel(), &iter_child));

    }

	return l;
}
*/


//  ===========================================================================
//  Iter_refresh_launch_async_ops()
//  ===========================================================================
//
//  Launch the async ops at regular interval, until all async ops have been
//  launched.
//
//  The RefreshList has to be reseted before the first call, by the caller.
/*
gboolean
GcmdGtkFoldview::Model::Iter_refresh_launch_async_ops(
    gpointer    _data)
{
    RefreshList     *       rlist   = NULL;
    Model           *       THIS    = NULL;

    Refresh			*       refresh	= NULL;
    AsyncCallerData *       acd     = NULL;

    gint					j		= 0;
	//.........................................................................
    rlist   = (RefreshList*)_data;
    THIS    = rlist->model();

loop:
    // continue from we stopped
    refresh = rlist->getplusplus();

	// end of list reached, aborting g_timeout by returning FALSE
	if ( ! refresh )
	{
		// delete the list, mallocated datas are destroyed in callbacks
		delete rlist;
		return FALSE;
	}

	REFRESH_INF("Model::Iter_refresh_launch_async_ops():Launching async ops on %s", refresh->uri());

    acd = new AsyncCallerData(rlist->model(), Iter_refresh_children_callback, refresh->path()->dup());

	_USE_GIO_                                                               ?
		THIS->a_GIO.iter_enumerate_children     (acd, refresh->uri())       :
		THIS->a_GnomeVFS.iter_enumerate_children(acd, refresh->uri())       ;

    // refresh is not useful anymore
    delete refresh;

	// cycle limit reached, return
	if ( ( ++j ) == GCMDGTKFOLDVIEW_REFRESH_CYCLE_N_ITEMS )
		return TRUE;

	goto loop;
}
*/

/*
void
GcmdGtkFoldview::Model::iter_refresh_children(
	GtkTreeIter *   _iter)
{
	GList		*	list	=   NULL;
	GList		*	temp	=   NULL;
    RefreshList *   rlist   =   NULL;
	//.........................................................................
	// get all iters in a list
	//
	list = iter_refresh_store_R(list, _iter);

	// tree is empty !
	if ( !list )
	{
		REFRESH_INF("Model::iter_refresh():called on an empty tree");
		return;
	}

	list = g_list_first(list);

	// dump !
	REFRESH_INF("Model::iter_refresh():dumping items");
	temp = list;
	do
	{
		REFRESH_INF("  |  %s", ((Refresh*)temp->data)->uri() );
		temp = g_list_next(temp);
	}
	while ( temp != NULL );

	//.........................................................................
	// launch async ops - we use a timer, thus gcmd can 'digest' the list
	//

    // RefreshList
    rlist = new RefreshList(this, list);
    rlist->reset();

	//  guint   g_timeout_add(
	//			guint			interval,
	//			GSourceFunc		function,
	//			gpointer		data);
	//
	//  interval	:	the time between calls to the function, in milliseconds (1/1000ths of a second)
	//  function	:	function to call
	//  data		:	data to pass to function
	//  Returns		:	the ID (greater than 0) of the event source.
	//
	//  gboolean (*GSourceFunc) (gpointer data);

	// launch the timer
	g_timeout_add(
        GCMDGTKFOLDVIEW_REFRESH_CYCLE_PERIOD_MS,
        Iter_refresh_launch_async_ops,
        (gpointer)rlist);
}
*/

/*
//  ===========================================================================
//  Iter_monitor_callback_acc()
//  ===========================================================================
//
//  Monitoring callback : directory access changed
//
void
GcmdGtkFoldview::Model::Iter_monitor_callback_acc(
	MonitorData		*	_md,
	eFileAccess			_access,
	const gchar		*   _name_debug)
{
	GcmdGtkFoldview::Model				*   THIS			= NULL;
	GnomeCmdFoldviewTreestore			*   treestore		= NULL;
	GtkTreeModel						*   treemodel		= NULL;
	Row									*   row				= NULL;

	GtkTreeIter								iter			= Iter_zero;
	gboolean								was_readable	= FALSE;
	gboolean								is_readable		= FALSE;
	//.........................................................................
	MONITOR_INF("Model::Iter_monitor_callback_acc()");

	THIS		= _md->a_model;
	row			= _md->a_row;

	treestore   = THIS->treestore();
	treemodel   = THIS->treemodel();

	gdk_threads_enter();														// _GDK_LOCK_

	//.........................................................................
	//
	//  Step #1 : Get iter + data
	//
	if ( ! treestore->ext_iter_from_path(row->path(), &iter) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_acc():could not retrieve iter from Path [%s]", _name_debug);
		goto exit;
	}

	if ( ! treestore->ext_get_data(&iter, (GnomeCmdFoldviewTreestore::DataInterface**)&row) )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_acc():could not retrieve parent iter's TreeRow [%s]", _name_debug);
		goto exit;
	}

	//.........................................................................
	//
	//  Step #2 : verification that file access has changed
	//
	if ( row->access() == _access )
	{
		MONITOR_ERR("Model::Iter_monitor_callback_child_acc():access didnt change [%s]", row->utf8_name_display());
		goto exit;
	}

	//.........................................................................
	//
	//  Step #3 : convenience booleans
	//
	was_readable = row->readable();
	is_readable	 = Access_readable(_access);

	//.........................................................................
	//
	//  Step #4 : stuff
	//

	//  4.1 readable -> NOT readable
	if ( was_readable && ( !is_readable ) )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_acc():readable -> NOT readable [%s]", row->utf8_name_display());
	}

	//  4.2 NOT readable -> readable
	if ( ( !was_readable ) && is_readable )
	{
		MONITOR_INF("Model::Iter_monitor_callback_child_acc():NOT readable -> readable [%s]", row->utf8_name_display());
	}

	//  4.3 other changes, exit

exit:
	gdk_threads_leave();														// _GDK_LOCK_
}
*/

/*
//  ===========================================================================
//  iter_readable_set()
//  ===========================================================================
//
//  Set an iter readable
//  ( do logic verifications & launch check_if_empty )
//
gboolean
GcmdGtkFoldview::Model::iter_readable_set(
	GtkTreeIter * _iter)
{
    IterInfo                info;
	//.........................................................................
	//GCMD_INF("iter_readable_set");
	g_return_val_if_fail( _iter, FALSE );

	//.........................................................................
	// Logic verifications
    //

    info.gather(this, _iter, IterInfo::eExp | IterInfo::eChildren);
    // iter must be collapsed, and have no child
    if ( ! info.collapsed() || info.children() )
    {
		GCMD_ERR("Model::iter_readable_set():Test failed");
		return FALSE;
	}

	// iter already readable : return
	if ( info.row()->readable() )
	{
		GCMD_WNG("Model::iter_readable_set():already readable [%s]", info.row()->utf8_name_display());
		return TRUE;
	}

	//.........................................................................
	// All is ok

	// change data
	info.row()->readable(TRUE);

	// inform treview that row has changed
	treestore()->ext_data_changed(_iter);

	// launch check if empty
	iter_check_if_empty(_iter);

	return TRUE;
}
*/

/*
//  ===========================================================================
//  iter_readable_unset()
//  ===========================================================================
//
//  Set an iter not-readable
//  ( do logic verifications & launch check_if_empty )
//
gboolean
GcmdGtkFoldview::Model::iter_readable_unset(
	GtkTreeIter * _iter)
{
	Row					*   row			= NULL;
	gint					n			= 0;
	GtkTreePath			*   gtk_path	= NULL;
	//.........................................................................
	//GCMD_INF("iter_readable_unset");
	g_return_val_if_fail( _iter, FALSE );

	//.........................................................................
	// Logic verifications
    // Cant use iter_test, because we need the GtkTreePath
    //

	// Row
	if ( ! iter_get_treerow(_iter, &row) )
	{
		GCMD_ERR("Model::iter_readable_unset():could not get Row");
		return FALSE;
	}

	// Already not-readable
	if ( ! row->readable() )
	{
		GCMD_WNG("Model::iter_readable_unset():already not-readable [%s]", row->utf8_name_display());
		return TRUE;
	}

	// get the # of children
	n = iter_n_children(_iter);

	// get the GtkTreePath
	gtk_path = GnomeCmdFoldviewTreestore::get_path(treemodel(), _iter);
	if ( ! gtk_path )
	{
		GCMD_ERR("Model::iter_readable_unset():could not get GtkTreePath [%s]", row->utf8_name_display());
		return FALSE;
	}

	//.........................................................................
	// if no children, unset directly
	if ( n == 0 )
		goto iter_sterile;

	//.........................................................................
	// iter is fertile, and expanded : collapse it
	if ( iter_is_expanded(_iter) )
	{
		// It is a hack ! Let GtkTreeView update its cache by collapsing !
		// We dont have to manually delete each row and signals on them !
		if ( ! view()->row_collapse(gtk_path) )									// __GWR__HACK__
		{
			GCMD_ERR("Model::iter_readable_unset():iter could not be collapsed");
			goto exit_failure;
		}

		// We dont return here, since the iter is in the following state !
	}

	//.........................................................................
	// iter is fertile, and collapsed : silently remove children
	iter_collapsed_remove_children(_iter);

	//.........................................................................
	// Unset the sterile iter
iter_sterile:

	// change data
	row->readable(FALSE);

	// inform treview that row has changed
	treestore()->ext_data_changed(_iter);

//exit_success:
	gtk_tree_path_free(gtk_path);
	return TRUE;

exit_failure:
	gtk_tree_path_free(gtk_path);
	return FALSE;
}
*/


