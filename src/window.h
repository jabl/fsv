/* window.h */

/* Main program window */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_WINDOW_H
	#error
#endif
#define FSV_WINDOW_H

#include <gtk/gtk.h>

typedef enum {
	SB_LEFT,
	SB_RIGHT
} StatusBarID;


void window_init(GtkApplication *app, gpointer user_data);
void window_set_access( boolean enabled );
#ifdef FSV_COLOR_H
void window_set_color_mode( ColorMode mode );
#endif
void window_birdseye_view_off( void);
void window_statusbar( StatusBarID sb_id, const char *message );


/* end window.h */
