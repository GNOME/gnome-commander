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
	gnome-cmd-foldview.cc gnome-cmd-foldview.h \
	gnome-cmd-foldview-control.cc \
	gnome-cmd-foldview-gvfs.cc gnome-cmd-foldview-gvfs.h \
	gnome-cmd-foldview-model.cc \
	gnome-cmd-foldview-private.h \
	gnome-cmd-foldview-treestore.cc gnome-cmd-foldview-treestore.h \
	gnome-cmd-foldview-view.cc

-include $(top_srcdir)/git.mk
