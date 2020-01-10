/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2006 James Livingston  <doclivingston@gmail.com>
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

#define __EXTENSIONS__

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "mediaplayerid.h"

#include "eel-gconf-extensions.h"
#include "rb-nokia770-source.h"
#include "rb-debug.h"
#include "rb-util.h"
#include "rb-file-helpers.h"
#include "rhythmdb.h"
#include "rb-plugin.h"


static char * impl_uri_from_playlist_uri (RBGenericPlayerSource *source, const char *uri);


typedef struct {
#ifdef __SUNPRO_C
   int x;  /* To build with Solaris forte compiler */
#endif
} RBNokia770SourcePrivate;

RB_PLUGIN_DEFINE_TYPE (RBNokia770Source, rb_nokia770_source, RB_TYPE_GENERIC_PLAYER_SOURCE)
#define NOKIA770_SOURCE_GET_PRIVATE(o)   (G_TYPE_INSTANCE_GET_PRIVATE ((o), RB_TYPE_NOKIA770_SOURCE, RBNokia770SourcePrivate))


#define NOKIA_INTERNAL_MOUNTPOINT "file:///media/mmc1/"

static void
rb_nokia770_source_class_init (RBNokia770SourceClass *klass)
{
	RBGenericPlayerSourceClass *generic_class = RB_GENERIC_PLAYER_SOURCE_CLASS (klass);

	generic_class->impl_uri_from_playlist_uri = impl_uri_from_playlist_uri;

	g_type_class_add_private (klass, sizeof (RBNokia770SourcePrivate));
}

static void
rb_nokia770_source_init (RBNokia770Source *source)
{

}

RBRemovableMediaSource *
rb_nokia770_source_new (RBPlugin *plugin, RBShell *shell, GMount *mount, MPIDDevice *device_info)
{
	RBNokia770Source *source;
	RhythmDBEntryType entry_type;
	RhythmDB *db;
	GVolume *volume;
	char *name;
	char *path;

	g_assert (rb_nokia770_is_mount_player (mount, device_info));

	volume = g_mount_get_volume (mount);

	g_object_get (G_OBJECT (shell), "db", &db, NULL);
	path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	name = g_strdup_printf ("nokia770: %s", path);
	entry_type = rhythmdb_entry_register_type (db, name);
	g_object_unref (db);
	g_free (name);
	g_free (path);
	g_object_unref (volume);

	source = RB_NOKIA770_SOURCE (g_object_new (RB_TYPE_NOKIA770_SOURCE,
						   "plugin", plugin,
						   "entry-type", entry_type,
						   "ignore-entry-type", RHYTHMDB_ENTRY_TYPE_INVALID,
						   "error-entry-type", RHYTHMDB_ENTRY_TYPE_INVALID,
						   "mount", mount,
						   "shell", shell,
						   "source-group", RB_SOURCE_GROUP_DEVICES,
						   "device-info", device_info,
						   NULL));

	rb_shell_register_entry_type_for_source (shell, RB_SOURCE (source), entry_type);

	return RB_REMOVABLE_MEDIA_SOURCE (source);
}

static char *
impl_uri_from_playlist_uri (RBGenericPlayerSource *source, const char *uri)
{
	const char *path;
	char *local_uri;
	char *mount_uri;

	if (!g_str_has_prefix (uri, NOKIA_INTERNAL_MOUNTPOINT)) {
		rb_debug ("found playlist uri with unexpected mountpoint");
		return NULL;
	}

	path = uri + strlen (NOKIA_INTERNAL_MOUNTPOINT);
	mount_uri = rb_generic_player_source_get_mount_path (source);
	local_uri = rb_uri_append_uri (mount_uri, path);
	g_free (mount_uri);
	return local_uri;
}

gboolean
rb_nokia770_is_mount_player (GMount *mount, MPIDDevice *device_info)
{
	gboolean result;
	char *vendor;
	char *model;

	g_object_get (device_info, "vendor", &vendor, "model", &model, NULL);
	result = FALSE;
	if (vendor != NULL && g_str_equal (vendor, "Nokia")) {
		if (model != NULL && (g_str_equal (model, "770") || g_str_equal (model, "N800") || g_str_equal (model, "N810"))) {
			result = TRUE;
		}
	}

	g_free (vendor);
	g_free (model);
	return result;
}

