#ifndef __GNOME_CMD_PIXMAP_H__
#define __GNOME_CMD_PIXMAP_H__

typedef struct {
    GdkPixbuf *pixbuf;
    GdkPixmap *pixmap;
    GdkBitmap *mask;
    gint width;
    gint height;
} GnomeCmdPixmap;


GnomeCmdPixmap *
gnome_cmd_pixmap_new_from_file (const gchar *filename);

GnomeCmdPixmap *
gnome_cmd_pixmap_new_from_pixbuf (GdkPixbuf *pixbuf);

void
gnome_cmd_pixmap_free (GnomeCmdPixmap *pixmap);


#endif //__GNOME_CMD_PIXMAP_H__
