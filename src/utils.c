/*
  GNOME Commander - A GNOME based file manager 
  Copyright (C) 2001-2004 Marcus Bjurman

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

#include <config.h>
#include <locale.h>
#include <errno.h>
#include <dirent.h>
#include "gnome-cmd-includes.h"
#include "utils.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-xfer.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fnmatch.h>

typedef struct
{
	GnomeCmdFile *finfo;
	GtkWidget *dialog;
	gpointer *args;
	
} TmpDlData;

#ifdef HAVE_LOCALE_H
extern struct lconv *locale_information;
#endif

#define FIX_PW_HACK
#define STRINGS_TO_URIS_CHUNK 1024

static GdkCursor *cursor_busy = NULL;
extern gchar *debug_flags;
static gchar *tmp_file_dir = NULL;


gboolean DEBUG_ENABLED (gchar flag)
{
    if (debug_flags != NULL)
       return (strchr(debug_flags, flag) != 0);
	
	return FALSE;
}

/**
 * The already reserved debug flags:
 * --------------------------------
 * c: file and directory counting
 * d: directory ref-counting
 * f: file ref-counting
 * g: run_command debugging
 * i: imageloader
 * p: directory pool
 * l: directory listings
 * m: connection debug.
 * n: directory-monitoring debug
 * s: smb network browser debugging.
 * w: widget_lookup debugging
 * y: brief mime-based imageload debugging
 * z: detailed mime-based imageload debugging
 * x: xfer
 * 
 */
void DEBUG (gchar flag, const gchar *format, ...)
{
    if (DEBUG_ENABLED (flag)) {
		va_list ap;		
		va_start(ap, format);
		fprintf (stderr, "[%c%c] ", flag-32, flag-32);
		vfprintf(stderr, format, ap);
		va_end(ap);
    }
}


void warn_print (const gchar *fmt, ...)
{
    va_list     argptr;
    char        string[1024];

    va_start (argptr,fmt);
    vsnprintf (string,1024,fmt,argptr);
    va_end (argptr);
    g_printerr("WARNING: %s", string);
}


void run_command (const gchar *command, gboolean term)
{
	run_command_indir (command, NULL, term);
}


void run_command_indir (const gchar *in_command, const gchar *dir,
						gboolean term)
{
	gchar *command;
	
	if (term) {
		gchar *tmp, *arg;

		tmp = g_strdup_printf ("%s; %s/bin/gcmd-block", in_command, PREFIX);
		arg = g_shell_quote (tmp);
		command = g_strdup_printf (gnome_cmd_data_get_term (), arg);
		g_free (arg);
		g_free (tmp);
	}
	else
		command = g_strdup (in_command);

	DEBUG ('g', "running: %s\n", command);
	gnome_execute_shell (dir, command);
	g_free (command);
}


const char **
convert_varargs_to_name_array (va_list args)
{
	GPtrArray *resizeable_array;
	const char *name;
	const char **plain_ole_array;
	
	resizeable_array = g_ptr_array_new ();

	do {
		name = va_arg (args, const char *);
		g_ptr_array_add (resizeable_array, (gpointer) name);
	} while (name != NULL);

	plain_ole_array = (const char **) resizeable_array->pdata;
	
	g_ptr_array_free (resizeable_array, FALSE);

	return plain_ole_array;
}


static gboolean
delete_event_callback (gpointer data,
		       gpointer user_data)
{
	g_return_val_if_fail (GTK_IS_DIALOG (data), FALSE);
	
	gtk_signal_emit_stop_by_name (GTK_OBJECT (data), "delete_event");
	
	return TRUE;
}


static gboolean
on_run_dialog_keypress (GtkWidget *dialog, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Escape) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_NONE);
		return TRUE;
	}

	return FALSE;
}


gint
run_simple_dialog (GtkWidget *parent, gboolean ignore_close_box,
				   GtkMessageType msg_type,
				   const char *text, const char *title, ...)
{
	va_list button_title_args;
	const char **button_titles;
	GtkWidget *dialog;
//	GtkWidget *top_widget;
	int result, i;

	
	/* Create the dialog. */
	va_start (button_title_args, title);
	button_titles = convert_varargs_to_name_array (button_title_args);
	va_end (button_title_args);

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (main_win), GTK_DIALOG_MODAL,
		msg_type, GTK_BUTTONS_NONE, text);
	if (title)
		gtk_window_set_title (GTK_WINDOW (dialog), title);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	for ( i=0 ; button_titles[i] != NULL ; i++ )
		gtk_dialog_add_button (GTK_DIALOG (dialog), button_titles[i], i);
									 
	g_free (button_titles);
	
	/* Allow close. */
	if (ignore_close_box) {
		gtk_signal_connect (GTK_OBJECT (dialog),
							"delete_event",
							GTK_SIGNAL_FUNC (delete_event_callback),
							NULL);
	}
	else {
		gtk_signal_connect (GTK_OBJECT (dialog),
							"key-press-event",
							GTK_SIGNAL_FUNC (on_run_dialog_keypress),
							dialog);
	}
	
	gtk_window_set_wmclass (GTK_WINDOW (dialog), "dialog", "Eel");

	
	/* Run it. */
	do
	{
		gtk_widget_show (dialog);
		result = gtk_dialog_run (GTK_DIALOG (dialog));
	} while (ignore_close_box && result == GTK_RESPONSE_DELETE_EVENT);

	gtk_widget_destroy(dialog);

	return result;
}


gboolean string2int (const gchar *s, gint *i)
{
    int ret;
    ret = sscanf (s, "%d", i);
    return (ret == 1);
}


gboolean string2uint (const gchar *s, guint *i)
{
    int ret;
    ret = sscanf (s, "%d", i);
    return (ret == 1);
}


gboolean string2short (const gchar *s, gshort *sh)
{
    int i,ret;
    ret = sscanf (s, "%d", &i);
    *sh = i;    
    return (ret == 1);
}


gboolean string2ushort (const gchar *s, gushort *sh)
{
    int i,ret;
    ret = sscanf (s, "%d", &i);
    *sh = i;    
    return (ret == 1);
}


gboolean string2char (const gchar *s, gchar *c)
{
    int i, ret;
    ret = sscanf (s, "%d", &i);
    *c = i;
    return (ret == 1);
}


gboolean string2uchar (const gchar *s, guchar *c)
{
    int i, ret;
    ret = sscanf (s, "%d", &i);
    *c = i;
    return (ret == 1);
}


gboolean string2float (const gchar *s, gfloat *f)
{
    int ret;
    ret = sscanf (s, "%f", f);
    return (ret == 1);
}


char *int2string (int i)
{
    return g_strdup_printf ("%d", i);
}


gchar *str_uri_basename (const gchar *uri)
{
	int i, len, last_slash=0;

	if (!uri)
		return NULL;

	len = strlen (uri);

	if (len < 2)
		return NULL;

	for ( i=0 ; i<len ; i++ )
	{
		if (uri[i] == '/')
			last_slash = i;
	}

	return gnome_vfs_unescape_string (&uri[last_slash+1], NULL);
}          


void
type2string (GnomeVFSFileType type,
			 gchar *buf,
			 guint max)
{
	char *s;
	
	switch (type) {
	case GNOME_VFS_FILE_TYPE_UNKNOWN:
		s = "?";
		break;
	case GNOME_VFS_FILE_TYPE_REGULAR:
		s = " ";
		break;
	case GNOME_VFS_FILE_TYPE_DIRECTORY:
		s = "/";
		break;
	case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
		s = "@";
		break;
	case GNOME_VFS_FILE_TYPE_FIFO:
		s = "F";
		break;
	case GNOME_VFS_FILE_TYPE_SOCKET:
		s = "S";
		break;
	case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
		s = "C";
		break;
	case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
		s = "B";
		break;
	default:
		 s = "?";
	}

	g_snprintf (buf, max, "%s", s);
}


void name2string (gchar *filename, gchar *buf, guint max)
{
	g_snprintf (buf, max, "%s", filename);
}


void perm2string (GnomeVFSFilePermissions p, gchar *buf, guint max)
{
	switch (gnome_cmd_data_get_perm_disp_mode ()) {
		case GNOME_CMD_PERM_DISP_MODE_TEXT:
			perm2textstring (p, buf, max);
			break;
		case GNOME_CMD_PERM_DISP_MODE_NUMBER:
			perm2numstring (p, buf, max);
			break;

		default:
			perm2textstring (p, buf, max);
			break;
	}
}


void perm2textstring (GnomeVFSFilePermissions p, gchar *buf, guint max)
{
	g_snprintf (buf, max, "%s%s%s%s%s%s%s%s%s", 
				(p & GNOME_VFS_PERM_USER_READ) ? "r" : "-",
				(p & GNOME_VFS_PERM_USER_WRITE) ? "w" : "-",
				(p & GNOME_VFS_PERM_USER_EXEC) ? "x" : "-",
				(p & GNOME_VFS_PERM_GROUP_READ) ? "r" : "-",
				(p & GNOME_VFS_PERM_GROUP_WRITE) ? "w" : "-",
				(p & GNOME_VFS_PERM_GROUP_EXEC) ? "x" : "-",
				(p & GNOME_VFS_PERM_OTHER_READ) ? "r" : "-",
				(p & GNOME_VFS_PERM_OTHER_WRITE) ? "w" : "-",
				(p & GNOME_VFS_PERM_OTHER_EXEC) ? "x" : "-");
}


void perm2numstring (GnomeVFSFilePermissions p, gchar *buf, guint max)
{
	gint i = 0;

	if (p & GNOME_VFS_PERM_USER_READ) i += 100;
	if (p & GNOME_VFS_PERM_USER_WRITE) i += 200;
	if (p & GNOME_VFS_PERM_USER_EXEC) i += 400;
	if (p & GNOME_VFS_PERM_GROUP_READ) i += 10;
	if (p & GNOME_VFS_PERM_GROUP_WRITE) i += 20;
	if (p & GNOME_VFS_PERM_GROUP_EXEC) i += 40;
	if (p & GNOME_VFS_PERM_OTHER_READ) i += 1;
	if (p & GNOME_VFS_PERM_OTHER_WRITE) i += 2;
	if (p & GNOME_VFS_PERM_OTHER_EXEC) i += 4;

	g_snprintf (buf, max, "%d", i);
}


const gchar *size2string (GnomeVFSFileSize size, 
						  GnomeCmdSizeDispMode size_disp_mode)
{
	static gchar buf[64];
	
    if (size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_POWERED)
    {
        gchar *prefixes[5] = {"B ","kB ","MB ","GB ","TB "};
        gint i=0;
        gdouble dsize = (gdouble)size;
        
        for ( i=0 ; i<5 ; i++ )
        {
            if (dsize > 1024)
                dsize /= 1024;
            else
                break;
        }

        if (i)
            g_snprintf (buf, 64, "%.1f %s",dsize,prefixes[i]);
		else
			g_snprintf (buf, 64, "%lld %s", size, prefixes[0]);
    }
    else if (size_disp_mode == GNOME_CMD_SIZE_DISP_MODE_GROUPED)
    {
        int i,j,len,outlen;
        char tmp[256];
        char *out;

        sprintf (tmp, "%lld", size);
        len = strlen (tmp);
        
        if (len < 4)
		{
            strncpy (buf, tmp, 64);
			return buf;
		}
            
        outlen = len/3 + len;
		
        if ((len/3)*3 == len)
			outlen--;
           
        out = buf;
        memset (out, '\0', 64);
         
        for ( i=len-1,j=outlen-1 ; i>=0 ; i--,j-- )
        {
            if (((outlen-j)/4)*4 == outlen-j)
            {
#ifdef HAVE_LOCALE_H
				gchar sep = locale_information->thousands_sep[0];

				if (sep != '\0')
					out[j] = sep;
				else
					out[j] = ',';
#else
				out[j] = ',';
#endif
                i++;
            }
            else
                out[j] = tmp[i];
        }
    }
	else
	{
		g_snprintf (buf, 64, "%lld", size);
	}

	return buf;
}


const gchar *time2string (time_t t, const gchar *date_format)
{
	static gchar buf[64];
    struct tm lt;

	localtime_r (&t, &lt);
    strftime (buf, sizeof(buf), date_format, &lt);
	return buf;
}


static void
no_mime_app_found_error (gchar *mime_type)
{
	gchar *msg;

	msg = g_strdup_printf (_("No default application found for the mime-type %s.\nOpen the \"File types and programs\" page in the Control Center to add one."),
						   mime_type);
	create_error_dialog (msg);
	g_free (msg);
}


static void do_mime_exec_single (gpointer *args)
{
	gchar *cmd;
	GnomeCmdApp *app = (GnomeCmdApp*)args[0];
	gchar *path = (gchar*)args[1];
	gchar *arg;

	arg = g_shell_quote (path);
	cmd = g_strdup_printf ("%s %s", gnome_cmd_app_get_command (app), arg);
	g_free (arg);
	run_command (cmd, gnome_cmd_app_get_requires_terminal (app));
	g_free (cmd);
	g_free (path);
	gnome_cmd_app_free (app);
	g_free (args);
}


static void
on_tmp_download_response (GtkWidget *w, gint id, TmpDlData *dldata)
{
	if (id == GTK_RESPONSE_YES) {
		GnomeVFSURI *src_uri, *dest_uri;
		GnomeCmdCon *con;
		GnomeCmdPath *path;
		gchar *path_str;

		path_str = get_temp_download_filepath (gnome_cmd_file_get_name (dldata->finfo));
		if (!path_str) return;
		dldata->args[1] = (gpointer)path_str;

		src_uri = gnome_vfs_uri_dup (gnome_cmd_file_get_uri (dldata->finfo));
		
		path = gnome_cmd_plain_path_new (path_str);
		con = gnome_cmd_con_list_get_home (
			gnome_cmd_data_get_con_list ());
		dest_uri = gnome_cmd_con_create_uri (con, path);
		gtk_object_destroy (GTK_OBJECT (path));
			
		gnome_cmd_xfer_tmp_download (src_uri,
									 dest_uri,
									 GNOME_VFS_XFER_FOLLOW_LINKS,
									 GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
									 GTK_SIGNAL_FUNC (do_mime_exec_single),
									 dldata->args);
	}
	else {
		gnome_cmd_app_free ((GnomeCmdApp*)dldata->args[0]);
		g_free (dldata->args);
	}

	g_free (dldata);
	gtk_widget_destroy (dldata->dialog);
}


void mime_exec_single (GnomeCmdFile *finfo)
{
	gpointer *args;
	GnomeVFSMimeApplication *vfs_app;
	GnomeCmdApp *app;
	
	g_return_if_fail (finfo != NULL);
	g_return_if_fail (finfo->info != NULL);

	if (!finfo->info->mime_type)
		return;

	/* Check if the file is a binary executable that lacks the executable bit
	 */
	if (!gnome_cmd_file_is_executable (finfo)) {
		if (gnome_cmd_file_has_mime_type (finfo, "application/x-executable-binary")) {
			gchar *fname = get_utf8 (finfo->info->name);
			gchar *msg = g_strdup_printf (_("\"%s\" seems to be a binary executable file but it lacks the executable bit. Do you want to set it and then run the file?"), fname);
			gint ret = run_simple_dialog (
				GTK_WIDGET (main_win), FALSE, GTK_MESSAGE_QUESTION, msg,
				_("Make Executable?"), 
				_("Cancel"), _("OK"), NULL);
			g_free (fname);
			g_free (msg);

			if (ret == 1) {
				GnomeVFSResult res = gnome_cmd_file_chmod (
					finfo, finfo->info->permissions|GNOME_VFS_PERM_USER_EXEC);
				if (res != GNOME_VFS_OK)
					return;
			}
			else
				return;
		}
	}


	/* If the file is executable but not a binary file, check if the user wants to
	 * exec it or open it.
	 */
	if (gnome_cmd_file_is_executable (finfo)) {
		if (gnome_cmd_file_has_mime_type (finfo, "application/x-executable-binary")) {
			gnome_cmd_file_execute (finfo);
			return;
		}
		else if (gnome_cmd_file_mime_begins_with (finfo, "text/")) {
			gchar *fname = get_utf8 (finfo->info->name);
			gchar *msg = g_strdup_printf (_("\"%s\" is an executable text file. Do you want to run it, or display it's contents?"), fname);
			gint ret = run_simple_dialog (
				GTK_WIDGET (main_win), FALSE, GTK_MESSAGE_QUESTION, msg, _("Run or Display"), 
				_("Cancel"), _("Display"), _("Run"), NULL);
			g_free (fname);
			g_free (msg);

			if (ret != 1) {
				if (ret == 2)
					gnome_cmd_file_execute (finfo);
				return;
			}
		}
	}

	vfs_app = gnome_vfs_mime_get_default_application (finfo->info->mime_type);
	if (!vfs_app) {
		no_mime_app_found_error (finfo->info->mime_type);
		return;
	}
	
	app = gnome_cmd_app_new_from_vfs_app (vfs_app);
	gnome_vfs_mime_application_free (vfs_app);

	args = g_new (gpointer, 2);

	if (gnome_cmd_file_is_local (finfo)) {
		args[0] = (gpointer)app;
		args[1] = (gpointer)g_strdup (gnome_cmd_file_get_real_path (finfo));
		do_mime_exec_single (args);
	}
	else {
		if (gnome_cmd_app_get_handles_uris (app)
			&& gnome_cmd_data_get_honor_expect_uris()) {
			args[0] = (gpointer)app;
			args[1] = (gpointer)g_strdup (gnome_cmd_file_get_uri_str (finfo));
			do_mime_exec_single (args);
		}
		else {
			gchar *msg = g_strdup_printf (_("%s does not know how to open remote files. Do you want to download the file to a temporary location and then open it?"), gnome_cmd_app_get_name (app));
			GtkWidget *dialog = gtk_message_dialog_new (
				GTK_WINDOW (main_win), GTK_DIALOG_MODAL,
				GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, msg);
			TmpDlData *dldata = g_new (TmpDlData, 1);
			args[0] = (gpointer)app;
			dldata->finfo = finfo;
			dldata->dialog = dialog;
			dldata->args = args;
			gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

			gtk_signal_connect (GTK_OBJECT (dialog), "response",
								GTK_SIGNAL_FUNC (on_tmp_download_response), dldata);
			gtk_widget_show (dialog);
			g_free (msg);
		}
	}
}


static void do_mime_exec_multiple (gpointer *args)
{
	gchar *cmd;
	GnomeCmdApp *app = (GnomeCmdApp*)args[0];
	GList *files = (GList*)args[1];

	if (files) {
		cmd = g_strdup_printf ("%s ", gnome_cmd_app_get_command (app));

		while (files) {
			gchar *path = (gchar*)files->data;
			gchar *tmp = cmd;
			gchar *arg = g_shell_quote (path);
			cmd = g_strdup_printf ("%s %s", tmp, arg);
			g_free (arg);
			g_free (path);
			g_free (tmp);
			files = files->next;
		}

		run_command (cmd, gnome_cmd_app_get_requires_terminal (app));
		g_free (cmd);
		g_list_free (files);
	}
	gnome_cmd_app_free (app);
	g_free (args);
}


void mime_exec_multiple (GList *files, GnomeCmdApp *app)
{
	GList *src_uri_list = NULL;
	GList *dest_uri_list = NULL;
	gpointer *args;
	GList *local_files = NULL;
	gboolean asked = FALSE;
	gint retid;
	
	g_return_if_fail (files != NULL);
	g_return_if_fail (app != NULL);

	while (files) {
		GnomeCmdFile *finfo = (GnomeCmdFile*)files->data;

		if (gnome_vfs_uri_is_local (gnome_cmd_file_get_uri (finfo)))
			local_files = g_list_append (local_files, g_strdup (gnome_cmd_file_get_real_path (finfo)));
		else {
			if (gnome_cmd_app_get_handles_uris (app)
				&& gnome_cmd_data_get_honor_expect_uris()) {
				local_files = g_list_append (local_files,  g_strdup (gnome_cmd_file_get_uri_str (finfo)));
			}
			else {
				if (!asked) {
					gchar *msg = g_strdup_printf (
						_("%s does not know how to open remote files. Do you want to download the files to a temporary location and then open them?"), gnome_cmd_app_get_name (app));
					retid = run_simple_dialog (
						GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_QUESTION,
						msg, "", _("No"), _("Yes"), NULL);
					asked = TRUE;
				}

				if (retid == 1) {
					GnomeCmdCon *con;
					GnomeCmdPath *path;
					GnomeVFSURI *src_uri, *dest_uri;
					gchar *path_str;

					path_str = get_temp_download_filepath (gnome_cmd_file_get_name (finfo));
					if (!path_str) return;
			
					src_uri = gnome_vfs_uri_dup (gnome_cmd_file_get_uri (finfo));
					path = gnome_cmd_plain_path_new (path_str);
					con = gnome_cmd_con_list_get_home (
						gnome_cmd_data_get_con_list ());
					dest_uri = gnome_cmd_con_create_uri (con, path);
					gtk_object_destroy (GTK_OBJECT (path));

					src_uri_list = g_list_append (src_uri_list, src_uri);
					dest_uri_list = g_list_append (dest_uri_list, dest_uri);
					local_files = g_list_append (local_files, path_str);
				}
			}
		}

		files = files->next;
	}

	g_list_free (files);
	
	args = g_new (gpointer, 2);
	args[0] = app;
	args[1] = local_files;

	if (src_uri_list) {
		gnome_cmd_xfer_tmp_download_multiple (
			src_uri_list,
			dest_uri_list,
			GNOME_VFS_XFER_FOLLOW_LINKS,
			GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
			GTK_SIGNAL_FUNC (do_mime_exec_multiple),
			args);
	}
	else {
		do_mime_exec_multiple (args);
	}
}


gboolean state_is_blank (gint state)
{
	gboolean ret;

	ret = (state & GDK_SHIFT_MASK) || (state & GDK_CONTROL_MASK) || (state & GDK_MOD1_MASK);
	
	return !ret;
}


gboolean state_is_shift (gint state)
{
	return (state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && !(state & GDK_MOD1_MASK);
}


gboolean state_is_ctrl (gint state)
{
	return !(state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && !(state & GDK_MOD1_MASK);
}


gboolean state_is_alt (gint state)
{
	return !(state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}


gboolean state_is_alt_shift (gint state)
{
	return (state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}


gboolean state_is_ctrl_alt (gint state)
{
	return !(state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}


gboolean state_is_ctrl_shift (gint state)
{
	return (state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && !(state & GDK_MOD1_MASK);
}


gboolean state_is_ctrl_alt_shift (gint state)
{
	return (state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) && (state & GDK_MOD1_MASK);
}


void clear_event_key (GdkEventKey *event)
{
	g_return_if_fail (event != NULL);
	g_return_if_fail (event->string != NULL);
	
	event->keyval = GDK_VoidSymbol;
	event->string[0] = '\0';	
}


/**
 * Transform a "\r\n" separated string into a GList with GnomeVFSURI's
 */
GList *
strings_to_uris (gchar *data)
{
	int i;
	GList *uri_list = NULL;
	gchar **filenames;

	filenames = g_strsplit (data, "\r\n", STRINGS_TO_URIS_CHUNK);
	
	for ( i=0 ; filenames[i] != NULL ; i++ ) {
		gchar *fn;
		GnomeVFSURI *uri;

		if (i == STRINGS_TO_URIS_CHUNK) {
			uri_list = g_list_concat (uri_list, strings_to_uris (filenames[i]));
			break;
		}

		fn = g_strdup (filenames[i]);
		uri = gnome_vfs_uri_new (fn);
		fix_uri (uri);
		if (uri)
			uri_list = g_list_append (uri_list, uri);
		g_free (fn);
	}

	g_strfreev (filenames);
	return uri_list;
}


GnomeVFSFileSize calc_tree_size (const GnomeVFSURI *dir_uri)
{
	GnomeVFSFileSize size = 0;
	GnomeVFSFileInfoOptions infoOpts = 0;
	GList *list = NULL, *tmp;
	gchar *dir_uri_str;
	GnomeVFSResult result;

	if (!dir_uri) return -1;

	dir_uri_str = gnome_vfs_uri_to_string (dir_uri, 0);
		
	if (!dir_uri_str) return -1;
	
	result = gnome_vfs_directory_list_load (
		&list,
		dir_uri_str,
		infoOpts);

	if (result != GNOME_VFS_OK)
		return 0;
	
	if (!list)
		return 0;

	tmp = list;
	while (tmp) {
		GnomeVFSFileInfo *info = (GnomeVFSFileInfo*)tmp->data;
		if (strcmp (info->name, ".") != 0 && strcmp (info->name, "..") != 0) {		
			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				GnomeVFSURI *new_dir_uri = gnome_vfs_uri_append_file_name (
					dir_uri, info->name);
				size += calc_tree_size (new_dir_uri);
				gnome_vfs_uri_unref (new_dir_uri);
			}
			else
				size += info->size;
		}

		tmp = tmp->next;
	}

	tmp = list;
	while (tmp) {
		GnomeVFSFileInfo *info = (GnomeVFSFileInfo*)tmp->data;		
		gnome_vfs_file_info_unref (info);		
		tmp = tmp->next;
	}

	g_list_free (list);
	g_free (dir_uri_str);

	return size;
}


GList *string_history_add (GList *in, const gchar *value, gint maxsize)
{
	GList *tmp;
	GList *out;

	tmp = g_list_find_custom (in,
							  (gchar*)value,
							  (GCompareFunc)strcmp);

	/* if the same value has been given before move it first in the list */
	if (tmp != NULL) {
		out = g_list_remove_link (in, tmp);
		tmp->next = out;
		if (out)
			out->prev = tmp;
		out = tmp;
	}	
	/* or if its new just add it */
	else {
		out = g_list_prepend (in, g_strdup (value));
	}

	/* don't let the history get to long */
	while (g_list_length (out) > maxsize) {
		tmp = g_list_last (out);
		g_free (tmp->data);
		out = g_list_remove_link (out, tmp);
	}

	return out;
}


const gchar *
create_nice_size_str (GnomeVFSFileSize size)
{
	const gchar *s1;
	static gchar str1[64];
	s1 = size2string (size, GNOME_CMD_SIZE_DISP_MODE_GROUPED);
	snprintf (str1, 64, "%s bytes", s1);
	
	if (size >= 1000) {	
		const gchar *s2;
		static gchar str2[128];
		s2 = size2string (size, GNOME_CMD_SIZE_DISP_MODE_POWERED);
		snprintf (str2, 256, "%s (%s)", s2, str1);
		return str2;
	}
	
	return str1;
}


gchar* quote_if_needed (const gchar *in)
{
	//return strpbrk(in," ;&$'\"?")==NULL ? g_strdup (in) : g_strdup_printf ("'%s'", in);
	return g_shell_quote (in);
}


gchar* unquote_if_needed (const gchar *in)
{
	gint l;
	
	g_return_val_if_fail (in != NULL, NULL);

	l = strlen (in);
	/* Check if the first and last character is a quote */
	if (l>1 && strchr("'\"",in[0])!=NULL && in[0]==in[l-1]) {
		gchar *out = g_strdup (in+1);
		out[l-2] = '\0';
		return out;
	}

	return g_strdup (in);
}


void stop_kp (GtkObject *obj)
{
	gtk_signal_emit_stop_by_name (obj, "key-press-event");
}


GtkWidget *create_styled_button (const gchar *text)
{
	GtkWidget *w;
	
	if (text)
		w = gtk_button_new_with_label (text);
	else
		w = gtk_button_new ();

	gtk_button_set_relief (GTK_BUTTON (w), gnome_cmd_data_get_button_relief ());
	gtk_widget_ref (w);
	gtk_widget_show (w);
	return w;
}


GtkWidget *create_styled_pixmap_button (const gchar *text, GnomeCmdPixmap *pm)
{
	GtkWidget *btn, *label, *pixmap;
	GtkWidget *hbox;

	g_return_val_if_fail (text || pm, NULL);

	btn = create_styled_button (NULL);

	hbox = gtk_hbox_new (FALSE, 1);
	gtk_object_set_data_full (GTK_OBJECT (btn), "hbox", hbox,
							  (GtkDestroyNotify) gtk_widget_unref);	
	gtk_widget_ref (hbox);
	gtk_widget_show (hbox);		

	if (pm) {
		pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
		if (pixmap) {
			gtk_widget_ref (pixmap);
			gtk_object_set_data_full (GTK_OBJECT (btn), "pixmap", pixmap,
									  (GtkDestroyNotify) gtk_widget_unref);
			gtk_widget_show (pixmap);
		}
	}

	if (text) {
		label = gtk_label_new (text);
		gtk_widget_ref (label);
		gtk_object_set_data_full (GTK_OBJECT (btn), "label", label,
								  (GtkDestroyNotify) gtk_widget_unref);
		gtk_widget_show (label);
	}

	if (pm && !text)
		gtk_container_add (GTK_CONTAINER (btn), pixmap);
	else if (!pm && text)
		gtk_container_add (GTK_CONTAINER (btn), label);
	else {
		gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
		gtk_container_add (GTK_CONTAINER (btn), hbox);
	}

	return btn;
}


void set_cursor_busy_for_widget (GtkWidget *widget)
{
	if (!cursor_busy)
		cursor_busy = gdk_cursor_new (GDK_WATCH);
	
	gdk_window_set_cursor (widget->window, cursor_busy);

	while (gtk_events_pending ())
		gtk_main_iteration();
}

void set_cursor_default_for_widget (GtkWidget *widget)
{
	gdk_window_set_cursor (widget->window, NULL);
}

void set_cursor_busy (void)
{
	set_cursor_busy_for_widget (GTK_WIDGET (main_win));
}

void set_cursor_default (void)
{
	set_cursor_default_for_widget (GTK_WIDGET (main_win));
}


GList *app_get_linked_libs (GnomeCmdFile *finfo)
{
	FILE *fd;
	gchar *cmd, *s;
	gchar tmp[256];
	gchar *arg;
	GList *libs = NULL;

	g_return_val_if_fail (GNOME_CMD_IS_FILE (finfo), NULL);

	arg = g_shell_quote (gnome_cmd_file_get_real_path (finfo));
	cmd = g_strdup_printf ("ldd %s", arg);
	g_free (arg);
	fd = popen (cmd, "r");
	g_free (cmd);

	if (!fd) return NULL;

	while ((s = fgets (tmp, 256, fd)))
	{
		char **v = g_strsplit (s, " ", 1);
		if (v) {
			libs = g_list_append (libs, g_strdup (v[0]));
			g_strfreev (v);
		}
	}

	return libs;
}


gboolean app_needs_terminal (GnomeCmdFile *finfo)
{
	GList *tmp, *libs;
	gboolean need_term = TRUE;

	if (strcmp (finfo->info->mime_type, "application/x-executable-binary"))
		return TRUE;

	libs = tmp = app_get_linked_libs (finfo);
	if  (!libs) return FALSE;

	while (tmp) {
		gchar *lib = (gchar*)tmp->data;
		lib = g_strstrip (lib);
		if (strncmp (lib, "libX11", 6) == 0) {
			need_term = FALSE;
			break;
		}
		tmp = tmp->next;
	}

	g_list_foreach (libs, (GFunc)g_free, NULL);
	g_list_free (libs);
	
	return need_term;
}


gchar *get_temp_download_filepath (const gchar *fname)
{
	const gchar *tmp_dir = g_get_tmp_dir ();
	
	if (!tmp_file_dir) {
		gchar *tmp_file_dir_template = g_strdup_printf ("gcmd-%s-XXXXXX", g_get_user_name());
		chdir (tmp_dir);
		tmp_file_dir = mkdtemp (tmp_file_dir_template);
		if (!tmp_file_dir) {
			g_free (tmp_file_dir_template);
			
			create_error_dialog (
				"Failed to create directory to store temporary files in.\nError message: %s\n",
				strerror (errno));
			return NULL;
		}
	}

	return g_build_path ("/", tmp_dir, tmp_file_dir, fname, NULL);
}


void remove_temp_download_dir (void)
{
	const gchar *tmp_dir = g_get_tmp_dir ();
	
	if (tmp_file_dir) {
		gchar *path = g_build_path ("/", tmp_dir, tmp_file_dir, NULL);
		gchar *command = g_strdup_printf ("rm -rf %s", path);
		g_free (path);
		system (command);
		g_free (command);
	}
}


GtkWidget *
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


static void
transform (gchar *s, gchar from, gchar to)
{
	gint i;
	gint len = strlen (s);
	
	for ( i=0 ; i<len ; i++ )
		if (s[i] == from) s[i] = to;
}


gchar *unix_to_unc (const gchar *path)
{
	gchar *out;
	
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (path[0] == '/', NULL);

	out = malloc (strlen(path)+2);
	out[0] = '\\';
	strcpy (out+1, path);
	transform (out+1, '/', '\\');

	return out;
}

gchar *unc_to_unix (const gchar *path)
{
	gchar *out;
	
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (path[0] == '\\', NULL);
	g_return_val_if_fail (path[1] == '\\', NULL);

	out = malloc (strlen(path));
	strcpy (out, path+1);
	transform (out, '\\', '/');

	return out;
}


GdkColor *gdk_color_new (gushort r, gushort g, gushort b)
{
	GdkColor *c = g_new (GdkColor, 1);
	c->pixel = 0;
	c->red = r;
	c->green = g;
	c->blue = b;
	return c;
}


GList *file_list_to_uri_list (GList *files)
{
	GList *uris = NULL;

	while (files) {
		GnomeCmdFile *finfo = GNOME_CMD_FILE (files->data);
		GnomeVFSURI *uri = gnome_cmd_file_get_uri (finfo);

		if (!uri)
			g_warning ("NULL uri!!!\n");
		else {
			uris = g_list_append (uris, uri);
		}
		
		files = files->next;
	}

	return uris;
}


GList *file_list_to_info_list (GList *files)
{
	GList *infos = NULL;

	if (!files) return NULL;

	while (files) {
		GnomeCmdFile *finfo = GNOME_CMD_FILE (files->data);
		infos = g_list_append (infos, finfo->info);
		files = files->next;
	}

	return infos;
}


gboolean
create_dir_if_needed (const gchar *dpath)
{
	DIR *dir;
	
	g_return_val_if_fail (dpath, FALSE);
	
	if ((dir = opendir (dpath)) == NULL) {
		if (errno == ENOENT) {
			g_print (_("Creating directory %s... "), dpath);
			if (mkdir (dpath, S_IRUSR|S_IWUSR|S_IXUSR) == 0) {
				return TRUE;
			}
			else {
				gchar *msg = g_strdup_printf (_("Failed to create the directory %s"), dpath);
				perror (msg);
				g_free (msg);
			}
		}
		else {
			gchar *msg = g_strdup_printf (_("Couldn't read from the directory %s: %s\n"),
										  dpath, strerror (errno));
			warn_print (msg);
			g_free (msg);
		}
		
		return FALSE;
	}

	closedir (dir);
	return TRUE;
}


void
edit_mimetypes (const gchar *mime_type, gboolean blocking)
{
	gchar *cmd;
	gchar *arg;

	if (!mime_type)
		mime_type = "";

	arg = g_shell_quote (mime_type);
	cmd = g_strdup_printf (
		"gnome-file-types-properties %s\n", arg);
	g_free (arg);
	if (blocking)
		system (cmd);
	else
		run_command (cmd, FALSE);
	g_free (cmd);
}


void fix_uri (GnomeVFSURI *uri)
{
#ifdef FIX_PW_HACK
	gchar *p, *t;
	const gchar *hn, *pw;
		
	hn = gnome_vfs_uri_get_host_name (uri);
	if (!hn) return;
	
	pw = gnome_vfs_uri_get_password (uri);

	t = g_strdup (hn);
	p = g_strrstr (t, "@");
	if (p && p[1] != '\0') {
		gchar *hn2;
		gchar *pw2;
		*p = '\0';
		hn2 = g_strdup (p+1);
		pw2 = g_strdup_printf ("%s@%s", pw, t);
		gnome_vfs_uri_set_host_name (uri, hn2);
		gnome_vfs_uri_set_password (uri, pw2);
		g_free (hn2);
		g_free (pw2);
	}
	g_free (t);
#endif
}


GList *patlist_new (const gchar *pattern_string)
{
	gint i;
	gchar **ents;
	GList *patlist = NULL;
	
	g_return_val_if_fail (pattern_string != NULL, NULL);

	i = 0;
	ents = g_strsplit (pattern_string, ";", 0);
	while (ents[i]) {
		patlist = g_list_append (patlist, ents[i]);
		i++;
	}
	g_free (ents);

	return patlist;
}


void patlist_free (GList *pattern_list)
{
	g_return_if_fail (pattern_list != NULL);
	
	g_list_foreach (pattern_list, (GFunc)g_free, NULL);
	g_list_free (pattern_list);
}


gboolean patlist_matches (GList *pattern_list, const gchar *s)
{
	GList *tmp = pattern_list;
	gint fn_flags = FNM_NOESCAPE|FNM_CASEFOLD;

	while (tmp) {
		if (fnmatch ((gchar*)tmp->data, s, fn_flags) == 0)
			return TRUE;
		tmp = tmp->next;
	}

	return FALSE;
}


