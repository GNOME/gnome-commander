
lib_LIBRARIES = \
	libfoldview.a

libfoldview_a_SOURCES = \
	gnome-cmd-foldview-docs.h\
	gnome-cmd-foldview-logger.cc \
	gnome-cmd-foldview-logger.h \
	gnome-cmd-foldview-quickstack.h \
	gnome-cmd-foldview-utils.h \
	gnome-cmd-foldview-treestore.cc \
	gnome-cmd-foldview-treestore.h \
	gnome-cmd-connection-treeview.cc \
	gnome-cmd-connection-treeview.h \
	gnome-cmd-connection-treeview-control.cc \
	gnome-cmd-connection-treeview-control.snippet.h \
	gnome-cmd-connection-treeview-model.cc \
	gnome-cmd-connection-treeview-model.snippet.h \
	gnome-cmd-connection-treeview-model-gio.cc \
	gnome-cmd-connection-treeview-model-struct.cc \
	gnome-cmd-connection-treeview-model-struct.snippet.h \
	gnome-cmd-connection-treeview-model-vfs.cc \
	gnome-cmd-connection-treeview-view.cc \
	gnome-cmd-connection-treeview-view.snippet.h \
	gnome-cmd-foldview.cc \
	gnome-cmd-foldview-private.h \
	gnome-cmd-foldview.h

libfoldview_a_CFLAGS = \
	-Wno-unused-label

AM_CPPFLAGS = \
	$(CC_WARNINGS) \
	-I$(top_srcdir) \
	-I$(srcdir)/.. \
	$(GNOMEUI_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	$(EXIV2_CFLAGS) \
	$(TAGLIB_CFLAGS) \
	$(CHM_CFLAGS) \
	$(GSF_CFLAGS) \
	$(POPPLER_CFLAGS) \
	-DPLUGIN_DIR=\""$(libdir)/$(PACKAGE)/plugins"\"

## File created by the gnome-build tools

