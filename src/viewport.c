/* viewport.c */

/* Viewport routines */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * SPDX-FileCopyrightText: 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "viewport.h"

#include <gtk/gtk.h>
#include <epoxy/gl.h> /* GLuint */

#include "about.h"
#include "camera.h"
#include "dialog.h" /* context_menu( ) */
#include "filelist.h" /* filelist_show_entry( ) */
#include "geometry.h"
#include "gui.h"
#include "ogl.h"
#include "window.h"


/* Sensitivity factor used for manual camera control */
#define MOUSE_SENSITIVITY 0.5


/* The node table, used to find a node by its ID number */
static GNode **node_table = NULL;
static size_t node_table_size;

/* The currently highlighted (indicated) node */
static GNode *indicated_node = NULL;


/* Receives a newly created node table from scanfs( ) */
void
viewport_pass_node_table(GNode **new_node_table, size_t ntsize)
{
	if (node_table != NULL)
		xfree( node_table );

	node_table = new_node_table;
	node_table_size = ntsize;
}


/* This returns the node (if any) that is visible at viewport location
 * (x,y) (where (0,0) indicates the upper-left corner). The ID number of
 * the particular face being pointed at is stored in face_id */
static GNode *
node_at_location( int x, int y)
{
	// First try the new method
	GLuint n_id = ogl_select_modern(x, y);
	if (n_id) {
		if (n_id >= node_table_size)
			g_warning("Got node id %u larger than node table size %zu\n", n_id, node_table_size);
		else
			return node_table[n_id];
	}
	return NULL;
}


/* This callback catches all events for the viewport */
gboolean
viewport_cb(GtkWidget *gl_area_w, GdkEvent *event, gpointer user_data)
{
	GNode *node;
	double dx, dy;
	gdouble x, y;
	GdkModifierType ev_state;
	/* Previous mouse pointer coordinates */
	static double prev_x, prev_y;
	boolean btn1, btn2, btn3;
	boolean ctrl_key;

	GdkEventType ev_type = gdk_event_get_event_type(event);
	/* Handle low-level GL area widget events */
	switch (ev_type) {
		case GDK_EXPOSE:
		ogl_refresh( );
		return FALSE;

		case GDK_CONFIGURE:
		ogl_resize( );
		return FALSE;

		default:
		/* Event is probably coming from the mouse */
		break;
	}

	if (ev_type == GDK_BUTTON_PRESS) {
		/* Exit the About presentation if it is up */
		if (about( ABOUT_END )) {
			indicated_node = NULL;
			return FALSE;
		}
	}

	/* If we're in splash screen mode, proceed no further */
	if (globals.fsv_mode == FSV_SPLASH)
		return FALSE;

	gint scale; // Scale factor for HiDPI

	/* Mouse-related events */
	switch (ev_type) {
		case GDK_BUTTON_PRESS:
		guint button;
		if (!gdk_event_get_button(event, &button))
			break;
		btn1 = button == 1;
		btn2 = button == 2;
		btn3 = button == 3;
		if (!gdk_event_get_state(event, &ev_state))
			break;
		ctrl_key = ev_state & GDK_CONTROL_MASK;
		if (!gdk_event_get_coords(event, &x, &y))
			break;
		scale = gtk_widget_get_scale_factor(gl_area_w);
		x = x * scale;
		y = y * scale;
		if (camera_moving( )) {
			/* Yipe! Impatient user */
			camera_pan_finish( );
			indicated_node = NULL;
		}
		else if (!ctrl_key) {
			if (btn2)
				indicated_node = NULL;
			else
				indicated_node = node_at_location(x, y);
			if (indicated_node == NULL) {
				geometry_highlight_node( NULL, FALSE );
				window_statusbar( SB_RIGHT, "" );
			}
			else {
				if (geometry_should_highlight(indicated_node) || btn1)
					geometry_highlight_node( indicated_node, btn1 );
				else
					geometry_highlight_node( NULL, FALSE );
				window_statusbar( SB_RIGHT, node_absname( indicated_node ) );
				if (btn3) {
					/* Bring up context-sensitive menu */
					context_menu( indicated_node, (GdkEventButton*) event );
					filelist_show_entry( indicated_node );
				}
			}
		}
		prev_x = x;
		prev_y = y;
		break;

		case GDK_2BUTTON_PRESS:
		/* Ignore second click of a double-click */
		break;

		case GDK_BUTTON_RELEASE:
		if (!gdk_event_get_state(event, &ev_state))
			break;
		btn1 = ev_state & GDK_BUTTON1_MASK;
		ctrl_key = ev_state & GDK_CONTROL_MASK;
		if (btn1 && !ctrl_key && !camera_moving( ) && (indicated_node != NULL))
			camera_look_at( indicated_node );
		gui_cursor( gl_area_w, -1 );
		break;

		case GDK_MOTION_NOTIFY:
		if (!gdk_event_get_state(event, &ev_state))
			break;
		btn1 = ev_state & GDK_BUTTON1_MASK;
		btn2 = ev_state & GDK_BUTTON2_MASK;
		btn3 = ev_state & GDK_BUTTON3_MASK;
		ctrl_key = ev_state & GDK_CONTROL_MASK;
		if (!gdk_event_get_coords(event, &x, &y))
			break;
		scale = gtk_widget_get_scale_factor(gl_area_w);
		x = x * scale;
		y = y * scale;
		if (!camera_moving( ) && !gtk_events_pending( )) {
			if (btn2) {
				/* Dolly the camera */
				gui_cursor( gl_area_w, GDK_DOUBLE_ARROW );
				dy = MOUSE_SENSITIVITY * (y - prev_y);
				camera_dolly( - dy );
				indicated_node = NULL;
			}
			else if (ctrl_key && btn1) {
				/* Revolve the camera */
				gui_cursor( gl_area_w, GDK_FLEUR );
				dx = MOUSE_SENSITIVITY * (x - prev_x);
				dy = MOUSE_SENSITIVITY * (y - prev_y);
				camera_revolve( dx, dy );
				indicated_node = NULL;
			}
			else if (!ctrl_key && (btn1 || btn3)) {
				/* Pointless dragging */
				if (indicated_node != NULL) {
					node = node_at_location(x, y);
					if (node != indicated_node)
						indicated_node = NULL;
				}
			}
                        else
				indicated_node = node_at_location(x, y);
			/* Update node highlighting */
			if (indicated_node == NULL) {
				geometry_highlight_node( NULL, FALSE );
				window_statusbar( SB_RIGHT, "" );
			}
			else {
				if (geometry_should_highlight(indicated_node) || btn1)
					geometry_highlight_node( indicated_node, btn1 );
				else
					geometry_highlight_node( NULL, FALSE);
				window_statusbar( SB_RIGHT, node_absname( indicated_node ) );
			}
			prev_x = x;
			prev_y = y;
		}
		break;

		case GDK_LEAVE_NOTIFY:
		/* The mouse has left the viewport */
		geometry_highlight_node( NULL, FALSE );
		window_statusbar( SB_RIGHT, "" );
		gui_cursor( gl_area_w, -1 );
		indicated_node = NULL;
		break;

		default:
		/* Ignore event */
		break;
	}

	return FALSE;
}


/* end viewport.c */
