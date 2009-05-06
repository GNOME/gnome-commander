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

#ifndef __GNOME_CMD_NOTEBOOK_H__
#define __GNOME_CMD_NOTEBOOK_H__

#include <gtk/gtk.h>

#define GNOME_CMD_TYPE_NOTEBOOK          (gnome_cmd_notebook_get_type ())
#define GNOME_CMD_NOTEBOOK(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_NOTEBOOK, GnomeCmdNotebook))
#define GNOME_CMD_IS_NOTEBOOK(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_NOTEBOOK))


GType gnome_cmd_notebook_get_type ();


struct GnomeCmdNotebook
{
    GtkNotebook parent;      // this MUST be the first member

    class Private;

    Private *priv;

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_NOTEBOOK, "show-tabs", FALSE, NULL);  }
    void operator delete (void *p)      {  g_object_unref (p);  }

    operator GtkWidget * ()             {  return GTK_WIDGET (this);    }
    operator GtkNotebook * ()           {  return GTK_NOTEBOOK (this);  }

    int size()                          {  return gtk_notebook_get_n_pages (*this);  }
    bool empty()                        {  return size()==0;  }

    gint get_current_page()             {  return gtk_notebook_get_current_page (*this);  }
    void set_current_page(gint n)       {  gtk_notebook_set_current_page (*this, n);      }

    GtkWidget *page()                   {  return gtk_notebook_get_nth_page (*this, get_current_page());  }
    GtkWidget *page(gint n)             {  return gtk_notebook_get_nth_page (*this, n);  }

    gint insert_page(GtkWidget *page, gint n, const gchar *label=NULL);
    gint prepend_page(GtkWidget *page, const gchar *label=NULL)             {  return insert_page(page, 0, label);   }
    gint append_page(GtkWidget *page, const gchar *label=NULL)              {  return insert_page(page, -1, label);  }

    void remove_page(gint n);
    void remove_page()                  {  remove_page (get_current_page());  }

    void set_label(const gchar *label=NULL);
    void set_label(gint n, const gchar *label=NULL);

    void prev_page();
    void next_page();
};


inline gint GnomeCmdNotebook::insert_page(GtkWidget *page, gint n, const gchar *label)
{
    if (size()==1)
        gtk_notebook_set_show_tabs (*this, TRUE);
    return gtk_notebook_insert_page (*this, page, label ? gtk_label_new (label) : NULL, n);
}


inline void GnomeCmdNotebook::remove_page(gint n)
{
    gtk_notebook_remove_page (*this, n);
    if (size()<2)
        gtk_notebook_set_show_tabs (*this, FALSE);
}


inline void GnomeCmdNotebook::set_label(const gchar *label)
{
    gtk_notebook_set_tab_label (*this, page(), label ? gtk_label_new (label) : NULL);
}


inline void GnomeCmdNotebook::set_label(gint n, const gchar *label)
{
    gtk_notebook_set_tab_label (*this, page(n), label ? gtk_label_new (label) : NULL);
}

inline void GnomeCmdNotebook::prev_page()
{
    if (get_current_page()>0)
        gtk_notebook_prev_page (*this);
    else
        if (size()>1)
            set_current_page(-1);
}

inline void GnomeCmdNotebook::next_page()
{
    int n = size();

    if (get_current_page()+1<n)
        gtk_notebook_next_page (*this);
    else
        if (n>1)
            set_current_page(0);
}

#endif // __GNOME_CMD_NOTEBOOK_H__
