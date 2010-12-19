## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	-I$(srcdir)/.. \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

noinst_LIBRARIES = libtreeview.a

libtreeview_a_SOURCES = \
	gnome-cmd-connection-treeview-control.cc \
	gnome-cmd-connection-treeview-control.snippet.h \
	gnome-cmd-connection-treeview-model-gio.cc \
	gnome-cmd-connection-treeview-model-struct.cc \
	gnome-cmd-connection-treeview-model-struct.snippet.h \
	gnome-cmd-connection-treeview-model-vfs.cc \
	gnome-cmd-connection-treeview-model.cc \
	gnome-cmd-connection-treeview-model.snippet.h \
	gnome-cmd-connection-treeview-view.cc \
	gnome-cmd-connection-treeview-view.snippet.h \
	gnome-cmd-connection-treeview.cc  gnome-cmd-connection-treeview.h \
	gnome-cmd-foldview-docs.h\
	gnome-cmd-foldview-logger.cc gnome-cmd-foldview-logger.h \
	gnome-cmd-foldview-private.h \
	gnome-cmd-foldview-quickstack.h \
	gnome-cmd-foldview-treestore.cc gnome-cmd-foldview-treestore.h \
	gnome-cmd-foldview-utils.h \
	gnome-cmd-foldview.cc gnome-cmd-foldview.h

-include $(top_srcdir)/git.mk
