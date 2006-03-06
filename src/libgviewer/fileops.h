#ifndef __LIBGVIEWER_FILEOPS_H__
#define __LIBGVIEWER_FILEOPS_H__

/*
	File Handling functions (based on Midnight Commander's view.c)

	'open' & 'close' : just open and close the file handle
	'load' & 'free' : allocate & free buffer memory, call mmap/munmap

	calling order should be: open->load->[use file with "get_byte"]->free (which calls close)
*/


typedef struct _ViewerFileOps ViewerFileOps;

ViewerFileOps* gv_fileops_new();

/*
	returns -1 on failure
*/
int     gv_file_open(ViewerFileOps *ops, const gchar* _file);

int     gv_file_open_fd(ViewerFileOps *ops, int filedesc);


/*
 	returns: NULL on success
*/
char   *gv_file_load (ViewerFileOps *ops, int fd);

/*
	return values: NULL for success, else points to error message
*/
char   *gv_file_init_growing_view (ViewerFileOps *ops, const char *filename);

/*
	returns: -1 on failure
		0->255 value on success
*/
int     gv_file_get_byte (ViewerFileOps *ops, offset_type byte_index);

offset_type gv_file_get_max_offset(ViewerFileOps *ops);

void    gv_file_close (ViewerFileOps *ops);

void    gv_file_free (ViewerFileOps *ops);

#endif
