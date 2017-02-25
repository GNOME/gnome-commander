/** 
 * @file gnome-cmd-notebook.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GNOME_CMD_NOTEBOOK_H__
#define __GNOME_CMD_NOTEBOOK_H__

#include <gtk/gtk.h>

#define GNOME_CMD_TYPE_NOTEBOOK              (gnome_cmd_notebook_get_type ())
#define GNOME_CMD_NOTEBOOK(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_NOTEBOOK, GnomeCmdNotebook))
#define GNOME_CMD_NOTEBOOK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_NOTEBOOK, GnomeCmdNotebookClass))
#define GNOME_CMD_IS_NOTEBOOK(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_NOTEBOOK))
#define GNOME_CMD_IS_NOTEBOOK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_NOTEBOOK))
#define GNOME_CMD_NOTEBOOK_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_NOTEBOOK, GnomeCmdNotebookClass))


GType gnome_cmd_notebook_get_type ();


struct GnomeCmdNotebook
{
    GtkNotebook parent;      // this MUST be the first member

    class Private;

    Private *priv;

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_NOTEBOOK, "show-tabs", FALSE, "scrollable", TRUE, "show-border", FALSE, NULL);  }
    void operator delete (void *p)      {  g_object_unref (p);  }

    operator GtkWidget * () const       {  return GTK_WIDGET (this);     }
    operator GtkContainer * () const    {  return GTK_CONTAINER (this);  }
    operator GtkNotebook * () const     {  return GTK_NOTEBOOK (this);   }

    enum TabBarVisibility
    {
        HIDE_TABS,
        HIDE_TABS_IF_ONE,
        SHOW_TABS
    };

    TabBarVisibility tabs_visibility;

    explicit GnomeCmdNotebook(TabBarVisibility visibility=HIDE_TABS_IF_ONE);

    int size() const                    {  return gtk_notebook_get_n_pages (*this);       }
    bool empty() const                  {  return size()==0;                              }

    gint get_current_page() const       {  return gtk_notebook_get_current_page (*this);  }
    void set_current_page(gint n)       {  gtk_notebook_set_current_page (*this, n);      }

    GtkWidget *page() const             {  return gtk_notebook_get_nth_page (*this, get_current_page());  }
    GtkWidget *page(gint n) const       {  return gtk_notebook_get_nth_page (*this, n);   }

    gint insert_page(GtkWidget *page_insert, gint n, GtkWidget *label=NULL);
    gint insert_page(GtkWidget *page_insert, gint n, const gchar *label)    {  return insert_page(page_insert, n, label ? gtk_label_new (label) : NULL);  }
    gint prepend_page(GtkWidget *page_prepend, GtkWidget *label=NULL)       {  return insert_page(page_prepend, 0, label);                                 }
    gint prepend_page(GtkWidget *page_prepend, const gchar *label)          {  return insert_page(page_prepend, 0, label);   }
    gint append_page(GtkWidget *page_append, GtkWidget *label=NULL)         {  return insert_page(page_append, -1, label);                                }
    gint append_page(GtkWidget *page_append, const gchar *label)            {  return insert_page(page_append, -1, label);  }

    void remove_page(gint n);
    void remove_page()                                                      {  remove_page (get_current_page());                                   }

    GtkWidget *get_label() const                                            {  return gtk_notebook_get_tab_label (*this, page());   }
    GtkWidget *get_label(gint n) const                                      {  return gtk_notebook_get_tab_label (*this, page(n));                 }
    void set_label(GtkWidget *label=NULL)                                   {  gtk_notebook_set_tab_label (*this, page(), label);   }
    void set_label(gint n, GtkWidget *label=NULL)                           {  gtk_notebook_set_tab_label (*this, page(n), label);                 }
    void set_label(const gchar *label)                                      {  set_label(label ? gtk_label_new (label) : NULL);     }
    void set_label(gint n, const gchar *label)                              {  set_label(n, label ? gtk_label_new (label) : NULL);                 }

    void prev_page();
    void next_page();

    void show_tabs(TabBarVisibility show);
    void show_tabs(gboolean show)                                           {  show_tabs(show ? SHOW_TABS : HIDE_TABS);                            }

    int find_tab_num_at_pos(gint screen_x, gint screen_y) const;
};

inline gint GnomeCmdNotebook::insert_page(GtkWidget *page_insert, gint n, GtkWidget *label)
{
    if (tabs_visibility==HIDE_TABS_IF_ONE && size()==1)
        gtk_notebook_set_show_tabs (*this, TRUE);
    return gtk_notebook_insert_page (*this, page_insert, label, n);
}

inline void GnomeCmdNotebook::remove_page(gint n)
{
    gtk_notebook_remove_page (*this, n);
    if (tabs_visibility==HIDE_TABS_IF_ONE && size()<2)
        gtk_notebook_set_show_tabs (*this, FALSE);
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
