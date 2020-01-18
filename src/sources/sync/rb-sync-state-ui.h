/*
 *  Copyright (C) 2010 Jonathan Matthew  <jonathan@d14n.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  The Rhythmbox authors hereby grant permission for non-GPL compatible
 *  GStreamer plugins to be used and distributed together with GStreamer
 *  and Rhythmbox. This permission is above and beyond the permissions granted
 *  by the GPL license by which Rhythmbox is covered. If you modify this code
 *  you may extend this exception to your version of the code, but you are not
 *  obligated to do so. If you do not wish to do so, delete this exception
 *  statement from your version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */

#ifndef __RB_SYNC_STATE_UI_H
#define __RB_SYNC_STATE_UI_H

#include <gtk/gtk.h>

#include "rb-sync-state.h"

G_BEGIN_DECLS

/* Segmented bar for device usage and sync before/after views */

typedef struct _RBSyncBarData RBSyncBarData;

struct _RBSyncBarData
{
	GtkWidget *widget;
	guint music_segment;
	guint podcast_segment;
	guint other_segment;
	guint free_segment;
	guint64 capacity;
};

void	rb_sync_state_ui_create_bar (RBSyncBarData *bar, guint64 capacity, GtkWidget *label);

void	rb_sync_state_ui_update_volume_usage (RBSyncBarData *bar, RBSyncState *state);	/* hm */

/* Sync state UI container */

#define RB_TYPE_SYNC_STATE_UI         (rb_sync_state_ui_get_type ())
#define RB_SYNC_STATE_UI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), RB_TYPE_SYNC_STATE_UI, RBSyncStateUI))
#define RB_SYNC_STATE_UI_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), RB_TYPE_SYNC_STATE_UI, RBSyncStateUIClass))
#define RB_IS_SYNC_STATE_UI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), RB_TYPE_SYNC_STATE_UI))
#define RB_IS_SYNC_STATE_UI_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), RB_TYPE_SYNC_STATE_UI))
#define RB_SYNC_STATE_UI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), RB_TYPE_SYNC_STATE_UI, RBSyncStateUIClass))

typedef struct _RBSyncStateUI RBSyncStateUI;
typedef struct _RBSyncStateUIClass RBSyncStateUIClass;
typedef struct _RBSyncStateUIPrivate RBSyncStateUIPrivate;

struct _RBSyncStateUI {
	GtkVBox parent;

	RBSyncStateUIPrivate *priv;
};

struct _RBSyncStateUIClass {
	GtkVBoxClass parent_class;
};

GType		rb_sync_state_ui_get_type (void);

GtkWidget *	rb_sync_state_ui_new (RBSyncState *state);

G_END_DECLS

#endif /* __RB_SYNC_STATE_UI_H */
