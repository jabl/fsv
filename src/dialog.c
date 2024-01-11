/* dialog.c */

/* Dialog windows */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist  <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "dialog.h"

#include <time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <errno.h>

#include "animation.h"
#include "camera.h"
#include "colexp.h"
#include "color.h"
#include "dirtree.h" /* dirtree_entry_expanded( ) */
#include "filelist.h" /* dir_contents_list_add( ) */
#include "fsv.h"
#include "gui.h"
#include "window.h"

/* OK/Cancel button XPM's */
#include "xmaps/button_ok.xpm"
#include "xmaps/button_cancel.xpm"


/* Main window widget */
static GtkWidget *main_window_w;


/* Correspondence from window_init( ) */
void
dialog_pass_main_window_widget( GtkWidget *window_w )
{
	main_window_w = window_w;
}


/* Callback to close a dialog window */
static void
close_cb( GtkWidget *unused, GtkWidget *window_w )
{
	gtk_widget_destroy( window_w );
}


/* End callback to allow time-bombed transient dialogs */
static void
transient_end_cb( Morph *morph )
{
	GtkWidget *window_w;

	window_w = (GtkWidget *)morph->data;
	gtk_widget_destroy( window_w );

	/* Restore normal mouse cursor */
	gui_cursor( main_window_w, -1 );
}


/**** File -> Change root... ****/

void
dialog_change_root( void )
{
	const char *root_name;
	char *dir;

	/* Build initial directory name (with trailing slash) */
	root_name = node_absname( root_dnode );
	dir = NEW_ARRAY(char, strlen( root_name ) + 2);
	strcpy( dir, root_name );
	strcat( dir, "/" );

	/* On networked filesystems, the file selection window can be
	 * sloooow in coming up (as each directory component in the default
	 * location has to be stat( )'ed-- takes >10 sec on MIT AFS!) */
	gui_cursor( main_window_w, GDK_WATCH );
	gui_update( );

	gchar *new_root = gui_dir_choose(_("Change Root Directory"), main_window_w, dir);
	xfree( dir );
	if (new_root && globals.fsv_mode != FSV_SPLASH)
		fsv_load(new_root);

	g_free(new_root);

	gui_cursor( main_window_w, -1 );
	gui_update( );
}


/**** Colors -> Setup... ****/

/* Types of rows in the wildcard pattern list
 * (for "row_type" field in struct WPListRowData) */
enum {
	WPLIST_HEADER_ROW,
	WPLIST_WPATTERN_ROW,
	WPLIST_NEW_WPATTERN_ROW,
	WPLIST_DEFAULT_HEADER_ROW,
	WPLIST_DEFAULT_ROW
};

/* Data associated with each row of the wildcard pattern list */
struct WPListRowData {
	int row_type;
	struct WPatternGroup *wpgroup;
	char *wpattern;
};

static struct ColorSetupDialog {
	/* Scratch copy of color configuration */
	struct ColorConfig color_config;

	/* Notebook widget (each page dedicated to a color mode) */
	GtkWidget *notebook_w;

	/* Node type configuration page */
	/* (doesn't have any widgets we need to keep) */

	/* Date/time configuration page */
	struct {
		/* Date edit widgets */
		GtkWidget *old_dateedit_w;
		GtkWidget *new_dateedit_w;

		/* Spectrum widget */
		GtkWidget *spectrum_w;

		/* Color pickers for interpolated spectrum setup */
		GtkWidget *old_colorpicker_w;
		GtkWidget *new_colorpicker_w;
	} time;

	/* Wildcard pattern configuration page */
	struct {
		/* Wildcard pattern list widget */
		GtkWidget *list_w;

		/* Flag: TRUE when a row in the list is being dragged */
		boolean row_is_being_dragged;

		/* Action buttons */
		GtkWidget *new_color_button_w;
		GtkWidget *edit_pattern_button_w;
		GtkWidget *delete_button_w;
	} wpattern;
} csdialog;

static const char *time_last_access;
static const char *time_last_modification;
static const char *time_last_change;

static const char *spectrum_gradient;
static const char *spectrum_rainbow;
static const char *spectrum_heat;


/* Callback for the node type color pickers */
static void
csdialog_node_type_color_picker_cb( RGBcolor *picked_color, RGBcolor *node_type_color )
{
	/* node_type_color points to the appropriate member of
	 * csdialog.color_config.by_nodetype.colors[] */
	node_type_color->r = picked_color->r;
	node_type_color->g = picked_color->g;
	node_type_color->b = picked_color->b;
}


/* Callback for the date edit widgets on the "By date/time" page */
static void
csdialog_time_edit_cb( GtkWidget *dateedit_w )
{
	time_t old_time, new_time;
	time_t cur_time;

	old_time = gui_dateedit_get_time( csdialog.time.old_dateedit_w );
	new_time = gui_dateedit_get_time( csdialog.time.new_dateedit_w );
	cur_time = time( NULL );

	/* Check that neither time is in the future */
	if (difftime( cur_time, new_time ) < 0.0)
		new_time = cur_time;
	if (difftime( cur_time, old_time ) < 0.0)
		old_time = cur_time;

	/* Check that old time is at least one minute before new time */
	if (difftime( new_time, old_time ) < 60.0) {
		if (dateedit_w == csdialog.time.old_dateedit_w)
			new_time = old_time + (time_t)60;
		else if (dateedit_w == csdialog.time.new_dateedit_w)
			old_time = new_time - (time_t)60;
		else {
			g_assert_not_reached( );
			return;
		}
	}

	/* Reset old and new times */
	g_signal_handlers_block_by_func(G_OBJECT(csdialog.time.old_dateedit_w), G_CALLBACK(csdialog_time_edit_cb), NULL);
	g_signal_handlers_block_by_func(G_OBJECT(csdialog.time.new_dateedit_w), G_CALLBACK(csdialog_time_edit_cb), NULL);
	gui_dateedit_set_time( csdialog.time.old_dateedit_w, old_time );
	gui_dateedit_set_time( csdialog.time.new_dateedit_w, new_time );
	g_signal_handlers_unblock_by_func(G_OBJECT(csdialog.time.old_dateedit_w), G_CALLBACK(csdialog_time_edit_cb), NULL);
	g_signal_handlers_unblock_by_func(G_OBJECT(csdialog.time.new_dateedit_w), G_CALLBACK(csdialog_time_edit_cb), NULL);

	csdialog.color_config.by_timestamp.old_time = old_time;
	csdialog.color_config.by_timestamp.new_time = new_time;
}


/* Callback for the "Color by:" timestamp combobox */
static void
csdialog_time_timestamp_combobox_changed(GtkComboBox *combobox, gpointer user_data)
{
	char *selected = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combobox));
	if (!selected)
		return;

	TimeStampType type;

	if (strcmp(selected, time_last_access) == 0)
		type = TIMESTAMP_ACCESS;
	else if (strcmp(selected, time_last_modification) == 0)
		type = TIMESTAMP_MODIFY;
	else if (strcmp(selected, time_last_change) == 0)
		type = TIMESTAMP_ATTRIB;
	else {
		g_error("selected = %s\n", selected);
		g_assert_not_reached( );
		return;
	}

	csdialog.color_config.by_timestamp.timestamp_type = type;
}


/* This is the spectrum function used to paint the spectrum widget */
static RGBcolor
csdialog_time_spectrum_func( double x )
{
	RGBcolor *boundary_colors[2];
	void *data = NULL;

	if (csdialog.color_config.by_timestamp.spectrum_type == SPECTRUM_GRADIENT) {
		boundary_colors[0] = &csdialog.color_config.by_timestamp.old_color;
		boundary_colors[1] = &csdialog.color_config.by_timestamp.new_color;
		data = boundary_colors;
	}

	return color_spectrum_color( csdialog.color_config.by_timestamp.spectrum_type, x, data );
}


/* Helper function for spectrum_option_menu_cb( ). This enables or
 * disables the color picker buttons as necessary */
static void
csdialog_time_color_picker_set_access( boolean enabled )
{
	RGBcolor *color;

	gtk_widget_set_sensitive( csdialog.time.old_colorpicker_w, enabled );
	gtk_widget_set_sensitive( csdialog.time.new_colorpicker_w, enabled );

	/* Change the color pickers' colors, as simply enabling/disabling
	 * them isn't enough to make the state change obvious */
	if (enabled) {
		gtk_widget_show(csdialog.time.old_colorpicker_w);
		gtk_widget_show(csdialog.time.new_colorpicker_w);
		color = &csdialog.color_config.by_timestamp.old_color;
		gui_colorpicker_set_color( csdialog.time.old_colorpicker_w, color );
		color = &csdialog.color_config.by_timestamp.new_color;
		gui_colorpicker_set_color( csdialog.time.new_colorpicker_w, color );
	}
	else {
		gtk_widget_hide(csdialog.time.old_colorpicker_w);
		gtk_widget_hide(csdialog.time.new_colorpicker_w);
	}
}


/* Callback for the spectrum type combo box */
static void
csdialog_time_spectrum_combobox_changed(GtkComboBox *cbox, gpointer user_data)
{
	char *selected = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(cbox));
	if (!selected)
		return;

	SpectrumType type;

	if (strcmp(selected, spectrum_rainbow) == 0)
		type = SPECTRUM_RAINBOW;
	else if (strcmp(selected, spectrum_heat) == 0)
		type = SPECTRUM_HEAT;
	else if (strcmp(selected, spectrum_gradient) == 0)
		type = SPECTRUM_GRADIENT;
	else {
		g_assert_not_reached( );
		return;
	}

	/* Set new spectrum type and draw it */
	csdialog.color_config.by_timestamp.spectrum_type = type;
	gui_spectrum_fill(csdialog.time.spectrum_w, csdialog_time_spectrum_func);
	csdialog_time_color_picker_set_access( type == SPECTRUM_GRADIENT );
}


/* Callback for the spectrum's color picker buttons */
static void
csdialog_time_color_picker_cb( RGBcolor *picked_color, RGBcolor *end_color )
{
	/* end_color points to either old_color or new_color in
	 * csdialog.color_config.by_timestamp */
	end_color->r = picked_color->r;
	end_color->g = picked_color->g;
	end_color->b = picked_color->b;

	/* Redraw spectrum */
	gui_spectrum_fill(csdialog.time.spectrum_w, csdialog_time_spectrum_func);
}


/* Helper function for csdialog_wpattern_list_populate( ). This adds a
 * new row to the wildcard pattern list at the given index (replacing a
 * row if one is already there) */
static void
wplist_row(GtkListStore *store, GtkTreeIter *iter, struct WPListRowData *row_data )
{
	char *rowtext;

	/* Determine textual content of new row */
	switch (row_data->row_type) {
		case WPLIST_WPATTERN_ROW:
		rowtext = row_data->wpattern;
		break;

		case WPLIST_NEW_WPATTERN_ROW:
		rowtext = _("(New pattern)");
		break;

		case WPLIST_DEFAULT_ROW:
		rowtext = _("(Default color)");
		break;

		default:
		rowtext = NULL;
		break;
	}

	RGBcolor *color;
	if (row_data && row_data->wpgroup)
		color = &row_data->wpgroup->color;
	else
		color = &csdialog.color_config.by_wpattern.default_color;
	GdkRGBA *gdk_rgba = NEW(GdkRGBA);
	*gdk_rgba = RGB2GdkRGBA(color);

	gtk_list_store_set(store, iter,
			   DIALOG_WPATTERN_WPATTERN_COLUMN, rowtext,
			   DIALOG_WPATTERN_COLOR2_COLUMN, gdk_rgba,
			   DIALOG_WPATTERN_ROWDATA_COLUMN, row_data,
			   -1);

	/* Set row properties appropriately */
	// switch (row_data->row_type) {
	// 	case WPLIST_HEADER_ROW:
	// 	case WPLIST_DEFAULT_HEADER_ROW:
	// 	gtk_clist_set_row_style( GTK_CLIST(csdialog.wpattern.list_w), row, row_data->style );
	// 	gtk_clist_set_selectable( GTK_CLIST(csdialog.wpattern.list_w), row, FALSE );
	// 	break;

	// 	case WPLIST_WPATTERN_ROW:
	// 	case WPLIST_NEW_WPATTERN_ROW:
	// 	case WPLIST_DEFAULT_ROW:
	// 	gtk_clist_set_cell_style( GTK_CLIST(csdialog.wpattern.list_w), row, 0, row_data->style );
	// 	break;

	// 	SWITCH_FAIL
	// }
	// gtk_clist_set_row_data_full( GTK_CLIST(csdialog.wpattern.list_w), row, row_data, _xfree );
}


/* Updates the wildcard pattern list with state in
 * csdialog.color_config.by_wpattern */
static void
csdialog_wpattern_list_populate( void )
{
	struct WPatternGroup *wpgroup;
	struct WPListRowData *row_data;
	GList *wpgroup_llink, *wp_llink;
	char *wpattern;

	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(csdialog.wpattern.list_w));
	GtkListStore *store = GTK_LIST_STORE(model);
	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_get_iter_first(model, &iter);

	/* Iterate through all the wildcard pattern color groups */
	wpgroup_llink = csdialog.color_config.by_wpattern.wpgroup_list;
	while (wpgroup_llink != NULL) {
		wpgroup = (struct WPatternGroup *)wpgroup_llink->data;

		if (!valid) {
			gtk_list_store_append(store, &iter);
		}

		/* Add header row */
		row_data = NEW(struct WPListRowData);
		row_data->row_type = WPLIST_HEADER_ROW;
		row_data->wpgroup = wpgroup;
		row_data->wpattern = NULL;
		wplist_row(store, &iter, row_data);

		/* Iterate through all the patterns in this group */
		wp_llink = wpgroup->wp_list;
		while (wp_llink != NULL) {
			wpattern = (char *)wp_llink->data;

			valid = gtk_tree_model_iter_next(model, &iter);
			if (!valid)
				gtk_list_store_append(store, &iter);

			/* Add wildcard pattern row */
			row_data = NEW(struct WPListRowData);
			row_data->row_type = WPLIST_WPATTERN_ROW;
			row_data->wpgroup = wpgroup;
			row_data->wpattern = wpattern;
			wplist_row(store, &iter, row_data);

			wp_llink = wp_llink->next;
		}

		/* Add a "New pattern" row for adding new patterns
		 * to this color group */
		valid = gtk_tree_model_iter_next(model, &iter);
		if (!valid) {
			gtk_list_store_append(store, &iter);
			valid = TRUE;
		}
		row_data = NEW(struct WPListRowData);
		row_data->row_type = WPLIST_NEW_WPATTERN_ROW;
		row_data->wpgroup = wpgroup;
		row_data->wpattern = NULL;
		wplist_row(store, &iter, row_data);

		wpgroup_llink = wpgroup_llink->next;
	}

	/* Add default-color header row */
	if (valid && csdialog.color_config.by_wpattern.wpgroup_list != NULL)
		valid = gtk_tree_model_iter_next(model, &iter);
	if (!valid)
		gtk_list_store_append(store, &iter);
	row_data = NEW(struct WPListRowData);
	row_data->row_type = WPLIST_DEFAULT_HEADER_ROW;
	row_data->wpgroup = NULL;
	row_data->wpattern = NULL;
	wplist_row(store, &iter, row_data);

	/* Add default-color row */
	valid = gtk_tree_model_iter_next(model, &iter);
	if (!valid)
		gtk_list_store_append(store, &iter);
	row_data = NEW(struct WPListRowData);
	row_data->row_type = WPLIST_DEFAULT_ROW;
	row_data->wpgroup = NULL;
	row_data->wpattern = NULL;
	wplist_row(store, &iter, row_data);

	/* Remove any leftover rows */
	valid = gtk_tree_model_iter_next(model, &iter);
	while (valid)
		valid = gtk_list_store_remove(store, &iter);
}


/* Callback for the color selection dialog popped up by clicking on a
 * color bar in the wildcard pattern list */
static void
csdialog_wpattern_color_selection_cb( RGBcolor *selected_color, RGBcolor *wpattern_color )
{
	/* wpattern_color points to the appropriate color record
	 * somewhere inside csdialog.color_config.by_wpattern */
	wpattern_color->r = selected_color->r;
	wpattern_color->g = selected_color->g;
	wpattern_color->b = selected_color->b;

	/* Update the list */
	csdialog_wpattern_list_populate( );
}


/* Callback for mouse button release in the wildcard pattern list */
static gboolean
csdialog_wpattern_list_click_cb( GtkWidget *list_w, GdkEventButton *ev_button, gpointer user_data)
{
	struct WPListRowData *row_data = NULL;
        RGBcolor *color;
	const char *title;

	/* Ignore button release following a row drag */
	if (csdialog.wpattern.row_is_being_dragged) {
		csdialog.wpattern.row_is_being_dragged = FALSE;
		return FALSE;
	}

	/* Respond only to mouse button 1 (left button) */
	guint button;
	if (!gdk_event_get_button((GdkEvent*) ev_button, &button))
		return FALSE;
	else if (button != 1)
		return FALSE;

	GtkTreeSelection *select
                        = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_w));
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid = gtk_tree_selection_get_selected(select, &model, &iter);
	if (!valid)
		return FALSE;

	gtk_tree_model_get(model, &iter,
		DIALOG_WPATTERN_ROWDATA_COLUMN, &row_data,
		-1);
	if (!row_data)
		return FALSE;

	switch (row_data->row_type) {
		case WPLIST_WPATTERN_ROW:
		case WPLIST_NEW_WPATTERN_ROW:
		return FALSE;
		case WPLIST_HEADER_ROW:
		title = _("Group Color");
		color = &row_data->wpgroup->color;
		break;

		case WPLIST_DEFAULT_ROW:
		return FALSE;
		case WPLIST_DEFAULT_HEADER_ROW:
		title = _("Default Color");
		color = &csdialog.color_config.by_wpattern.default_color;
		break;

		SWITCH_FAIL
	}

	/* Bring up color selection dialog */
	gui_colorsel_window( title, color, csdialog_wpattern_color_selection_cb, color );

	return FALSE;
}


/* Callback for row selection/unselection in the wildcard pattern list */
static void
csdialog_wpattern_list_select_unselect_cb(GtkTreeSelection *selection, gpointer data)
{
	struct WPListRowData *row_data = NULL;
	boolean newwp_row = FALSE;
	boolean defcolor_row = FALSE;
	boolean empty_wpgroup = FALSE;
	boolean row_selected;
	boolean new_color_allow;
	boolean edit_pattern_allow;
	boolean delete_allow;
	GtkTreeIter iter;
        GtkTreeModel *model;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter,
			DIALOG_WPATTERN_ROWDATA_COLUMN, &row_data, -1);

		//g_signal_stop_emission_by_name(G_OBJECT(selection), "changed" );
	}

	/* Set some flags */
	if (row_data) {
		newwp_row = row_data->row_type == WPLIST_NEW_WPATTERN_ROW;
		defcolor_row = (row_data->row_type == WPLIST_DEFAULT_ROW) || (row_data->row_type == WPLIST_DEFAULT_HEADER_ROW);
		if (!defcolor_row)
			empty_wpgroup = row_data->wpgroup->wp_list == NULL;
		row_selected = TRUE;
	} else
		row_selected = FALSE;

	/* Decide which actions are allowable */
	new_color_allow = !row_selected || !defcolor_row;
	edit_pattern_allow = row_selected && !defcolor_row;
        delete_allow = row_selected && !defcolor_row && (!newwp_row || empty_wpgroup);

	/* Enable/disable the buttons accordingly */
	gtk_widget_set_sensitive( csdialog.wpattern.new_color_button_w, new_color_allow );
	gtk_widget_set_sensitive( csdialog.wpattern.edit_pattern_button_w, edit_pattern_allow );
	gtk_widget_set_sensitive( csdialog.wpattern.delete_button_w, delete_allow );
}


/* Callback for whenever a row is being dragged in the wildcard pattern
 * list area */
static boolean
csdialog_wpattern_list_drag_cb( GtkWidget *unused1, GdkDragContext *unused2, int unused3, int unused4, unsigned int unused5 )
{
	/* This gets checked in csdialog_wpattern_list_click_cb( ) */
	csdialog.wpattern.row_is_being_dragged = TRUE;
	return FALSE;
}


/* Callback for the color selection dialog popped up by the "New color"
 * button */
static void
csdialog_wpattern_new_color_selection_cb( RGBcolor *selected_color, struct WPListRowData *row_data )
{
	struct WPatternGroup *wpgroup;
	boolean place_before_existing_group = FALSE;

	/* Create new group */
	wpgroup = NEW(struct WPatternGroup);
        wpgroup->color.r = selected_color->r;
        wpgroup->color.g = selected_color->g;
	wpgroup->color.b = selected_color->b;
	wpgroup->wp_list = NULL;

	/* If a row in an existing group was selected, we add the new group
	 * immediately before the existing group */
        if (row_data != NULL)
		if (row_data->wpgroup != NULL)
			place_before_existing_group = TRUE;

#define WPGROUP_LIST csdialog.color_config.by_wpattern.wpgroup_list
	if (place_before_existing_group) {
		GList *pos = g_list_find(WPGROUP_LIST, row_data->wpgroup);
		G_LIST_INSERT_BEFORE(WPGROUP_LIST, pos, wpgroup);
	} else {
		G_LIST_APPEND(WPGROUP_LIST, wpgroup);
		/* Scroll list to bottom (to make new group visible) */
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(csdialog.wpattern.list_w));
		GtkTreeIter iter;
		gint nitems = gtk_tree_model_iter_n_children(model, NULL);
		gtk_tree_model_iter_nth_child(model, &iter, NULL, nitems - 1);
		GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(csdialog.wpattern.list_w));
		gtk_tree_selection_select_iter(select, &iter);
		/* Scroll file list to proper entry */
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(csdialog.wpattern.list_w),
					     path, NULL, FALSE, 0, 0);
		gtk_tree_path_free(path);
	}
#undef WPGROUP_LIST

	/* Update the list */
	csdialog_wpattern_list_populate();
}


/* Callback for the wildcard pattern edit subdialog */
static void
csdialog_wpattern_edit_cb( const char *input_text, struct WPListRowData *row_data )
{
	GList *l; /* for debugging */
	char *wpattern;

	/* Trim leading/trailing whitespace in input */
	wpattern = xstrstrip( xstrdup( input_text ) );

	if (strlen( wpattern ) == 0) {
		/* Ignore empty input */
		xfree( wpattern );
		return;
	}

	/* Check for duplicate pattern in group */
	/* (This doesn't prevent the possibility of duplicate patterns
	 * across groups, but hey, this is better than nothing) */
	if (g_list_find_custom( row_data->wpgroup->wp_list, wpattern, (GCompareFunc)strcmp ) != NULL) {
		/* Yep, there's a duplicate */
		xfree( wpattern );
		return;
	}

	switch (row_data->row_type) {
		case WPLIST_WPATTERN_ROW:
		/* Update existing pattern */
		l = g_list_replace( row_data->wpgroup->wp_list, row_data->wpattern, wpattern );
		g_assert( l != NULL );
		xfree( row_data->wpattern );
		break;

		case WPLIST_NEW_WPATTERN_ROW:
		/* Add new pattern */
		G_LIST_APPEND(row_data->wpgroup->wp_list, wpattern);
		break;

		SWITCH_FAIL
	}

	/* Update the list */
	csdialog_wpattern_list_populate();
}


/* Callback for the buttons to the right of the wildcard pattern list */
static void
csdialog_wpattern_button_cb( GtkWidget *button_w )
{
	struct WPListRowData *row_data = NULL;
	RGBcolor default_new_color = { 0.0, 0.0, 0.75 }; /* I like blue */
        RGBcolor *color;
        const char *title = NULL;

	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(csdialog.wpattern.list_w));
	GtkTreeIter iter;
	GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(csdialog.wpattern.list_w));
	gboolean valid = gtk_tree_selection_get_selected(select, &model, &iter);

	if (valid) {
		/* Get data for currently selected row */
		gtk_tree_model_get(model, &iter,
			DIALOG_WPATTERN_ROWDATA_COLUMN, &row_data, -1);
		g_assert( row_data != NULL );
	}

	if (button_w == csdialog.wpattern.new_color_button_w) {
                /* Bring up color selection dialog for new color group */
		title = _("New Color Group");
		color = &default_new_color;
		if (row_data && row_data->wpgroup)
			color = &row_data->wpgroup->color;
		gui_colorsel_window( title, color, csdialog_wpattern_new_color_selection_cb, row_data );
	}
	else if (button_w == csdialog.wpattern.edit_pattern_button_w) {
		/* Bring up pattern edit subdialog */
		g_assert( row_data != NULL );
		switch (row_data->row_type) {
			case WPLIST_WPATTERN_ROW:
			title = _("Edit Wildcard Pattern");
			break;

			case WPLIST_NEW_WPATTERN_ROW:
			title = _("New Wildcard Pattern");
			break;

			SWITCH_FAIL
		}
		gui_entry_window( title, row_data->wpattern, csdialog_wpattern_edit_cb, row_data );
	}
	else if (button_w == csdialog.wpattern.delete_button_w) {
		/* Delete a pattern or color group */
		g_assert( row_data != NULL );
		GtkListStore *store = GTK_LIST_STORE(model);
		switch (row_data->row_type) {
			case WPLIST_WPATTERN_ROW:
			/* Delete pattern */
			G_LIST_REMOVE(row_data->wpgroup->wp_list, row_data->wpattern);
			xfree( row_data->wpattern );
			/* Remove corresponding row */
			valid = gtk_list_store_remove(store, &iter);
			/* As a courtesy to the user, leave the next
			 * row selected */
			if (valid)
				gtk_tree_selection_select_iter(select, &iter);
			break;

			case WPLIST_NEW_WPATTERN_ROW:
			/* Delete color group ONLY if group is empty */
			if (row_data->wpgroup->wp_list != NULL)
				return;
			G_LIST_REMOVE(csdialog.color_config.by_wpattern.wpgroup_list, row_data->wpgroup);
			xfree( row_data->wpgroup );
			/* Remove corresponding rows */
			GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
			valid = gtk_list_store_remove(store, &iter);
			valid = gtk_tree_path_prev(path);
			if (valid) {
				valid = gtk_tree_model_get_iter(model, &iter, path);
				gtk_list_store_remove(store, &iter);
			}
			gtk_tree_path_free(path);
			break;

			SWITCH_FAIL
		}
	}
}


/* Callback for the "OK" button */
static void
csdialog_ok_button_cb( GtkWidget *unused, GtkWidget *window_w )
{
	ColorMode mode;

	/* Commit new color configuration, and set color mode to match
	 * current notebook page */
	mode = (ColorMode)gtk_notebook_get_current_page( GTK_NOTEBOOK(csdialog.notebook_w) );
	color_set_config( &csdialog.color_config, mode );

        /* Update option menu to reflect current color mode */
	window_set_color_mode( mode );

	gtk_widget_destroy( window_w );
}


/* Callback for dialog window destruction */
static void
csdialog_destroy_cb(GtkWidget *unused, gpointer data_unused)
{
	/* We'd leak memory like crazy if we didn't do this */
	color_config_destroy( &csdialog.color_config );
}


void
dialog_color_setup( void )
{
	GtkWidget *window_w;
	GtkWidget *main_vbox_w;
	GtkWidget *vbox_w;
	GtkWidget *vbox2_w;
	GtkWidget *hbox_w;
	GtkWidget *hbox2_w;
	GtkWidget *frame_w;
	GtkWidget *table_w;
	GtkWidget *label_w;
	GtkWidget *optmenu_w;
        RGBcolor *color;
        ColorMode color_mode;
	int i;
	char strbuf[256];

	window_w = gui_dialog_window( _("Color Setup"), NULL );
	gui_window_modalize( window_w, main_window_w );
	main_vbox_w = gui_vbox_add( window_w, 5 );
	csdialog.notebook_w = gui_notebook_add( main_vbox_w );

	/* Get current color mode/configuration */
        color_mode = color_get_mode( );
	color_get_config( &csdialog.color_config );


	/**** "By node type" page ****/

	hbox_w = gui_hbox_add( NULL, 7 );
	gui_box_set_packing( hbox_w, EXPAND, NO_FILL, AT_START );
	gui_notebook_page_add( csdialog.notebook_w, _("By node type"), hbox_w );

	vbox_w = gui_vbox_add( hbox_w, 10 );
	gtk_container_set_border_width( GTK_CONTAINER(vbox_w), 3 );
	gui_box_set_packing( vbox_w, EXPAND, NO_FILL, AT_START );
	vbox2_w = gui_vbox_add( hbox_w, 10 );
	gtk_container_set_border_width( GTK_CONTAINER(vbox2_w), 3 );
	gui_box_set_packing( vbox2_w, EXPAND, NO_FILL, AT_START );

	/* Create two-column listing of node type colors */
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		if ((i % 2) == 1)
			frame_w = gui_frame_add( vbox_w, NULL );
		else
			frame_w = gui_frame_add( vbox2_w, NULL );
		gtk_frame_set_shadow_type( GTK_FRAME(frame_w), GTK_SHADOW_ETCHED_OUT );
		hbox_w = gui_hbox_add( frame_w, 10 );

		/* Color picker button */
		sprintf( strbuf, _("Color: %s"), node_type_names[i] );
		color = &csdialog.color_config.by_nodetype.colors[i];
		gui_colorpicker_add( hbox_w, color, strbuf, csdialog_node_type_color_picker_cb, color );

		/* Node type icon */
		gui_pixbuf_xpm_add(hbox_w, node_type_xpms[i]);
		/* Node type label */
		gui_label_add( hbox_w, _(node_type_names[i]) );
	}


	/**** "By date/time" page ****/

	vbox_w = gui_vbox_add( NULL, 10 );
	gui_notebook_page_add( csdialog.notebook_w, _("By date/time"), vbox_w );

	/* Arrange the top part using a table */
	hbox_w = gui_hbox_add( vbox_w, 0 );
	table_w = gui_table_add( hbox_w, 3, 2, FALSE, 4 );
	gui_widget_packing( table_w, EXPAND, NO_FILL, AT_START );
        /* Old label */
	hbox2_w = gui_hbox_add( NULL, 0 );
	gui_table_attach( table_w, hbox2_w, 0, 1, 0, 1 );
	label_w = gui_label_add( hbox2_w, _("Oldest:") );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	/* New label */
	hbox2_w = gui_hbox_add( NULL, 0 );
	gui_table_attach( table_w, hbox2_w, 0, 1, 1, 2 );
	label_w = gui_label_add( hbox2_w, _("Newest:") );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	/* Timestamp selection label */
	hbox2_w = gui_hbox_add( NULL, 0 );
	gui_table_attach( table_w, hbox2_w, 0, 1, 2, 3 );
	label_w = gui_label_add( hbox2_w, _("Color by:") );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	/* Old date edit widget */
	csdialog.time.old_dateedit_w = gui_dateedit_add( NULL, csdialog.color_config.by_timestamp.old_time, csdialog_time_edit_cb, NULL );
        gui_table_attach( table_w, csdialog.time.old_dateedit_w, 1, 2, 0, 1 );
	/* New date edit widget */
	csdialog.time.new_dateedit_w = gui_dateedit_add( NULL, csdialog.color_config.by_timestamp.new_time, csdialog_time_edit_cb, NULL );
        gui_table_attach( table_w, csdialog.time.new_dateedit_w, 1, 2, 1, 2 );
	/* Timestamp selection option menu */
	optmenu_w = gtk_combo_box_text_new();
	time_last_access = _("Time of last access");
	time_last_modification = _("Time of last modification");
	time_last_change = _("Time of last attribute change");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(optmenu_w), time_last_access);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(optmenu_w), time_last_modification);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(optmenu_w), time_last_change);
	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenu_w), 0);
	g_signal_connect(optmenu_w, "changed", G_CALLBACK(csdialog_time_timestamp_combobox_changed), NULL);
        gui_table_attach( table_w, optmenu_w, 1, 2, 2, 3 );

	/* Time spectrum */
	frame_w = gui_frame_add( vbox_w, NULL );
	gtk_frame_set_shadow_type( GTK_FRAME(frame_w), GTK_SHADOW_IN );
	csdialog.time.spectrum_w = gui_spectrum_new( frame_w );
	gui_spectrum_fill(csdialog.time.spectrum_w, csdialog_time_spectrum_func);

	/* Horizontal box for spectrum color pickers and menu */
	hbox_w = gui_hbox_add( vbox_w, 0 );

	/* Old end */
        color = &csdialog.color_config.by_timestamp.old_color;
	csdialog.time.old_colorpicker_w = gui_colorpicker_add( hbox_w, color, _("Older Color"), csdialog_time_color_picker_cb, color );
	gui_hbox_add( hbox_w, 5 );
	gui_label_add( hbox_w, _("Older") );

	/* Spectrum type selection */
	optmenu_w = gtk_combo_box_text_new();
	spectrum_rainbow = _("Rainbow");
	spectrum_heat = _("Heat");
	spectrum_gradient = _("Gradient");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(optmenu_w), spectrum_rainbow);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(optmenu_w), spectrum_heat);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(optmenu_w), spectrum_gradient);
	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenu_w), 2);
	g_signal_connect(optmenu_w, "changed", G_CALLBACK(csdialog_time_spectrum_combobox_changed), NULL);
	gui_set_parent_child(hbox_w, optmenu_w);
	gui_widget_packing( optmenu_w, EXPAND, NO_FILL, AT_START );

	/* New end */
	gui_box_set_packing( hbox_w, NO_EXPAND, NO_FILL, AT_END );
        color = &csdialog.color_config.by_timestamp.new_color;
	csdialog.time.new_colorpicker_w = gui_colorpicker_add( hbox_w, color, _("Newer Color"), csdialog_time_color_picker_cb, color );
	gui_hbox_add( hbox_w, 5 );
	gui_label_add( hbox_w, _("Newer") );

	/* Color pickers are accessible only for gradient spectrum */
	csdialog_time_color_picker_set_access( csdialog.color_config.by_timestamp.spectrum_type == SPECTRUM_GRADIENT );


	/**** "By wildcards" page ****/

	hbox_w = gui_hbox_add( NULL, 10 );
	gui_notebook_page_add( csdialog.notebook_w, _("By wildcards"), hbox_w );

	/* List of colors and associated wildcard patterns */
	csdialog.wpattern.list_w = gui_wpattern_list_new(hbox_w);
	GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(csdialog.wpattern.list_w));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

	g_signal_connect( G_OBJECT(csdialog.wpattern.list_w), "button_release_event", G_CALLBACK(csdialog_wpattern_list_click_cb), NULL );
	g_signal_connect( G_OBJECT(select), "changed", G_CALLBACK(csdialog_wpattern_list_select_unselect_cb), NULL );
	g_signal_connect( G_OBJECT(csdialog.wpattern.list_w), "drag_motion", G_CALLBACK(csdialog_wpattern_list_drag_cb), NULL );

        /* Action buttons */
	vbox_w = gui_vbox_add( hbox_w, 0 );
	csdialog.wpattern.new_color_button_w = gui_button_add( vbox_w, _("New color"), csdialog_wpattern_button_cb, NULL );
	gui_separator_add( vbox_w );
	csdialog.wpattern.edit_pattern_button_w = gui_button_add( vbox_w, _("Edit pattern"), csdialog_wpattern_button_cb, NULL );
	gtk_widget_set_sensitive( csdialog.wpattern.edit_pattern_button_w, FALSE );
        gui_separator_add( vbox_w );
	csdialog.wpattern.delete_button_w = gui_button_add( vbox_w, _("Delete"), csdialog_wpattern_button_cb, NULL );
	gtk_widget_set_sensitive( csdialog.wpattern.delete_button_w, FALSE );

	csdialog.wpattern.row_is_being_dragged = FALSE;
	csdialog_wpattern_list_populate( );


	/* Horizontal box for OK and Cancel buttons */
	hbox_w = gui_hbox_add( main_vbox_w, 0 );
	gtk_box_set_homogeneous( GTK_BOX(hbox_w), TRUE );
	gui_box_set_packing( hbox_w, EXPAND, FILL, AT_START );

	/* OK and Cancel buttons */
	gui_button_with_pixbuf_xpm_add(hbox_w, button_ok_xpm, _("OK"), csdialog_ok_button_cb, window_w);
	gui_hbox_add( hbox_w, 0 ); /* spacer */
	gui_button_with_pixbuf_xpm_add(hbox_w, button_cancel_xpm, _("Cancel"), close_cb, window_w);

	/* Set page to current color mode */
	gtk_notebook_set_current_page( GTK_NOTEBOOK(csdialog.notebook_w), color_mode );

	/* Some cleanup will be required once the window goes away */
	g_signal_connect(G_OBJECT(window_w), "destroy", G_CALLBACK(csdialog_destroy_cb), NULL);

	gtk_widget_show( window_w );
}


/* Help -> Contents... */
void
dialog_help( void )
{
	static double t;
	GtkWidget *window_w;
	GtkWidget *frame_w;
	GtkWidget *hbox_w;
	char location[] = "file:///" DOCDIR "/fsv.html";
	char cmdbuf[2048];

	/* Browser may take a few seconds to start up... */
	gui_cursor( main_window_w, GDK_WATCH );
	gui_update( );

	/* Create message window to acknowledge action */
	window_w = gui_dialog_window( _("Help"), NULL );
	gtk_container_set_border_width( GTK_CONTAINER(window_w), 5 );
	frame_w = gui_frame_add( window_w, NULL );
	hbox_w = gui_hbox_add( frame_w, 10 );
	gui_label_add( hbox_w, _("Launching help browser . . .") );
	gtk_widget_show( window_w );
	/* and time-bomb it */
	morph_finish( &t );
	t = 0.0;
	morph_full( &t, MORPH_LINEAR, 1.0, 4.0, NULL, transient_end_cb, window_w );

	if (xfork( )) {
		/* Browser startup command */
		sprintf( cmdbuf,
		    "xdg-open %s > /dev/null 2>&1", location);

		/* Execute command */
		if (system(cmdbuf) != 0)
			g_error("Failed to launch browser cmd: %s, error message: %s\n",
				cmdbuf, g_strerror(errno));

		/* End subprocess */
		_exit( 0 );
	}
}


/* Callback for the "Look at target node" button on the "Target" page
 * of the Properties dialog for symlinks */
static void
look_at_target_node_cb( GtkWidget *unused, GNode *node )
{
	/* Target node may be buried inside a collapsed tree--
	 * if it is, expand it out into the open */
	if (NODE_IS_DIR(node->parent))
		if (!dirtree_entry_expanded( node->parent ))
			colexp( node->parent, COLEXP_EXPAND_ANY );

	camera_look_at( node );
}


/* The Properties dialog */
static void
dialog_node_properties( GNode *node )
{
	const struct NodeInfo *node_info;
	GtkWidget *window_w;
	GtkWidget *main_vbox_w;
	GtkWidget *notebook_w;
	GtkWidget *vbox_w;
	GtkWidget *table_w;
	GtkWidget *pixbuf_w;
	GtkWidget *hbox_w;
	GtkWidget *label_w;
	GtkWidget *separator_w;
	GtkWidget *button_w;
	GtkWidget *vbox2_w;
	GtkWidget *list_w;
	GtkWidget *entry_w;
	GNode *target_node;
	char strbuf[1024];
	char *proptext;

	/* Get the lowdown on the node. get_node_info( ) may cause some
	 * disk activity, so change the cursor meanwhile (just in case) */
	gui_cursor( main_window_w, GDK_WATCH );
	gui_update( );
	node_info = get_node_info( node );
	gui_cursor( main_window_w, GDK_X_CURSOR );

	window_w = gui_dialog_window( _("Properties"), NULL );
	gui_window_modalize( window_w, main_window_w );
	main_vbox_w = gui_vbox_add( window_w, 5 );
	notebook_w = gui_notebook_add( main_vbox_w );

	/**** "General" page ****/

	vbox_w = gui_vbox_add( NULL, 10 );
	gui_notebook_page_add( notebook_w, _("General"), vbox_w );
	table_w = gui_table_add( vbox_w, 6, 2, FALSE, 0 );

	/* Node type icon */
	hbox_w = gui_hbox_add( NULL, 8 );
	gui_table_attach( table_w, hbox_w, 0, 1, 0, 1 );
	pixbuf_w = gui_pixbuf_xpm_add(hbox_w, node_type_xpms[NODE_DESC(node)->type]);
	gui_widget_packing(pixbuf_w, NO_EXPAND, NO_FILL, AT_END);
	/* Name */
	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, node_info->name );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_LEFT );
	gui_table_attach( table_w, hbox_w, 1, 2, 0, 1 );

	separator_w = gui_separator_add( NULL );
	gui_table_attach( table_w, separator_w, 0, 2, 1, 2 );

	/* Labels: type, location, size, owner, group */
        strcpy( strbuf, "" );
	strcat( strbuf, _("Type:\n\n") );
	strcat( strbuf, _("Location:\n\n") );
	if (NODE_IS_DIR(node))
		strcat( strbuf, _("Total size:\n\n") );
	else {
		strcat( strbuf, _("Size:\n") );
		strcat( strbuf, _("Allocation:\n\n") );
	}
	strcat( strbuf, _("Owner:\n") );
	strcat( strbuf, _("Group:") );
	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, strbuf );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_RIGHT );
	gui_table_attach( table_w, hbox_w, 0, 1, 2, 3 );

	proptext = xstrdup( "" );
	/* Type */
	STRRECAT(proptext, _(node_type_names[NODE_DESC(node)->type]));
	STRRECAT(proptext, "\n\n");
	/* Location */
        STRRECAT(proptext, node_info->prefix);
	STRRECAT(proptext, "\n\n");
	if (NODE_IS_DIR(node)) {
		/* Total size */
		sprintf( strbuf, _("%s bytes"), node_info->subtree_size );
		STRRECAT(proptext, strbuf);
		if (DIR_NODE_DESC(node)->subtree.size >= 1024) {
			sprintf( strbuf, " (%s)", node_info->subtree_size_abbr );
			STRRECAT(proptext, strbuf);
		}
	}
	else {
		/* Size */
		sprintf( strbuf, _("%s bytes"), node_info->size );
		STRRECAT(proptext, strbuf);
		if (NODE_DESC(node)->size >= 1024) {
			sprintf( strbuf, " (%s)", node_info->size_abbr );
			STRRECAT(proptext, strbuf);
		}
		STRRECAT(proptext, "\n");
		/* Allocation */
		sprintf( strbuf, _("%s bytes"), node_info->size_alloc );
		STRRECAT(proptext, strbuf);
	}
	STRRECAT(proptext, "\n\n");
	/* Owner (user) */
	sprintf( strbuf, _("%s (uid %u)"), node_info->user_name, NODE_DESC(node)->user_id );
	STRRECAT(proptext, strbuf);
	STRRECAT(proptext, "\n");
	/* Group */
	sprintf( strbuf, _("%s (gid %u)"), node_info->group_name, NODE_DESC(node)->group_id );
	STRRECAT(proptext, strbuf);

	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, proptext );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_LEFT );
	gui_table_attach( table_w, hbox_w, 1, 2, 2, 3 );

	separator_w = gui_separator_add( NULL );
	gui_table_attach( table_w, separator_w, 0, 2, 3, 4 );

	/* Labels for date/time stamps */
	strcpy( strbuf, "" );
	strcat( strbuf, _("Modified:\n") );
	strcat( strbuf, _("AttribCh:\n") );
	strcat( strbuf, _("Accessed:") );
	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, strbuf );
	gui_widget_packing( label_w, NO_EXPAND, NO_FILL, AT_END );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_RIGHT );
	gui_table_attach( table_w, hbox_w, 0, 1, 4, 5 );

	/* Date/time stamps */
	strcpy( proptext, "" );
	/* Modified */
	STRRECAT(proptext, node_info->mtime);
	STRRECAT(proptext, "\n");
	/* Attributes changed */
	STRRECAT(proptext, node_info->ctime);
	STRRECAT(proptext, "\n");
	/* Accessed */
	STRRECAT(proptext, node_info->atime);

	hbox_w = gui_hbox_add( NULL, 8 );
	label_w = gui_label_add( hbox_w, proptext );
	gtk_label_set_justify( GTK_LABEL(label_w), GTK_JUSTIFY_LEFT );
	gui_table_attach( table_w, hbox_w, 1, 2, 4, 5 );

	separator_w = gui_separator_add( NULL );
	gui_table_attach( table_w, separator_w, 0, 2, 5, 6 );

	/* Node-type-specific information pages */

	switch (NODE_DESC(node)->type) {

		case NODE_DIRECTORY:
		/**** "Contents" page ****/

		vbox_w = gui_vbox_add( NULL, 10 );
		gui_notebook_page_add( notebook_w, _("Contents"), vbox_w );

		hbox_w = gui_hbox_add( vbox_w, 0 );
		gui_widget_packing( hbox_w, EXPAND, NO_FILL, AT_START );
		vbox2_w = gui_vbox_add( hbox_w, 10 );
		gui_widget_packing( vbox2_w, EXPAND, NO_FILL, AT_START );

		gui_label_add( vbox2_w, _("This directory contains:") );

		/* Directory contents listing */
		list_w = dir_contents_list( node );
		gtk_box_pack_start( GTK_BOX(vbox2_w), list_w, FALSE, FALSE, 0 );
		gtk_widget_show( list_w );

		gui_separator_add( vbox2_w );

		strcpy( proptext, "" );
		/* Total size readout */
		sprintf( strbuf, _("%s bytes"), node_info->subtree_size );
		STRRECAT(proptext, strbuf);
		if (DIR_NODE_DESC(node)->subtree.size >= 1024) {
			sprintf( strbuf, " (%s)", node_info->subtree_size_abbr );
			STRRECAT(proptext, strbuf);
		}
		gui_label_add( vbox2_w, proptext );
                break;


#ifdef HAVE_FILE_COMMAND
		case NODE_REGFILE:
		/**** "File type" page ****/

		vbox_w = gui_vbox_add( NULL, 10 );
		gui_notebook_page_add( notebook_w, _("File type"), vbox_w );

		gui_label_add( vbox_w, _("This file is recognized as:") );

		/* 'file' command output */
		gui_text_area_add( vbox_w, node_info->file_type_desc );
                break;
#endif /* HAVE_FILE_COMMAND */


		case NODE_SYMLINK:
		/**** "Target" page ****/

		vbox_w = gui_vbox_add( NULL, 10 );
		gui_notebook_page_add( notebook_w, _("Target"), vbox_w );

		/* (Relative) name of target */
		gui_label_add( vbox_w, _("This symlink points to:") );
		hbox_w = gui_hbox_add( vbox_w, 0 );
		entry_w = gui_entry_add( hbox_w, node_info->target, NULL, NULL );
		gtk_editable_set_editable(GTK_EDITABLE(entry_w), FALSE);

		hbox_w = gui_hbox_add( vbox_w, 0 ); /* spacer */

		/* Absolute name of target */
		gui_label_add( vbox_w, _("Absolute name of target:") );
		hbox_w = gui_hbox_add( vbox_w, 0 );
                if (!strcmp( node_info->target, node_info->abstarget ))
			entry_w = gui_entry_add( hbox_w, _("(same as above)"), NULL, NULL );
                else
			entry_w = gui_entry_add( hbox_w, node_info->abstarget, NULL, NULL );
		gtk_editable_set_editable(GTK_EDITABLE(entry_w), FALSE);

		/* This is NULL if target isn't in the filesystem tree */
		target_node = node_named( node_info->abstarget );

		/* The "Look at target node" feature does not work in TreeV
		 * mode if directories have to be expanded to see the
		 * target node, because unbuilt TreeV geometry does not
		 * have a definite location */
		if ((globals.fsv_mode == FSV_TREEV) && (target_node != NULL))
			if (NODE_IS_DIR(target_node->parent))
				if (!dirtree_entry_expanded( target_node->parent ))
					target_node = NULL;

		/* Button to point camera at target node (if present) */
		hbox_w = gui_hbox_add( vbox_w, 10 );
		button_w = gui_button_add( hbox_w, _("Look at target node"), look_at_target_node_cb, target_node );
		gui_widget_packing( button_w, EXPAND, NO_FILL, AT_START );
		gtk_widget_set_sensitive( button_w, target_node != NULL );
		g_signal_connect(G_OBJECT(button_w), "clicked", G_CALLBACK(close_cb), window_w);
		break;


		default:
		/* No additional information for this node type */
		break;
	}

	/* Close button */
	button_w = gui_button_add( main_vbox_w, _("Close"), close_cb, window_w );

	xfree( proptext );

	gtk_widget_show( window_w );
}


/**** Context-sensitive right-click menu ****/

/* (I know, it's not a dialog, but where else to put this? :-) */

/* Helper callback for context_menu( ) */
static void
collapse_cb( GtkWidget *unused, GNode *dnode )
{
	colexp( dnode, COLEXP_COLLAPSE_RECURSIVE );
}


/* ditto */
static void
expand_cb( GtkWidget *unused, GNode *dnode )
{
	colexp( dnode, COLEXP_EXPAND );
}


/* ditto */
static void
expand_recursive_cb( GtkWidget *unused, GNode *dnode )
{
	colexp( dnode, COLEXP_EXPAND_RECURSIVE );
}


/* ditto */
static void
look_at_cb( GtkWidget *unused, GNode *node )
{
	camera_look_at( node );
}


/* ditto */
static void
properties_cb( GtkWidget *unused, GNode *node )
{
	dialog_node_properties( node );
}


void
context_menu( GNode *node)
{
	static GtkWidget *popup_menu_w = NULL;

	/* Recycle previous popup menu */
	if (popup_menu_w != NULL) {
		g_assert( GTK_IS_MENU(popup_menu_w) );
		gtk_widget_destroy( popup_menu_w );
	}

	/* Check for the special case in which the menu has only one item */
	if (!NODE_IS_DIR(node) && (node == globals.current_node)) {
		dialog_node_properties( node );
		return;
	}

	/* Create menu */
	popup_menu_w = gtk_menu_new( );
	if (NODE_IS_DIR(node)) {
		if (dirtree_entry_expanded( node ))
			gui_menu_item_add( popup_menu_w, _("Collapse"), collapse_cb, node );
		else {
			gui_menu_item_add( popup_menu_w, _("Expand"), expand_cb, node );
			if (DIR_NODE_DESC(node)->subtree.counts[NODE_DIRECTORY] > 0)
				gui_menu_item_add( popup_menu_w, _("Expand all"), expand_recursive_cb, node );
		}
	}
	if (node != globals.current_node)
		gui_menu_item_add( popup_menu_w, _("Look at"), look_at_cb, node );
	gui_menu_item_add( popup_menu_w, _("Properties"), properties_cb, node );

	gtk_menu_popup_at_pointer(GTK_MENU(popup_menu_w), NULL);
}


/* end dialog.c */
