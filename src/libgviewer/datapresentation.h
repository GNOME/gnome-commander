#ifndef __LIBGVIEWER_DATA_PRESENTATION_H__
#define __LIBGVIEWER_DATA_PRESENTATION_H__

typedef struct _GVDataPresentation GVDataPresentation;

typedef enum
{
	PRSNT_NO_WRAP,
	PRSNT_WRAP,
	
	/* Here, BIN_FIXED is "fixed number of binary characters per line"
	   (e.g. CHAR=BYTE, no UTF-8 or other translations) */
	PRSNT_BIN_FIXED
} PRESENTATION;

GVDataPresentation* gv_data_presentation_new();

void gv_init_data_presentation(GVDataPresentation *dp, GVInputModesData *imd, offset_type max_offset);
void gv_free_data_presentation(GVDataPresentation *dp);

void gv_set_data_presentation_mode(GVDataPresentation *dp, PRESENTATION present) ;
PRESENTATION gv_get_data_presentation_mode(GVDataPresentation *dp);
void gv_set_wrap_limit(GVDataPresentation *dp, guint chars_per_line);
void gv_set_fixed_count(GVDataPresentation *dp, guint chars_per_line);
void gv_set_tab_size(GVDataPresentation *dp, guint tab_size);

offset_type gv_align_offset_to_line_start(GVDataPresentation *dp, offset_type offset);
offset_type gv_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta);
offset_type gv_get_end_of_line_offset(GVDataPresentation *dp, offset_type start_of_line);


#endif
