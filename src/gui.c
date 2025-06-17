/* gui.c */

/* Higher-level GTK+ interface */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include <gtk/gtk.h>
#include "gui.h"

#include "animation.h"
#include "ogl.h" /* ogl_widget_new( ) */


/* Box packing flags */
enum {
	GUI_PACK_EXPAND	= 1 << 0,
	GUI_PACK_FILL	= 1 << 1,
	GUI_PACK_START	= 1 << 2
};


/* For whenever the main loop is far away */
void
gui_update( void )
{
	GMainContext* ctx = g_main_context_default();
	while (g_main_context_pending(ctx))
		g_main_context_iteration(ctx, FALSE);
}


/* This checks if the widget associated with the given adjustment is
 * currently busy redrawing/reconfiguring itself, or is in steady state
 * (this is used when animating widgets to avoid changing the adjustment
 * too often, otherwise the widget can't keep up and things slow down) */
boolean
gui_adjustment_widget_busy( GtkAdjustment *adj )
{
	static const double threshold = (1.0 / 18.0);
	double t_prev;
	double t_now;
	double *tp;

	/* ---- HACK ALERT ----
	 * This doesn't actually check GTK+ internals-- I'm not sure which
	 * ones are relevant here. This just checks the amount of time that
	 * has passed since the last time the function was called with the
	 * same adjustment and returned FALSE, and if it's below a certain
	 * threshold, the object is considered "busy" (returning TRUE) */

	t_now = xgettime( );

	tp = g_object_get_data(G_OBJECT(adj), "t_prev");
	if (tp == NULL) {
		tp = NEW(double);
		*tp = t_now;
		g_object_set_data_full(G_OBJECT(adj), "t_prev", tp, _xfree);
		return FALSE;
	}

	t_prev = *tp;

	if ((t_now - t_prev) > threshold) {
		*tp = t_now;
		return FALSE;
	}

	return TRUE;
}


/* This places child_w into parent_w intelligently. expand and fill
 * flags are applicable only if parent_w is a box widget */
static void
parent_child_full( GtkWidget *parent_w, GtkWidget *child_w, boolean expand, boolean fill )
{
	bitfield *packing_flags;
	boolean start = TRUE;

	if (parent_w != NULL) {
		if (GTK_IS_BOX(parent_w)) {
			packing_flags = g_object_get_data(G_OBJECT(parent_w), "packing_flags");
			if (packing_flags != NULL) {
                                /* Get (non-default) box-packing flags */
				expand = *packing_flags & GUI_PACK_EXPAND;
				fill = *packing_flags & GUI_PACK_FILL;
				start = *packing_flags & GUI_PACK_START;
			}
                        if (start)
				gtk_box_pack_start( GTK_BOX(parent_w), child_w, expand, fill, 0 );
                        else
				gtk_box_pack_end( GTK_BOX(parent_w), child_w, expand, fill, 0 );
		}
		else
			gtk_container_add( GTK_CONTAINER(parent_w), child_w );
		gtk_widget_show( child_w );
	}
}


/* Calls parent_child_full( ) with defaults */
static void
parent_child( GtkWidget *parent_w, GtkWidget *child_w )
{
	parent_child_full( parent_w, child_w, NO_EXPAND, NO_FILL );
}


// Non-static wrapper for parent_child
void
gui_set_parent_child(GtkWidget *parent_w, GtkWidget *child_w)
{
	parent_child(parent_w, child_w);
}


/* The horizontal box widget */
GtkWidget *
gui_hbox_add( GtkWidget *parent_w, int spacing )
{
	GtkWidget *hbox_w;

	hbox_w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
	gtk_container_set_border_width( GTK_CONTAINER(hbox_w), spacing );
	parent_child( parent_w, hbox_w );

	return hbox_w;
}


/* The vertical box widget */
GtkWidget *
gui_vbox_add( GtkWidget *parent_w, int spacing )
{
	GtkWidget *vbox_w;

	vbox_w = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
	gtk_container_set_border_width( GTK_CONTAINER(vbox_w), spacing );
	parent_child( parent_w, vbox_w );

	return vbox_w;
}


/* Changes a box widget's default packing flags (i.e. the flags that will
 * be used to pack subsequent children) */
void
gui_box_set_packing( GtkWidget *box_w, boolean expand, boolean fill, boolean start )
{
	static const char data_key[] = "packing_flags";
	bitfield *packing_flags;

	/* Make sure box_w is a box widget */
	g_assert( GTK_IS_BOX(box_w) );
	/* If expand is FALSE, then fill should not be TRUE */
	g_assert( expand || !fill );

	packing_flags = g_object_get_data(G_OBJECT(box_w), data_key);
	if (packing_flags == NULL) {
		/* Allocate new packing-flags variable for box */
		packing_flags = NEW(bitfield);
		g_object_set_data_full(G_OBJECT(box_w), data_key, packing_flags, _xfree);
	}

        /* Set flags appropriately */
	*packing_flags = 0;
	*packing_flags |= (expand ? GUI_PACK_EXPAND : 0);
	*packing_flags |= (fill ? GUI_PACK_FILL : 0);
	*packing_flags |= (start ? GUI_PACK_START : 0);
}


/* The standard button widget */
GtkWidget *
gui_button_add( GtkWidget *parent_w, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *button_w;

	button_w = gtk_button_new( );
	if (label != NULL)
		gui_label_add( button_w, label );
	g_signal_connect(G_OBJECT(button_w), "clicked", G_CALLBACK(callback), callback_data);
	parent_child( parent_w, button_w );

	return button_w;
}


/* Creates a button with a pixbuf prepended to the label */
GtkWidget *
gui_button_with_pixbuf_xpm_add(GtkWidget *parent_w, const char **xpm_data, const char *label, void (*callback)( ), void *callback_data)
{
	GtkWidget *button_w;
	GtkWidget *hbox_w, *hbox2_w;

	button_w = gtk_button_new( );
	parent_child( parent_w, button_w );
	hbox_w = gui_hbox_add( button_w, 0 );
	hbox2_w = gui_hbox_add( hbox_w, 0 );
	gui_widget_packing( hbox2_w, EXPAND, NO_FILL, AT_START );
	gui_pixbuf_xpm_add(hbox2_w, xpm_data);
	if (label != NULL) {
		gui_vbox_add( hbox2_w, 2 ); /* spacer */
		gui_label_add( hbox2_w, label );
	}
	g_signal_connect(G_OBJECT(button_w), "clicked", G_CALLBACK(callback), callback_data);

	return button_w;
}


/* The toggle button widget */
GtkWidget *
gui_toggle_button_add( GtkWidget *parent_w, const char *label, boolean active, void (*callback)( ), void *callback_data )
{
	GtkWidget *tbutton_w;

	tbutton_w = gtk_toggle_button_new( );
	if (label != NULL)
		gui_label_add( tbutton_w, label );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(tbutton_w), active );
	g_signal_connect(G_OBJECT(tbutton_w), "toggled", G_CALLBACK(callback), callback_data);
	parent_child( parent_w, tbutton_w );

	return tbutton_w;
}


/* The wildcard color pattern list widget (fitted into a scrolled window) */
GtkWidget *
gui_wpattern_list_new(GtkWidget *parent_w)
{
	/* Make the scrolled window widget */
	GtkWidget *scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Make the tree view widget */
	GtkWidget *view = gtk_tree_view_new();

	/* The color column is used to display the color as a background specified in
	the third (hidden) column. */
	GtkTreeViewColumn *col_col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col_col, "Color");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col_col);

	GtkCellRenderer *renderer_col = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col_col, renderer_col, TRUE);
	gtk_tree_view_column_add_attribute(col_col, renderer_col, "background-rgba",
		DIALOG_WPATTERN_COLOR2_COLUMN);

	GtkTreeViewColumn *col_wp = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col_wp, "Wildcard pattern");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col_wp);

	GtkCellRenderer *renderer_wp = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col_wp, renderer_wp, TRUE);
	gtk_tree_view_column_add_attribute(col_wp, renderer_wp, "text",
		DIALOG_WPATTERN_WPATTERN_COLUMN);

	GtkListStore *liststore = gtk_list_store_new(DIALOG_WPATTERN_NUM_COLS,
		G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_RGBA, G_TYPE_POINTER);
	GtkTreeModel *model = GTK_TREE_MODEL(liststore);

	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), TRUE);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
	g_object_unref(model);

	gtk_container_add( GTK_CONTAINER(scrollwin_w), view );
	gtk_widget_show(view);

	return view;
}


/* Internal callback for the color picker widget */
static void
color_picker_cb(GtkColorButton *colorpicker_w, gpointer data)
{
	void (*user_callback)( RGBcolor *, void * );
	GdkRGBA gcolor;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorpicker_w), &gcolor);
	RGBcolor color = GdkRGBA2RGB(&gcolor);

	/* Call user callback */
	user_callback = (void (*)( RGBcolor *, void * ))g_object_get_data(G_OBJECT(colorpicker_w), "user_callback");
	(user_callback)( &color, data );
}


/* The color picker widget. Color is initialized to the one given, and the
 * color selection dialog will have the specified title when brought up.
 * Changing the color (i.e. pressing OK in the color selection dialog)
 * activates the given callback */
GtkWidget *
gui_colorpicker_add( GtkWidget *parent_w, RGBcolor *init_color, const char *title, void (*callback)( ), void *callback_data )
{
	GtkWidget *colorbutton_w;

	colorbutton_w = gtk_color_button_new();
	gui_colorpicker_set_color(colorbutton_w, init_color);
	gtk_color_button_set_title(GTK_COLOR_BUTTON(colorbutton_w), title);
	g_signal_connect(G_OBJECT(colorbutton_w), "color-set", G_CALLBACK(color_picker_cb), callback_data);
	g_object_set_data(G_OBJECT(colorbutton_w), "user_callback", (void *)callback);
	parent_child(parent_w, colorbutton_w);

	return colorbutton_w;
}


/* Sets the color on a color picker widget */
void
gui_colorpicker_set_color( GtkWidget *colorbutton_w, RGBcolor *color )
{
	GdkRGBA gdk_color = RGB2GdkRGBA(color);

	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorbutton_w), &gdk_color);
}


/* Create filelist widget */
GtkWidget *
gui_filelist_new(GtkWidget *parent_w)
{
	GtkWidget *scrollwin_w;

	/* Make the scrolled window widget */
	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Make the tree view widget */
	GtkWidget *view = gtk_tree_view_new();

	GtkTreeViewColumn *col_pb = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col_pb, "Icon");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col_pb);

	GtkCellRenderer *renderer_pb = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col_pb, renderer_pb, TRUE);
	gtk_tree_view_column_add_attribute(col_pb, renderer_pb, "pixbuf",
		FILELIST_ICON_COLUMN);

	GtkTreeViewColumn *col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "File name");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", FILELIST_NAME_COLUMN);

	GtkListStore *liststore = gtk_list_store_new(FILELIST_NUM_COLS,
		GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
	GtkTreeModel *model = GTK_TREE_MODEL(liststore);
	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
	g_object_unref(model);

	gtk_container_add( GTK_CONTAINER(scrollwin_w), view );
	gtk_widget_show(view);

	return view;
}


/* Create filelist scan widget */
GtkWidget *
gui_filelist_scan_new(GtkWidget *parent_w)
{
	GtkWidget *scrollwin_w;

	/* Make the scrolled window widget */
	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Make the tree view widget */
	GtkWidget *view = gtk_tree_view_new();

	GtkTreeViewColumn *col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Icon");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	GtkCellRenderer *renderer_pb = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col, renderer_pb, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer_pb, "pixbuf",
		FILELIST_SCAN_ICON_COLUMN);

	GtkTreeViewColumn *col_fn = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col_fn, "Files found");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col_fn);

	GtkCellRenderer *renderer_fn = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col_fn, renderer_fn, TRUE);
	gtk_tree_view_column_add_attribute(col_fn, renderer_fn, "text",
		FILELIST_SCAN_FOUND_COLUMN);

	GtkTreeViewColumn *col_sz = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col_sz, "Files total size");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col_sz);

	GtkCellRenderer *renderer_sz = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col_sz, renderer_sz, TRUE);
	gtk_tree_view_column_add_attribute(col_sz, renderer_sz, "text",
		FILELIST_SCAN_BYTES_COLUMN);

	GtkListStore *liststore = gtk_list_store_new(FILELIST_SCAN_NUM_COLS,
		GDK_TYPE_PIXBUF, G_TYPE_INT, G_TYPE_INT64);
	GtkTreeModel *model = GTK_TREE_MODEL(liststore);
	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
	g_object_unref(model);

	gtk_container_add( GTK_CONTAINER(scrollwin_w), view );
	gtk_widget_show(view);

	return view;
}

/* The tree widget (fitted into a scrolled window) */
GtkWidget *
gui_tree_add( GtkWidget *parent_w )
{
	GtkWidget *scrollwin_w;

	/* Make the scrolled window widget */
	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Make the tree view widget */
	GtkWidget *view = gtk_tree_view_new();

	GtkTreeViewColumn *col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Directory name");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", DIRTREE_NAME_COLUMN);

	GtkTreeStore *treestore = gtk_tree_store_new(DIRTREE_NUM_COLS, G_TYPE_STRING, G_TYPE_POINTER);
	GtkTreeModel *model = GTK_TREE_MODEL(treestore);
	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
	g_object_unref(model);

	gtk_container_add( GTK_CONTAINER(scrollwin_w), view );
	gtk_widget_show(view);

	return view;
}


/* This creates and adds a new (tree item) to the given tree. Returns a
 * GtkTreePath describing the location in the tree of the new item.
 * GtkWidget *tree_w: the tree widget
 * GtkTreePath *parent: the parent node (NULL if creating a top-level node)
 * Icon icon_pair[2]: two icons, for collapsed ([0]) and expanded ([1]) states
 * const char *text: label for node
 * boolean expanded: initial state of node
 * void *data: arbitrary pointer to associate data with node */
GtkTreePath *
gui_tree_node_add( GtkWidget *tree_w, GtkTreePath *parent, Icon icon_pair[2], const char *text, boolean expanded, GNode *data )
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_w));
	GtkTreeStore *store = GTK_TREE_STORE(model);

	GtkTreeIter it;
	GtkTreeIter parent_it;
	if (parent && gtk_tree_model_get_iter (model, &parent_it, parent))
		gtk_tree_store_append(store, &it, &parent_it);
	else
		gtk_tree_store_append(store, &it, NULL);

	gtk_tree_store_set(store, &it, DIRTREE_NAME_COLUMN, text, DIRTREE_NODE_COLUMN, data, -1);

	return gtk_tree_model_get_path(model, &it);
}


/* Changes the mouse cursor glyph associated with the given widget.
 * A glyph of -1 indicates the default cursor */
void
gui_cursor( GtkWidget *widget, int glyph )
{
	GdkCursor *prev_cursor, *cursor;
	int *prev_glyph;

	/* Get cursor information from widget */
	prev_cursor = g_object_get_data(G_OBJECT(widget), "gui_cursor");
	prev_glyph = g_object_get_data(G_OBJECT(widget), "gui_glyph");

	if (prev_glyph == NULL) {
		if (glyph < 0)
			return; /* default cursor is already set */
                /* First-time setup */
		prev_glyph = NEW(int);
		g_object_set_data_full(G_OBJECT(widget), "gui_glyph", prev_glyph, _xfree);
	}
	else {
		/* Check if requested glyph is same as previous one */
		if (glyph == *prev_glyph)
			return;
	}

	/* Create new cursor and make it active */
	if (glyph >= 0)
		cursor = gdk_cursor_new_for_display(gdk_display_get_default(),
						    (GdkCursorType)glyph);
	else
		cursor = NULL;
	gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);

	/* Don't need the old cursor anymore */
	if (prev_cursor != NULL)
		g_object_unref(prev_cursor);

	if (glyph >= 0) {
		/* Save new cursor information */
		g_object_set_data(G_OBJECT(widget), "gui_cursor", cursor);
		*prev_glyph = glyph;
	}
	else {
		/* Clean up after ourselves */
		g_object_set_data(G_OBJECT(widget), "gui_cursor", NULL);
		g_object_set_data(G_OBJECT(widget), "gui_glyph", NULL);
	}
}


/* The date edit widget (imported from Gnomeland). The given callback is
 * called whenever the date/time is changed */
GtkWidget *
gui_dateedit_add( GtkWidget *parent_w, time_t the_time, void (*callback)( ), void *callback_data )
{
	GtkWidget *dateedit_w = gtk_calendar_new();

	/*dateedit_w = gnome_date_edit_new( the_time, TRUE, TRUE );
	gnome_date_edit_set_popup_range( GNOME_DATE_EDIT(dateedit_w), 0, 23 );
	gtk_signal_connect( GTK_OBJECT(dateedit_w), "date_changed", GTK_SIGNAL_FUNC(callback), callback_data );
	gtk_signal_connect( GTK_OBJECT(dateedit_w), "time_changed", GTK_SIGNAL_FUNC(callback), callback_data );
	parent_child( parent_w, dateedit_w );*/

	return dateedit_w;
}


/* Reads current time from a date edit widget */
time_t
gui_dateedit_get_time( GtkWidget *dateedit_w )
{
	//return gnome_date_edit_get_date( GNOME_DATE_EDIT(dateedit_w) );
	return 0;
}


/* Sets the time on a date edit widget */
void
gui_dateedit_set_time( GtkWidget *dateedit_w, time_t the_time )
{
	//gnome_date_edit_set_time( GNOME_DATE_EDIT(dateedit_w), the_time );
}


/* The entry (text input) widget */
GtkWidget *
gui_entry_add( GtkWidget *parent_w, const char *init_text, void (*callback)( ), void *callback_data )
{
	GtkWidget *entry_w;

	entry_w = gtk_entry_new( );
        if (init_text != NULL)
		gtk_entry_set_text( GTK_ENTRY(entry_w), init_text );
	if (callback != NULL )
		g_signal_connect(G_OBJECT(entry_w), "activate", G_CALLBACK(callback), callback_data);
	parent_child_full( parent_w, entry_w, EXPAND, FILL );

	return entry_w;
}


/* Sets the text in an entry to the specified string */
void
gui_entry_set_text( GtkWidget *entry_w, const char *entry_text )
{
	gtk_entry_set_text( GTK_ENTRY(entry_w), entry_text );
}


/* The frame widget (with optional title) */
GtkWidget *
gui_frame_add( GtkWidget *parent_w, const char *title )
{
	GtkWidget *frame_w;

	frame_w = gtk_frame_new( title );
	parent_child_full( parent_w, frame_w, EXPAND, FILL );

	return frame_w;
}


/* The OpenGL area widget */
GtkWidget *
gui_gl_area_add( GtkWidget *parent_w )
{
	GtkWidget *gl_area_w;
	int bitmask = 0;

	gl_area_w = ogl_widget_new( );
	bitmask |= GDK_EXPOSURE_MASK;
	bitmask |= GDK_POINTER_MOTION_MASK;
	bitmask |= GDK_BUTTON_MOTION_MASK;
	bitmask |= GDK_BUTTON1_MOTION_MASK;
	bitmask |= GDK_BUTTON2_MOTION_MASK;
	bitmask |= GDK_BUTTON3_MOTION_MASK;
	bitmask |= GDK_BUTTON_PRESS_MASK;
	bitmask |= GDK_BUTTON_RELEASE_MASK;
	bitmask |= GDK_LEAVE_NOTIFY_MASK;
	gtk_widget_set_events( GTK_WIDGET(gl_area_w), bitmask );
	parent_child_full( parent_w, gl_area_w, EXPAND, FILL );

	return gl_area_w;
}


/* Sets up keybindings (accelerators). Call this any number of times with
 * widget/keystroke pairs, and when all have been specified, call with the
 * parent window widget (and no keystroke) to attach the keybindings.
 * Keystroke syntax: "K" == K keypress, "^K" == Ctrl-K */
void
gui_keybind( GtkWidget *widget, char *keystroke )
{
	static GtkAccelGroup *accel_group = NULL;
	int mods;
	char key;

	if (accel_group == NULL)
		accel_group = gtk_accel_group_new( );

	if (GTK_IS_WINDOW(widget)) {
		/* Attach keybindings */
		gtk_window_add_accel_group(GTK_WINDOW(widget), accel_group);
		accel_group = NULL;
		return;
	}

	/* Parse keystroke string */
	switch (keystroke[0]) {
		case '^':
		/* Ctrl-something keystroke specified */
		mods = GDK_CONTROL_MASK;
		key = keystroke[1];
		break;

		default:
		/* Simple keypress */
		mods = 0;
		key = keystroke[0];
		break;
	}

	if (GTK_IS_MENU_ITEM(widget)) {
		gtk_widget_add_accelerator( widget, "activate", accel_group, key, mods, GTK_ACCEL_VISIBLE );
		return;
	}
	if (GTK_IS_BUTTON(widget)) {
		gtk_widget_add_accelerator( widget, "clicked", accel_group, key, mods, GTK_ACCEL_VISIBLE );
		return;
	}

	/* Make widget grab focus when its key is pressed */
	gtk_widget_add_accelerator( widget, "grab_focus", accel_group, key, mods, GTK_ACCEL_VISIBLE );
}


/* The label widget */
GtkWidget *
gui_label_add( GtkWidget *parent_w, const char *label_text )
{
	GtkWidget *label_w;
	GtkWidget *hbox_w;

	label_w = gtk_label_new( label_text );
	if (parent_w != NULL) {
		if (GTK_IS_BUTTON(parent_w)) {
			/* Labels are often too snug inside buttons */
			hbox_w = gui_hbox_add( parent_w, 0 );
			gtk_box_pack_start( GTK_BOX(hbox_w), label_w, TRUE, FALSE, 5 );
			gtk_widget_show( label_w );
		}
		else
			parent_child( parent_w, label_w );
	}

	return label_w;
}


/* Adds a menu to a menu bar, or a submenu to a menu */
GtkWidget *
gui_menu_add( GtkWidget *parent_menu_w, const char *label )
{
	GtkWidget *menu_item_w;
	GtkWidget *menu_w;

	menu_item_w = gtk_menu_item_new_with_label( label );
	/* parent_menu can be a menu bar or a regular menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu_w), menu_item_w);

	gtk_widget_show( menu_item_w );
	menu_w = gtk_menu_new( );
	gtk_menu_item_set_submenu( GTK_MENU_ITEM(menu_item_w), menu_w );

	return menu_w;
}


/* Adds a menu item to a menu */
GtkWidget *
gui_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *menu_item_w;

	menu_item_w = gtk_menu_item_new_with_label( label );
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), menu_item_w);
	if (callback != NULL)
		g_signal_connect(G_OBJECT(menu_item_w), "activate", G_CALLBACK(callback), callback_data);
	gtk_widget_show( menu_item_w );

	return menu_item_w;
}


/* This initiates the definition of a radio menu item group. The item in
 * the specified position will be the one that is initially selected
 * (0 == first, 1 == second, and so on) */
void
gui_radio_menu_begin( int init_selected )
{
	gui_radio_menu_item_add( NULL, NULL, NULL, &init_selected );
}


/* Adds a radio menu item to a menu. Don't forget to call
 * gui_radio_menu_begin( ) first.
 * WARNING: When the initially selected menu item is set, the first item
 * in the group will be "toggled" off. The callback should either watch
 * for this, or do nothing if the widget's "active" flag is FALSE */
GtkWidget *
gui_radio_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data )
{
	static GSList *radio_group;
	static int init_selected;
	static int radmenu_item_num;
	GtkWidget *radmenu_item_w = NULL;

	if (menu_w == NULL) {
		/* We're being called from begin_radio_menu_group( ) */
		radio_group = NULL;
		radmenu_item_num = 0;
		init_selected = *((int *)callback_data);
	}
	else {
		radmenu_item_w = gtk_radio_menu_item_new_with_label( radio_group, label );
		radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(radmenu_item_w));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), radmenu_item_w);
		if (radmenu_item_num == init_selected)
			gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(radmenu_item_w), TRUE );
		g_signal_connect(G_OBJECT(radmenu_item_w), "toggled", G_CALLBACK(callback), callback_data);
		gtk_widget_show( radmenu_item_w );
		++radmenu_item_num;
	}

	return radmenu_item_w;
}


/* The notebook widget */
GtkWidget *
gui_notebook_add( GtkWidget *parent_w )
{
	GtkWidget *notebook_w;

	notebook_w = gtk_notebook_new( );
	parent_child_full( parent_w, notebook_w, EXPAND, FILL );

	return notebook_w;
}


/* Adds a new page to a notebook, with the given tab label, and whose
 * content is defined by the given widget */
void
gui_notebook_page_add( GtkWidget *notebook_w, const char *tab_label, GtkWidget *content_w )
{
	GtkWidget *tab_label_w;

	tab_label_w = gtk_label_new( tab_label );
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook_w), content_w, tab_label_w );
	gtk_widget_show( tab_label_w );
	gtk_widget_show( content_w );
}


/* Horizontal paned window widget */
GtkWidget *
gui_hpaned_add( GtkWidget *parent_w, int divider_x_pos )
{
	GtkWidget *hpaned_w;

	hpaned_w = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position( GTK_PANED(hpaned_w), divider_x_pos );
	parent_child_full( parent_w, hpaned_w, EXPAND, FILL );

	return hpaned_w;
}


/* Vertical paned window widget */
GtkWidget *
gui_vpaned_add( GtkWidget *parent_w, int divider_y_pos )
{
	GtkWidget *vpaned_w;

	vpaned_w = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_paned_set_position( GTK_PANED(vpaned_w), divider_y_pos );
	parent_child_full( parent_w, vpaned_w, EXPAND, FILL );

	return vpaned_w;
}


/* The pixbuf widget (created from XPM data) */
GtkWidget *
gui_pixbuf_xpm_add(GtkWidget *parent_w, const char **xpm_data)
{
	/* Realize parent widget to prevent "NULL window" error */
	gtk_widget_realize( parent_w );
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data(xpm_data);
	GtkWidget *pixbuf_w = gtk_image_new();
	gtk_image_set_from_pixbuf(GTK_IMAGE(pixbuf_w), pixbuf);
	parent_child( parent_w, pixbuf_w );

	return pixbuf_w;
}


/* The color spectrum widget */
GtkWidget *
gui_spectrum_new( GtkWidget *parent_w )
{
	GtkWidget *spectrum_w;

	spectrum_w = gtk_image_new();
	gtk_widget_set_size_request(spectrum_w, -1, 40);
	parent_child_full( parent_w, spectrum_w, EXPAND, FILL );

	return spectrum_w;
}


// Delete pixbuf data when it's released
static void
delete_pixbuf(guchar *pixels, gpointer data)
{
	xfree(pixels);
}

/* Helper function for gui_spectrum_fill( ) */
static void
spectrum_draw(GtkWidget *spectrum_w)
{
	RGBcolor (*spectrum_func)( double x );
	RGBcolor color;
	int width, height;
	int i;

	GtkAllocation allocation;
	gtk_widget_get_allocation(spectrum_w, &allocation);
	width = allocation.width;
	height = allocation.height;

	if (!gtk_widget_is_drawable(spectrum_w))
		return;

	/* Get spectrum function */
	spectrum_func = (RGBcolor (*)( double x ))g_object_get_data(G_OBJECT(spectrum_w), "spectrum_func");

	/* Create the spectrum image */
	guchar *imgbuf = NEW_ARRAY(guchar, 3 * width * height);
	// Iterate in column major order, with the assumption that
	// spectrum_func() is more expensive than the cache inefficiency.
	for (i = 0; i < width; i++) {
		color = (spectrum_func)( (double)i / (double)(width - 1) ); /* struct assign */
		for (int j = 0; j < height; j++) {
			imgbuf[3*j*width + 3 * i] = (unsigned char)(255.0 * color.r);
			imgbuf[3*j*width + 3 * i + 1] = (unsigned char)(255.0 * color.g);
			imgbuf[3*j*width + 3 * i + 2] = (unsigned char)(255.0 * color.b);
		}
	}

	/* Draw spectrum into spectrum widget, row by row */
	GdkPixbuf *pb = gdk_pixbuf_new_from_data(imgbuf, GDK_COLORSPACE_RGB,
						 FALSE, 8, width, height,
						 width * 3, delete_pixbuf, NULL);
	gtk_image_set_from_pixbuf(GTK_IMAGE(spectrum_w), pb);
}


/* Fills a spectrum widget with an arbitrary spectrum. Second argument
 * should be a function returning the appropriate color at a specified
 * fractional position in the spectrum */
void
gui_spectrum_fill( GtkWidget *spectrum_w, RGBcolor (*spectrum_func)( double x ) )
{
	static const char data_key[] = "spectrum_func";

	/* Attach spectrum function to spectrum widget */
	g_object_set_data(G_OBJECT(spectrum_w), data_key, (void *)spectrum_func);

	spectrum_draw(spectrum_w);
}


/* The horizontal scrollbar widget */
GtkWidget *
gui_hscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *frame_w;
	GtkWidget *hscrollbar_w;

	/* Make a nice-looking frame to put the scrollbar in */
	frame_w = gui_frame_add( NULL, NULL );
	parent_child( parent_w, frame_w );

	hscrollbar_w = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
	gtk_container_add( GTK_CONTAINER(frame_w), hscrollbar_w );
	gtk_widget_show( hscrollbar_w );

	return hscrollbar_w;
}


/* The vertical scrollbar widget */
GtkWidget *
gui_vscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *frame_w;
	GtkWidget *vscrollbar_w;

	/* Make a nice-looking frame to put the scrollbar in */
	frame_w = gui_frame_add( NULL, NULL );
	parent_child( parent_w, frame_w );

	vscrollbar_w = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, adjustment);
	gtk_container_add( GTK_CONTAINER(frame_w), vscrollbar_w );
	gtk_widget_show( vscrollbar_w );

	return vscrollbar_w;
}


/* The (ever-ubiquitous) separator widget */
GtkWidget *
gui_separator_add( GtkWidget *parent_w )
{
	GtkWidget *separator_w;

	if (parent_w != NULL) {
		if (GTK_IS_MENU(parent_w)) {
			separator_w = gtk_menu_item_new( );
			gtk_menu_shell_append(GTK_MENU_SHELL(parent_w), separator_w);
		}
		else {
			separator_w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
			gtk_box_pack_start( GTK_BOX(parent_w), separator_w, FALSE, FALSE, 10 );
		}
		gtk_widget_show( separator_w );
	}
	else
		separator_w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

	return separator_w;
}


/* The statusbar widget */
GtkWidget *
gui_statusbar_add( GtkWidget *parent_w )
{
	GtkWidget *statusbar_w;

	statusbar_w = gtk_statusbar_new( );
	parent_child( parent_w, statusbar_w );

	return statusbar_w;
}


/* Displays the given message in the given statusbar widget */
void
gui_statusbar_message( GtkWidget *statusbar_w, const char *message )
{
	char strbuf[1024];

	gtk_statusbar_pop( GTK_STATUSBAR(statusbar_w), 1 );
	/* Prefix a space so that text doesn't touch left edge */
	snprintf( strbuf, sizeof(strbuf), " %s", message );
	gtk_statusbar_push( GTK_STATUSBAR(statusbar_w), 1, strbuf );
}


/* The table (layout) widget */
GtkWidget *
gui_table_add( GtkWidget *parent_w, int num_rows, int num_cols, boolean homog, int cell_padding )
{
	GtkWidget *grid_w = gtk_grid_new();
	GtkGrid *grid = GTK_GRID(grid_w);
	for (int i = 0; i < num_rows; i++)
		gtk_grid_insert_row(grid, i);
	for (int i = 0; i < num_cols; i++)
		gtk_grid_insert_column(grid, i);
	gtk_grid_set_column_homogeneous(grid, homog);
	gtk_grid_set_column_spacing(grid, cell_padding);
	parent_child_full( parent_w, grid_w, EXPAND, FILL );

	return grid_w;
}


/* Attaches a widget to a table */
void
gui_table_attach( GtkWidget *table_w, GtkWidget *widget, int left, int right, int top, int bottom )
{
	GtkGrid *grid = GTK_GRID(table_w);
	gtk_grid_attach(grid, widget, left, top, right - left, bottom - top);

	gtk_widget_show( widget );
}


/* The text (area) widget, optionally initialized with text */
GtkWidget *
gui_text_area_add( GtkWidget *parent_w, const char *init_text )
{
	GtkWidget *text_area_w;

	/* Text (area) widget */
	text_area_w = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_area_w), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_area_w), GTK_WRAP_WORD);
	if (init_text != NULL) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_area_w));
		gtk_text_buffer_set_text(buffer, init_text, -1);
	}
	parent_child( parent_w, text_area_w );

	return text_area_w;
}


/* This changes the packing flags of a widget inside a box widget. This
 * allows finer control than gtk_box_set_packing( ) (as this only affects
 * a single widget) */
void
gui_widget_packing( GtkWidget *widget, boolean expand, boolean fill, boolean start )
{
	GtkWidget *parent_box_w;

	parent_box_w = gtk_widget_get_parent(widget);
	g_assert( GTK_IS_BOX(parent_box_w) );

	gtk_box_set_child_packing( GTK_BOX(parent_box_w), widget, expand, fill, 0, start ? GTK_PACK_START : GTK_PACK_END );
}


/* Creates a color selection window. OK button activates ok_callback */
void
gui_colorsel_window( const char *title, RGBcolor *init_color, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *widget = gtk_color_chooser_dialog_new(title, NULL);
	GtkColorChooser *chooser = GTK_COLOR_CHOOSER(widget);
	GdkRGBA gcolor = RGB2GdkRGBA(init_color);
	gtk_color_chooser_set_rgba(chooser, &gcolor);

	if (gtk_dialog_run(GTK_DIALOG(widget)) == GTK_RESPONSE_OK) {
		gtk_color_chooser_get_rgba(chooser, &gcolor);
		RGBcolor color = GdkRGBA2RGB(&gcolor);
		/* Call user callback */
		(ok_callback)(&color, ok_callback_data);
        }

#if GTK_MAJOR_VERSION >= 4
	gtk_window_destroy(widget);
#else
	gtk_widget_destroy(widget);
#endif
}


/* Creates a base dialog window. close_callback is called when the
 * window is destroyed */
GtkWidget *
gui_dialog_window( const char *title, void (*close_callback)( ) )
{
	GtkWidget *window_w;

	window_w = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_resizable(GTK_WINDOW(window_w), FALSE);
	gtk_window_set_position( GTK_WINDOW(window_w), GTK_WIN_POS_CENTER );
	gtk_window_set_title( GTK_WINDOW(window_w), title );
	if (close_callback != NULL)
		g_signal_connect(G_OBJECT(window_w), "destroy", G_CALLBACK(close_callback), NULL);
	/* !gtk_widget_show( ) */

	return window_w;
}


/* Internal callback for the text-entry window, called when the
 * OK button is pressed */
static void
entry_window_cb( GtkWidget *unused, GtkWidget *entry_window_w )
{
	GtkWidget *entry_w;
	char *entry_text;
	void (*user_callback)( const char *, void * );
	void *user_callback_data;

	entry_w = g_object_get_data(G_OBJECT(entry_window_w), "entry_w");
	entry_text = xstrdup( gtk_entry_get_text( GTK_ENTRY(entry_w) ) );

	user_callback = (void (*)( const char *, void * ))g_object_get_data(G_OBJECT(entry_window_w), "user_callback");
	user_callback_data = g_object_get_data(G_OBJECT(entry_window_w), "user_callback_data");
#if GTK_MAJOR_VERSION >= 4
	gtk_window_destroy(entry_window_w);
#else
	gtk_widget_destroy( entry_window_w );
#endif

	/* Call user callback */
	(user_callback)( entry_text, user_callback_data );
        xfree( entry_text );
}


/* Creates a one-line text-entry window, initialized with the given text
 * string. OK button activates ok_callback */
GtkWidget *
gui_entry_window( const char *title, const char *init_text, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *entry_window_w;
	GtkWidget *frame_w;
	GtkWidget *vbox_w;
	GtkWidget *entry_w;
	GtkWidget *hbox_w;
	GtkWidget *button_w;
        int width;

	entry_window_w = gui_dialog_window( title, NULL );
	gtk_container_set_border_width( GTK_CONTAINER(entry_window_w), 5 );
	width = 500; // TODO: Some better way to determine this
	gtk_widget_set_size_request(entry_window_w, width, 100);
	g_object_set_data(G_OBJECT(entry_window_w), "user_callback", (void *)ok_callback);
	g_object_set_data(G_OBJECT(entry_window_w), "user_callback_data", ok_callback_data);

	frame_w = gui_frame_add( entry_window_w, NULL );
	vbox_w = gui_vbox_add( frame_w, 10 );

        /* Text entry widget */
	entry_w = gui_entry_add( vbox_w, init_text, entry_window_cb, entry_window_w );
	g_object_set_data(G_OBJECT(entry_window_w), "entry_w", entry_w);

	/* Horizontal box for buttons */
	hbox_w = gui_hbox_add( vbox_w, 0 );
	gtk_box_set_homogeneous( GTK_BOX(hbox_w), TRUE );
	gui_box_set_packing( hbox_w, EXPAND, FILL, AT_START );

	/* OK/Cancel buttons */
	gui_button_add( hbox_w, _("OK"), entry_window_cb, entry_window_w );
	vbox_w = gui_vbox_add( hbox_w, 0 ); /* spacer */
	button_w = gui_button_add( hbox_w, _("Cancel"), NULL, NULL );
	g_signal_connect_swapped(button_w, "clicked",
#if GTK_MAJOR_VERSION >= 4
		G_CALLBACK(gtk_window_destroy),
#else
		G_CALLBACK(gtk_widget_destroy),
#endif
		 entry_window_w);

	gtk_widget_show( entry_window_w );
	gtk_widget_grab_focus( entry_w );

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(entry_window_w), TRUE );

	return entry_window_w;
}


/* Creates a file selection window, with an optional default filename.
 * OK button activates ok_callback */
gchar *
gui_dir_choose( const char *title, GtkWidget *parent, const char *init_dir)
{
	GtkWidget *file_dialog = gtk_file_chooser_dialog_new(title,
		GTK_WINDOW(parent),
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		"Cancel", GTK_RESPONSE_CANCEL,
		"Open", GTK_RESPONSE_ACCEPT,
		NULL);
	if (init_dir != NULL)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_dialog), init_dir);

	gchar *dirname = NULL;
	if (gtk_dialog_run(GTK_DIALOG(file_dialog)) == GTK_RESPONSE_ACCEPT) {
		dirname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_dialog));
	}

#if GTK_MAJOR_VERSION >= 4
	gtk_window_destroy(file_dialog);
#else
	gtk_widget_destroy(file_dialog);
#endif
	return dirname;
}


/* Associates an icon (created from XPM data) to a window */
void
gui_window_icon_xpm(GtkWidget *window_w, const char **xpm_data)
{
	GdkPixbuf *pb = gdk_pixbuf_new_from_xpm_data(xpm_data);
	gtk_window_set_icon(GTK_WINDOW(window_w), pb);
}


/* Helper function for gui_window_modalize( ), called upon the destruction
 * of the modal window */
static void
window_unmodalize(GtkWidget *self, gpointer data)
{
	GtkWidget *parent_window_w = GTK_WIDGET(data);
	gtk_widget_set_sensitive( parent_window_w, TRUE );
	gui_cursor( parent_window_w, -1 );
}


/* Makes a window modal w.r.t its parent window */
void
gui_window_modalize( GtkWidget *window_w, GtkWidget *parent_window_w )
{
	gtk_window_set_transient_for( GTK_WINDOW(window_w), GTK_WINDOW(parent_window_w) );
	gtk_window_set_modal( GTK_WINDOW(window_w), TRUE );
	gtk_widget_set_sensitive( parent_window_w, FALSE );
	gui_cursor( parent_window_w, GDK_X_CURSOR );

	/* Restore original state once the window is destroyed */
	g_signal_connect(G_OBJECT(window_w), "destroy", G_CALLBACK(window_unmodalize), parent_window_w);
}


RGBcolor
GdkRGBA2RGB(const GdkRGBA *color)
{
	RGBcolor c;
	c.r = color->red;
	c.g = color->green;
	c.b = color->blue;
	return c;
}

GdkRGBA
RGB2GdkRGBA(const RGBcolor *src)
{
	GdkRGBA dest;
	dest.red = src->r;
	dest.green = src->g;
	dest.blue = src->b;
	dest.alpha = 1.0;
	return dest;
}

#if 0
/* The following is stuff that isn't being used right now (obviously),
 * but may be in the future. TODO: Delete this section by v1.0! */


/* The check button widget */
GtkWidget *
gui_check_button_add( GtkWidget *parent_w, const char *label, boolean init_state, void (*callback)( ), void *callback_data )
{
	GtkWidget *cbutton_w;

	cbutton_w = gtk_check_button_new_with_label( label );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cbutton_w), init_state );
	gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(cbutton_w), TRUE );
	if (callback != NULL)
		gtk_signal_connect( GTK_OBJECT(cbutton_w), "toggled", GTK_SIGNAL_FUNC(callback), callback_data );
	parent_child( parent_w, cbutton_w );

	return cbutton_w;
}


/* Adds a check menu item to a menu */
GtkWidget *
gui_check_menu_item_add( GtkWidget *menu_w, const char *label, boolean init_state, void (*callback)( ), void *callback_data )
{
	GtkWidget *chkmenu_item_w;

	chkmenu_item_w = gtk_check_menu_item_new_with_label( label );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(chkmenu_item_w), init_state );
	gtk_check_menu_item_set_show_toggle( GTK_CHECK_MENU_ITEM(chkmenu_item_w), TRUE );
	gtk_menu_append( GTK_MENU(menu_w), chkmenu_item_w );
	gtk_signal_connect( GTK_OBJECT(chkmenu_item_w), "toggled", GTK_SIGNAL_FUNC(callback), callback_data );
	gtk_widget_show( chkmenu_item_w );

	return chkmenu_item_w;
}


/* Resizes an entry to fit the width of the specified string */
void
gui_entry_set_width( GtkWidget *entry_w, const char *str )
{
	GtkStyle *style;
	int width;

	style = gtk_widget_get_style( entry_w );
	width = gdk_string_width( style->font, str );
	gtk_widget_set_usize( entry_w, width + 16, 0 );
}


/* The spin button widget */
GtkWidget *
gui_spin_button_add( GtkWidget *parent_w, GtkAdjustment *adj )
{
	GtkWidget *spinbtn_w;

	spinbtn_w = gtk_spin_button_new( adj, 0.0, 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), spinbtn_w, FALSE, FALSE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), spinbtn_w );
	gtk_widget_show( spinbtn_w );

	return spinbtn_w;
}


/* Returns the width of string, when drawn in the given widget */
int
gui_string_width( const char *str, GtkWidget *widget )
{
	GtkStyle *style;
	style = gtk_widget_get_style( widget );
	return gdk_string_width( style->font, str );
}


/* The horizontal value slider widget */
GtkWidget *
gui_hscale_add( GtkWidget *parent_w, GtkObject *adjustment )
{
	GtkWidget *hscale_w;

	hscale_w = gtk_hscale_new( GTK_ADJUSTMENT(adjustment) );
	gtk_scale_set_digits( GTK_SCALE(hscale_w), 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), hscale_w, TRUE, TRUE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), hscale_w );
	gtk_widget_show( hscale_w );

	return hscale_w;
}


/* The vertical value slider widget */
GtkWidget *
gui_vscale_add( GtkWidget *parent_w, GtkObject *adjustment )
{
	GtkWidget *vscale_w;

	vscale_w = gtk_vscale_new( GTK_ADJUSTMENT(adjustment) );
	gtk_scale_set_value_pos( GTK_SCALE(vscale_w), GTK_POS_RIGHT );
	gtk_scale_set_digits( GTK_SCALE(vscale_w), 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), vscale_w, TRUE, TRUE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), vscale_w );
	gtk_widget_show( vscale_w );

	return vscale_w;
}


/* Associates a tooltip with a widget */
void
gui_tooltip_add( GtkWidget *widget, const char *tip_text )
{
	static GtkTooltips *tooltips = NULL;

	if (tooltips == NULL) {
		tooltips = gtk_tooltips_new( );
		gtk_tooltips_set_delay( tooltips, 2000 );
	}
	gtk_tooltips_set_tip( tooltips, widget, tip_text, NULL );
}
#endif /* 0 */


/* end gui.c */
