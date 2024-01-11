/* dialog.h */

/* Dialog windows */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_DIALOGS_H
	#error
#endif
#define FSV_DIALOGS_H


#ifdef __GTK_H__
void dialog_pass_main_window_widget( GtkWidget *window_w );
void context_menu( GNode *node );
#endif
void dialog_change_root( void );
void dialog_color_setup( void );
void dialog_help( void );


/* end dialog.h */
