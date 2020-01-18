/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003,2004 Colin Walters <walters@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
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

#include <config.h>

#include <locale.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
/* X11 headers */
#include <X11/Xlib.h>
#endif

#include <girepository.h>

#include "rb-shell.h"
#include "rb-util.h"
#include "eggdesktopfile.h"
#include "rb-debug.h"
#include "rb-application.h"

int
main (int argc, char **argv)
{
	GApplication *app;
	char *desktop_file_path;
	int rc;

#ifdef GDK_WINDOWING_X11
	if (XInitThreads () == 0) {
		g_critical ("Initialising threading support failed.");
		return 1;
	}
#endif

	g_type_init ();
	g_random_set_seed (time (0));

#ifdef USE_UNINSTALLED_DIRS
	desktop_file_path = g_build_filename (SHARE_UNINSTALLED_BUILDDIR, "rhythmbox.desktop", NULL);

	g_setenv ("GSETTINGS_SCHEMA_DIR", SHARE_UNINSTALLED_BUILDDIR, TRUE);
#else
	desktop_file_path = g_build_filename (DATADIR, "applications", "rhythmbox.desktop", NULL);
#endif
	egg_set_desktop_file (desktop_file_path);
	g_free (desktop_file_path);

	setlocale (LC_ALL, NULL);

#ifdef ENABLE_NLS
	/* initialize i18n */
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	textdomain (GETTEXT_PACKAGE);
#endif

#if defined(USE_UNINSTALLED_DIRS)
	g_irepository_prepend_search_path (SHARE_UNINSTALLED_BUILDDIR "/../bindings/gi");
#endif

	/* TODO: kill this function */
	rb_threads_init ();

	app = rb_application_new ();
	rc = rb_application_run (RB_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return rc;
}
