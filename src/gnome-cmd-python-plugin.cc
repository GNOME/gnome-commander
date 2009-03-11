/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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
#include <Python.h>
#include <glib/gprintf.h>
#include <gdk/gdkx.h>
#include <dirent.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-python-plugin.h"
#include "utils.h"

using namespace std;


#define MODULE_INIT_FUNC "main"


static GList *py_plugins = NULL;


static gint compare_plugins(const PythonPluginData *p1, const PythonPluginData *p2)
{
    return g_ascii_strcasecmp (p1->name, p2->name);
}


static void scan_plugins_in_dir (const gchar *dpath)
{
    DIR *dir = opendir(dpath);

    if (!dir)
    {
        g_warning ("Could not list files in %s: %s", dpath, strerror (errno));
        return;
    }

    long dir_size = pathconf(".", _PC_PATH_MAX);

   if (dir_size==-1)
   {
       g_warning ( "pathconf(\".\"): %s", strerror(errno) );
      return;
   }

    gchar *prev_dir = (gchar *) g_malloc (dir_size);

    if (!prev_dir)
    {
        closedir(dir);
        return;
    }

    getcwd(prev_dir, dir_size);

    if (chdir(dpath) == 0)
    {
        struct dirent *ent = readdir(dir);

        for (; ent; ent=readdir(dir))
        {

            if (!g_str_has_suffix (ent->d_name, ".py")  &&
                !g_str_has_suffix (ent->d_name, ".pyc") &&
                !g_str_has_suffix (ent->d_name, ".pyo"))
                continue;

            struct stat buf;

            if (stat(ent->d_name, &buf) != 0)
                g_warning("Failed to stat %s", ent->d_name);
            else
                if (S_ISREG(buf.st_mode))                           // if the direntry is a regular file ...
                {
                    PythonPluginData *data = g_new0 (PythonPluginData, 1);
                    data->name = g_strdup (ent->d_name);
                    *strchr (data->name, '.') = '\0';               // strip {.py, .pyc} off

                    data->path = g_build_path (G_DIR_SEPARATOR_S, dpath, data->name, NULL);
                    g_strdelimit (data->name, "_-", ' ');           // replace '[-_]' with ' '

                    // if there is already data->name plugin in py_plugins list ...
                    if (g_list_find_custom (py_plugins, data, (GCompareFunc) compare_plugins))
                    {
                        DEBUG('p', "Ignored duplicate plugin '%s' (%s%s%s)\n", data->name, dpath, G_DIR_SEPARATOR_S, ent->d_name);
                        g_free (data->name);
                        g_free (data->path);
                        g_free (data);
                        continue;
                    }

                    data->fname = strrchr(data->path, G_DIR_SEPARATOR);
                    if (data->fname)  data->fname++;

                    py_plugins = g_list_append (py_plugins, data);
                    DEBUG('p', "Found %s%s%s plugin and added as '%s'\n", dpath, G_DIR_SEPARATOR_S, ent->d_name, data->name);
                }
        }

        chdir(prev_dir);
        py_plugins = g_list_sort (py_plugins, (GCompareFunc) compare_plugins);
    }

    closedir(dir);

    g_free (prev_dir);
}


void python_plugin_manager_init ()
{
    gchar *user_dir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir(), ".gnome-commander/plugins", NULL);
    gchar *set_plugin_path = g_strdup_printf("sys.path = ['%s', '%s'] + sys.path", user_dir, PLUGIN_DIR);

    DEBUG('p', "User plugin dir: %s\n", user_dir);
    DEBUG('p', "System plugin dir: %s\n", PLUGIN_DIR);

    create_dir_if_needed (user_dir);
    scan_plugins_in_dir (user_dir);

    scan_plugins_in_dir (PLUGIN_DIR);

    Py_Initialize ();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString(set_plugin_path);

    g_free (user_dir);
    g_free (set_plugin_path);
}


void python_plugin_manager_shutdown ()
{
    for (GList *l=py_plugins; l; l=l->next)
    {
        PythonPluginData *data = (PythonPluginData *) l->data;
        g_free (data->name);
        g_free (data->path);
        // do not free data->fname, as it points to a part of data->path
    }

    if (py_plugins)
        g_list_free(py_plugins);

    py_plugins = NULL;

    Py_Finalize();
}


GList *gnome_cmd_python_plugin_get_list()
{
    return py_plugins;
}


inline gchar *get_dir_as_str (GnomeCmdMainWin *mw, FileSelectorID id)
{
    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_fs (mw, id);

    if (!fs)
        return NULL;

    GnomeVFSURI *dir_uri = gnome_cmd_dir_get_uri (fs->get_directory());

    if (!dir_uri)
        return NULL;

    gchar *dir = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (dir_uri), NULL);

    gnome_vfs_uri_unref (dir_uri);

    return dir;
}


gboolean gnome_cmd_python_plugin_execute(const PythonPluginData *plugin, GnomeCmdMainWin *mw)
{
    gboolean retval = FALSE;

    PyObject *pURIclass = NULL;
    PyObject *pName = NULL;
    PyObject *pModule = NULL;
    PyObject *pFunc = NULL;

    DEBUG('p', "Calling %s." MODULE_INIT_FUNC "()\n", plugin->fname);

    PyObject *pmod = PyImport_ImportModule("gnomevfs");

    if (!pmod)
        pmod = PyImport_ImportModule("gnome.vfs");

    if (!pmod)
    {
        create_error_dialog (_("Can't load python module 'gnomevfs' ('gnome.vfs')\n"));
        goto out_A;
    }

    pURIclass = PyObject_GetAttrString(pmod, "URI");
    Py_XDECREF(pmod);

    if (!pURIclass)
        goto out_B;

    pName = PyString_FromString(plugin->fname);
    pModule = PyImport_Import(pName);
    Py_XDECREF(pName);

    if (!pModule)
    {
        PyErr_Print();
        g_warning("Failed to load '%s'", plugin->path);

        goto out_C;
    }

    pFunc = PyObject_GetAttrString(pModule, MODULE_INIT_FUNC);

    if (!pFunc || !PyCallable_Check(pFunc))
    {
        if (PyErr_Occurred())
            PyErr_Print();
        g_warning("Cannot find function '%s'", MODULE_INIT_FUNC);

        goto out_D;
    }

    GnomeCmdFileSelector *active_fs;
    GnomeCmdFileList     *active_fl;

    active_fs = gnome_cmd_main_win_get_fs (mw, ACTIVE);
    active_fl = active_fs ? active_fs->file_list() : NULL;

    if (!GNOME_CMD_IS_FILE_LIST (active_fl))
        goto out_D;

    GList *selected_files;
    GList *f;

    selected_files = active_fl->get_selected_files();
    f = selected_files = active_fl->sort_selection(selected_files);

    gint n;

    n = g_list_length (selected_files);

    DEBUG('p', "Selected files: %d\n", n);

    if (!n)
    {
        retval = TRUE;
        goto out_D;
    }

    gchar *active_dir;
    gchar *inactive_dir;

    active_dir = get_dir_as_str (mw, ACTIVE);
    inactive_dir = get_dir_as_str (mw, INACTIVE);

    XID main_win_xid;

    PyObject *pMainWinXID;
    PyObject *pActiveCwd;
    PyObject *pInactiveCwd;
    PyObject *pSelectedFiles;

    main_win_xid = GDK_WINDOW_XID (GTK_WIDGET (mw)->window);
    pMainWinXID = PyLong_FromUnsignedLong (main_win_xid);
    pActiveCwd = PyString_FromString (active_dir);
    pInactiveCwd = PyString_FromString (inactive_dir);
    pSelectedFiles = PyTuple_New(n);

    DEBUG('p', "Main window XID: %lu (%#lx)\n", main_win_xid, main_win_xid);
    DEBUG('p', "Active directory:   %s\n", active_dir);
    DEBUG('p', "Inactive directory: %s\n", inactive_dir);

    for (gint i=0; f; f=f->next, ++i)
    {
        GnomeCmdFile *finfo = (GnomeCmdFile *) f->data;
        gchar *uri_str = gnome_cmd_file_get_uri_str (finfo);
        PyObject *pArgs  = Py_BuildValue("(s)", uri_str);
        PyObject *pURI = PyEval_CallObject(pURIclass, pArgs);
        Py_XDECREF(pArgs);
        g_free (uri_str);

        if (!pURI)
            continue;

        PyTuple_SetItem(pSelectedFiles, i, pURI);   // pURI reference stolen here
    }

    g_free (active_dir);
    g_free (inactive_dir);
    g_list_free (selected_files);

    PyObject *pValue;
    pValue = PyObject_CallFunctionObjArgs(pFunc, pMainWinXID, pActiveCwd, pInactiveCwd, pSelectedFiles, NULL);

    if (pValue)
    {
        retval = PyInt_AsLong(pValue);
        DEBUG('p', "Result of call %s." MODULE_INIT_FUNC "(): %ld\n", plugin->fname, retval);
    }
    else
    {
        PyErr_Print();
        g_warning("Call to %s." MODULE_INIT_FUNC "() failed", plugin->fname);
    }

    Py_XDECREF(pMainWinXID);
    Py_XDECREF(pActiveCwd);
    Py_XDECREF(pInactiveCwd);
    Py_XDECREF(pSelectedFiles);

    Py_XDECREF(pValue);

  out_D:

    Py_XDECREF(pFunc);

  out_C:

    Py_XDECREF(pModule);

  out_B:

    Py_XDECREF(pURIclass);

  out_A:

    return retval;
}
