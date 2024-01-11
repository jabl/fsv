/* dirtree.c */

/* Directory tree control */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "dirtree.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "colexp.h"
#include "dialog.h"
#include "filelist.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"

/* Mini collapsed/expanded directory icon XPM's */
#define mini_folder_xpm mini_folder_closed_xpm
#include "xmaps/mini-folder.xpm"
#include "xmaps/mini-folder-open.xpm"


/* Time for the directory tree to scroll to a given entry (in seconds) */
#define DIRTREE_SCROLL_TIME 0.5


/* The directory tree widget */
static GtkWidget *dir_tree_w;

/* Mini collapsed/expanded directory icons */
static Icon dir_colexp_mini_icons[2];

/* Current directory */
static GNode *dirtree_current_dnode;


/* Callback for button press in the directory tree area */
static void
dirtree_select_cb(GtkTreeSelection *selection, gpointer data)
{
	GNode *dnode = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;
	//int row;

	/* If About presentation is up, end it */
	about( ABOUT_END );

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, DIRTREE_NODE_COLUMN, &dnode, -1);
		if (!dnode)
			return;
		if (dirtree_entry_expanded(dnode)) {
			camera_look_at( dnode );
			g_signal_stop_emission_by_name(G_OBJECT(selection), "changed" );
			return;
		}
		geometry_highlight_node(dnode, FALSE);
		window_statusbar(SB_RIGHT, node_absname(dnode));
		if (dnode != dirtree_current_dnode) {
			filelist_populate( dnode );
			dirtree_current_dnode = dnode;
		}
	}
#if 0
	gtk_clist_get_selection_info( GTK_CLIST(ctree_w), ev_button->x, ev_button->y, &row, NULL );
	if (row < 0)
		return FALSE;

	dnode = (GNode *)gtk_clist_get_row_data( GTK_CLIST(ctree_w), row );
	if (dnode == NULL)
		return FALSE;

	/* A single-click from button 1 highlights the node, shows the
	 * name, and updates the file list if necessary. (and also selects
	 * the row, but GTK+ does that automatically for us) */
	if ((ev_button->button == 1) && (ev_button->type == GDK_BUTTON_PRESS)) {
		geometry_highlight_node( dnode, FALSE );
		window_statusbar( SB_RIGHT, node_absname( dnode ) );
		if (dnode != dirtree_current_dnode)
			filelist_populate( dnode );
		dirtree_current_dnode = dnode;
		return FALSE;
	}

	/* A double-click from button 1 gets the camera moving */
	if ((ev_button->button == 1) && (ev_button->type == GDK_2BUTTON_PRESS)) {
		camera_look_at( dnode );
		/* Preempt the forthcoming tree expand/collapse
		 * (the standard action spawned by a double-click) */
		gtk_signal_emit_stop_by_name( GTK_OBJECT(ctree_w), "button_press_event" );
		return TRUE;
	}

	/* A click from button 3 selects the row, highlights the node,
	 * shows the name, updates the file list if necessary, and brings
	 * up a context-sensitive menu */
	if (ev_button->button == 3) {
		gtk_clist_select_row( GTK_CLIST(ctree_w), row, 0 );
		geometry_highlight_node( dnode, FALSE );
		window_statusbar( SB_RIGHT, node_absname( dnode ) );
		if (dnode != dirtree_current_dnode)
			filelist_populate( dnode );
		dirtree_current_dnode = dnode;
		context_menu( dnode );
		return FALSE;
	}

	return FALSE;
#endif
}


/* Callback for collapse of a directory tree entry */
static void
dirtree_collapse_cb(GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, gpointer data)
{
	GNode *dnode;

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	//GtkTreeIter iter;

	//gtk_tree_model_get_iter(model, &iter, tnode);
	gtk_tree_model_get(model, iter, DIRTREE_NODE_COLUMN, &dnode, -1);
	colexp( dnode, COLEXP_COLLAPSE_RECURSIVE );
}


/* Callback for expand of a directory tree entry */
static void
dirtree_expand_cb(GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, gpointer data)
{
	GNode *dnode;

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	//GtkTreeIter iter;
	//gtk_tree_model_get_iter(model, &iter, tnode);
	gtk_tree_model_get(model, iter, DIRTREE_NODE_COLUMN, &dnode, -1);
	colexp( dnode, COLEXP_EXPAND );
}


/* Loads the mini collapsed/expanded directory icons (from XPM data) */
static void
dirtree_icons_init( void )
{
	static const char **dir_colexp_mini_xpms[] = {
		mini_folder_closed_xpm,
		mini_folder_open_xpm
	};
	int i;

	gtk_widget_realize( dir_tree_w );

	/* Make icons for collapsed and expanded directories */
	for (i = 0; i < 2; i++) {
		GdkPixbuf *pb = gdk_pixbuf_new_from_xpm_data(dir_colexp_mini_xpms[i]);
		dir_colexp_mini_icons[i].pixbuf = pb;
	}
}


/* Correspondence from window_init( ) */
void
dirtree_pass_widget( GtkWidget *tree_w )
{
	dir_tree_w = tree_w;
	GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_w));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	/* Connect signal handlers */
	g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(dirtree_select_cb), NULL );
	g_signal_connect( G_OBJECT(dir_tree_w), "row_collapsed", G_CALLBACK(dirtree_collapse_cb), NULL );
	g_signal_connect( G_OBJECT(dir_tree_w), "row_expanded", G_CALLBACK(dirtree_expand_cb), NULL );

	dirtree_icons_init( );
}


/* Clears out all entries from the directory tree */
void
dirtree_clear( void )
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(dir_tree_w));
	GtkTreeStore *store = GTK_TREE_STORE(model);
	gtk_tree_store_clear(store);
	dirtree_current_dnode = NULL;
}


/* Adds a new entry to the directory tree */
void
dirtree_entry_new( GNode *dnode )
{
	GtkTreePath *parent_tnode = NULL;
	const char *name;
	boolean expanded;

	g_assert( NODE_IS_DIR(dnode) );

	parent_tnode = DIR_NODE_DESC(dnode->parent)->tnode;
	if (strlen( NODE_DESC(dnode)->name ) > 0)
		name = NODE_DESC(dnode)->name;
	else
		name = _("/. (root)");
	expanded = g_node_depth( dnode ) <= 2;

	DIR_NODE_DESC(dnode)->tnode = gui_tree_node_add( dir_tree_w, parent_tnode, dir_colexp_mini_icons, name, expanded, dnode );
}


/* Call this after the last call to dirtree_entry_new( ) */
void
dirtree_no_more_entries( void )
{
	// TODO: Needs to keep reference to model in dirtree_entry_new
	// so dissociating works.
	// GtkTreeView *view = GTK_TREE_VIEW(dir_tree_w);
	// GtkTreeModel *model = gtk_tree_view_get_model(view);
	// gtk_tree_view_set_model(view, model); /* Re-attach model to view */
	// g_object_unref(model);
}


/* This updates the directory tree to show (and select) a particular
 * directory entry, repopulating the file list with the contents of the
 * directory if not already listed */
void
dirtree_entry_show( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

	/* Repopulate file list if directory is different */
	if (dnode != dirtree_current_dnode) {
		filelist_populate( dnode );
/* TODO: try removing this update from here */
		gui_update( );
	}

	/* Scroll directory tree to proper entry */
	GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(dir_tree_w));
	gtk_tree_selection_select_path(select, DIR_NODE_DESC(dnode)->tnode);

	dirtree_current_dnode = dnode;
}


/* Returns TRUE if the entry for the given directory is expanded */
boolean
dirtree_entry_expanded( GNode *dnode )
{
	if (!dnode)
		return FALSE;

	g_assert( NODE_IS_DIR(dnode) );

	return gtk_tree_view_row_expanded(GTK_TREE_VIEW(dir_tree_w), DIR_NODE_DESC(dnode)->tnode);
}


/* Helper function */
static void
block_colexp_handlers( void )
{
	g_signal_handlers_block_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_collapse_cb), NULL );
	g_signal_handlers_block_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_expand_cb), NULL );
}


/* Helper function */
static void
unblock_colexp_handlers( void )
{
	g_signal_handlers_unblock_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_collapse_cb), NULL );
	g_signal_handlers_unblock_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_expand_cb), NULL );
}


/* Recursively collapses the directory tree entry of the given directory */
void
dirtree_entry_collapse_recursive( GNode *dnode )
{
	if (!dnode)
		return;

	g_assert( NODE_IS_DIR(dnode) );

	block_colexp_handlers( );
	gtk_tree_view_collapse_row(GTK_TREE_VIEW(dir_tree_w), DIR_NODE_DESC(dnode)->tnode);
	unblock_colexp_handlers( );
}


/* Expands the directory tree entry of the given directory. If any of its
 * ancestor directory entries are not expanded, then they are expanded
 * as well */
void
dirtree_entry_expand( GNode *dnode )
{
	if (!dnode)
		return;

	g_assert( NODE_IS_DIR(dnode) );

	block_colexp_handlers( );
	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(dir_tree_w), DIR_NODE_DESC(dnode)->tnode);
	unblock_colexp_handlers( );
}


/* Recursively expands the entire directory tree subtree of the given
 * directory */
void
dirtree_entry_expand_recursive( GNode *dnode )
{
	if (!dnode)
		return;

	g_assert( NODE_IS_DIR(dnode) );

#if DEBUG
	/* Guard against expansions inside collapsed subtrees */
	/** NOTE: This function may be upgraded to behave similarly to
	 ** dirtree_entry_expand( ) w.r.t. collapsed parent directories.
	 ** This has been avoided thus far since such a behavior would
	 ** not be used by the program. */
	if (NODE_IS_DIR(dnode->parent))
		g_assert( dirtree_entry_expanded( dnode->parent ) );
#endif

	block_colexp_handlers( );
	gtk_tree_view_expand_row(GTK_TREE_VIEW(dir_tree_w), DIR_NODE_DESC(dnode)->tnode, TRUE);
	unblock_colexp_handlers( );
}


/* end dirtree.c */
