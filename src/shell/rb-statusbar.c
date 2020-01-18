/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2003 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright (C) 2003,2004 Colin Walters <walters@redhat.com>
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

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "rb-statusbar.h"
#include "rb-track-transfer-queue.h"
#include "rb-debug.h"

/**
 * SECTION:rb-statusbar
 * @short_description: status bar widget
 *
 * The status bar is displayed at the bottom of the main window.  It consists of some
 * status text and a progress bar.
 *
 * The status text usually comes from the selected page, and typically shows the number
 * of songs, the total duration and the total file size.  When a menu is open, however, 
 * the status text shows the description of the currently selected menu item.
 *
 * The progress bar shows progress information from a variety of sources.  The page that
 * is currently selected in the display page tree can provide progress information, such as
 * buffering feedback, track transfer status, or progress for updating a song catalog.
 * If the page does not provide status information and the database is busy (loading the
 * database from disk, processing a query, etc.) the progress bar will be pulsed periodically.
 */

#define EPSILON		(0.00001)

static void rb_statusbar_class_init (RBStatusbarClass *klass);
static void rb_statusbar_init (RBStatusbar *statusbar);
static void rb_statusbar_dispose (GObject *object);
static void rb_statusbar_finalize (GObject *object);
static void rb_statusbar_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec);
static void rb_statusbar_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec);

static gboolean poll_status (RBStatusbar *status);
static void rb_statusbar_sync_status (RBStatusbar *status);
static void rb_statusbar_page_status_changed_cb (RBDisplayPage *page,
						   RBStatusbar *statusbar);
static void rb_statusbar_transfer_progress_cb (RBTrackTransferQueue *queue,
					       int done,
					       int total,
					       double fraction,
					       int time_left,
					       RBStatusbar *statusbar);

struct RBStatusbarPrivate
{
        RBDisplayPage *selected_page;
	RBTrackTransferQueue *transfer_queue;

        RhythmDB *db;

        GtkWidget *progress;

        guint status_poll_id;
};

enum
{
        PROP_0,
        PROP_DB,
        PROP_PAGE,
	PROP_TRANSFER_QUEUE
};

G_DEFINE_TYPE (RBStatusbar, rb_statusbar, GTK_TYPE_STATUSBAR)

static void
rb_statusbar_class_init (RBStatusbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = rb_statusbar_dispose;
        object_class->finalize = rb_statusbar_finalize;

        object_class->set_property = rb_statusbar_set_property;
        object_class->get_property = rb_statusbar_get_property;

	/**
	 * RBStatusbar:db:
	 *
	 * The #RhythmDB instance
	 */
        g_object_class_install_property (object_class,
                                         PROP_DB,
                                         g_param_spec_object ("db",
                                                              "RhythmDB",
                                                              "RhythmDB object",
                                                              RHYTHMDB_TYPE,
                                                              G_PARAM_READWRITE));
	/**
	 * RBStatusbar:page:
	 *
	 * The currently selected #RBDisplayPage
	 */
        g_object_class_install_property (object_class,
                                         PROP_PAGE,
                                         g_param_spec_object ("page",
                                                              "RBDisplayPage",
                                                              "RBDisplayPage object",
                                                              RB_TYPE_DISPLAY_PAGE,
                                                              G_PARAM_READWRITE));
	/**
	 * RBStatusbar::transfer-queue:
	 *
	 * The #RBTrackTransferQueue instance
	 */
	g_object_class_install_property (object_class,
					 PROP_TRANSFER_QUEUE,
					 g_param_spec_object ("transfer-queue",
							      "RBTrackTransferQueue",
							      "RBTrackTransferQueue instance",
							      RB_TYPE_TRACK_TRANSFER_QUEUE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (klass, sizeof (RBStatusbarPrivate));
}

static void
rb_statusbar_init (RBStatusbar *statusbar)
{
	statusbar->priv = G_TYPE_INSTANCE_GET_PRIVATE (statusbar,
						       RB_TYPE_STATUSBAR,
						       RBStatusbarPrivate);

        statusbar->priv->progress = gtk_progress_bar_new ();
	gtk_widget_set_size_request (statusbar->priv->progress, -1, 10);

        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (statusbar->priv->progress), 1.0);
        gtk_widget_hide (statusbar->priv->progress);

        gtk_box_pack_start (GTK_BOX (statusbar),
                            GTK_WIDGET (statusbar->priv->progress), FALSE, TRUE, 0);
}

static void
rb_statusbar_dispose (GObject *object)
{
        RBStatusbar *statusbar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (RB_IS_STATUSBAR (object));

        statusbar = RB_STATUSBAR (object);

        g_return_if_fail (statusbar->priv != NULL);

	if (statusbar->priv->status_poll_id) {
                g_source_remove (statusbar->priv->status_poll_id);
		statusbar->priv->status_poll_id = 0;
	}

	if (statusbar->priv->db != NULL) {
		g_object_unref (statusbar->priv->db);
		statusbar->priv->db = NULL;
	}

	if (statusbar->priv->selected_page != NULL) {
		g_object_unref (statusbar->priv->selected_page);
		statusbar->priv->selected_page = NULL;
	}

	if (statusbar->priv->transfer_queue != NULL) {
		g_object_unref (statusbar->priv->transfer_queue);
		statusbar->priv->transfer_queue = NULL;
	}

        G_OBJECT_CLASS (rb_statusbar_parent_class)->dispose (object);
}

static void
rb_statusbar_finalize (GObject *object)
{
        RBStatusbar *statusbar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (RB_IS_STATUSBAR (object));

        statusbar = RB_STATUSBAR (object);

        g_return_if_fail (statusbar->priv != NULL);

        G_OBJECT_CLASS (rb_statusbar_parent_class)->finalize (object);
}

static void
rb_statusbar_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
        RBStatusbar *statusbar = RB_STATUSBAR (object);

        switch (prop_id)
        {
        case PROP_DB:
                statusbar->priv->db = g_value_get_object (value);
		g_object_ref (statusbar->priv->db);
                statusbar->priv->status_poll_id
                        = g_idle_add ((GSourceFunc) poll_status, statusbar);
                break;
        case PROP_PAGE:
                if (statusbar->priv->selected_page != NULL) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (statusbar->priv->selected_page),
							      G_CALLBACK (rb_statusbar_page_status_changed_cb),
							      statusbar);
			g_object_unref (statusbar->priv->selected_page);
                }

                statusbar->priv->selected_page = g_value_dup_object (value);
                rb_debug ("selected page %p", statusbar->priv->selected_page);

                if (statusbar->priv->selected_page != NULL) {
			g_signal_connect_object (G_OBJECT (statusbar->priv->selected_page),
						 "status-changed",
						 G_CALLBACK (rb_statusbar_page_status_changed_cb),
						 statusbar, 0);
                }
		rb_statusbar_sync_status (statusbar);

                break;
	case PROP_TRANSFER_QUEUE:
		statusbar->priv->transfer_queue = g_value_dup_object (value);
		g_signal_connect_object (G_OBJECT (statusbar->priv->transfer_queue),
					 "transfer-progress",
					 G_CALLBACK (rb_statusbar_transfer_progress_cb),
					 statusbar,
					 0);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
rb_statusbar_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
        RBStatusbar *statusbar = RB_STATUSBAR (object);

        switch (prop_id)
        {
        case PROP_DB:
                g_value_set_object (value, statusbar->priv->db);
                break;
        case PROP_PAGE:
                g_value_set_object (value, statusbar->priv->selected_page);
                break;
        case PROP_TRANSFER_QUEUE:
                g_value_set_object (value, statusbar->priv->transfer_queue);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

/**
 * rb_statusbar_set_page:
 * @statusbar: the #RBStatusbar
 * @page: the new selected #RBDisplayPage
 *
 * Updates the status bar for a newly selected page.
 */
void
rb_statusbar_set_page (RBStatusbar *statusbar, RBDisplayPage *page)
{
        g_return_if_fail (RB_IS_STATUSBAR (statusbar));
        g_return_if_fail (RB_IS_DISPLAY_PAGE (page));

        g_object_set (statusbar, "page", page, NULL);
}

static gboolean
poll_status (RBStatusbar *status)
{
        GDK_THREADS_ENTER ();

        status->priv->status_poll_id = 0;
        rb_statusbar_sync_status (status);

        GDK_THREADS_LEAVE ();

        return FALSE;
}

static void
rb_statusbar_sync_status (RBStatusbar *status)
{
        gboolean changed = FALSE;
        char *status_text = NULL;
	char *progress_text = NULL;
	float progress = 999;
	int time_left = 0;

        /*
         * Behaviour of status bar:
	 * - use page's status text
	 * - use page's progress value and text, unless transfer queue provides something
	 * - if no page progress value or transfer progress value and library is busy,
	 *    pulse the progress bar
         */
        
	/* library busy? */
        if (rhythmdb_is_busy (status->priv->db)) {
		progress = -1.0f;

		/* see if it wants to provide more details */
		rhythmdb_get_progress_info (status->priv->db, &progress_text, &progress);
		changed = TRUE;
        }

	/* get page details */
        if (status->priv->selected_page) {
		rb_display_page_get_status (status->priv->selected_page, &status_text, &progress_text, &progress);
		rb_debug ("updating status with: '%s', '%s', %f",
			status_text ? status_text : "", progress_text ? progress_text : "", progress);
	}

	/* get transfer details */
	rb_track_transfer_queue_get_status (status->priv->transfer_queue,
					    &status_text,
					    &progress_text,
					    &progress,
					    &time_left);

        /* set up the status text */
        if (status_text) {
                gtk_statusbar_pop (GTK_STATUSBAR (status), 0);
                gtk_statusbar_push (GTK_STATUSBAR (status), 0, status_text);
		g_free (status_text);
        }

        /* set up the progress bar */
	if (progress > (1.0f - EPSILON)) {
		gtk_widget_hide (status->priv->progress);
	} else {
                gtk_widget_show (status->priv->progress);

                if (progress < EPSILON) {
			gtk_progress_bar_pulse (GTK_PROGRESS_BAR (status->priv->progress));
                        changed = TRUE;
                } else {
                        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (status->priv->progress),
                                                       progress);
                }
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (status->priv->progress),
					   progress_text);
	}

	g_free (progress_text);

        if (status->priv->status_poll_id == 0 && changed)
                status->priv->status_poll_id = g_timeout_add (250, (GSourceFunc) poll_status, status);
}

/**
 * rb_statusbar_new:
 * @db: the #RhythmDB instance
 * @transfer_queue: the #RBTrackTransferQueue
 *
 * Creates the status bar widget.
 *
 * Return value: the status bar widget
 */
RBStatusbar *
rb_statusbar_new (RhythmDB *db,
		  RBTrackTransferQueue *queue)
{
        RBStatusbar *statusbar = g_object_new (RB_TYPE_STATUSBAR,
                                               "db", db,
					       "transfer-queue", queue,
                                               NULL);

        g_return_val_if_fail (statusbar->priv != NULL, NULL);

        return statusbar;
}

static void
add_status_poll (RBStatusbar *statusbar)
{
        if (statusbar->priv->status_poll_id == 0)
                statusbar->priv->status_poll_id =
                        g_idle_add ((GSourceFunc) poll_status, statusbar);
}

static void
rb_statusbar_page_status_changed_cb (RBDisplayPage *page, RBStatusbar *statusbar)
{
	rb_debug ("source status changed");
	add_status_poll (statusbar);
}

static void
rb_statusbar_transfer_progress_cb (RBTrackTransferQueue *queue,
				   int done,
				   int total,
				   double progress,
				   int time_left,
				   RBStatusbar *statusbar)
{
	rb_debug ("transfer progress changed");
	add_status_poll (statusbar);
}
