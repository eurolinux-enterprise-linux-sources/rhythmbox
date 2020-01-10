/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2006  James Livingston  <doclivingston@gmail.com>
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

#include <config.h>

#include "rb-encoder.h"
#include "rb-encoder-gst.h"
#include "rb-marshal.h"

/**
 * SECTION:rb-encoder
 * @short_description: audio transcoder interface
 *
 * The RBEncoder interface provides transcoding between audio formats based on
 * MIME types.
 *
 * The encoder picks the output format from a list provided by the caller,
 * limited by the available codecs.  It operatees asynchronously and provides
 * status updates in the form of signals.
 *
 * A new encoder instance should be created for each file that is transcoded.
 */

static void rb_encoder_factory_class_init (RBEncoderFactoryClass *klass);
static void rb_encoder_factory_init       (RBEncoderFactory *encoder);

/* Signals */
enum {
	PROGRESS,
	COMPLETED,
	ERROR,
	PREPARE_SOURCE,		/* this is on RBEncoderFactory */
	PREPARE_SINK,		/* this is on RBEncoderFactory */
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static RBEncoderFactory *the_encoder_factory = NULL;
static gsize encoder_factory_init = 0;

G_DEFINE_TYPE(RBEncoderFactory, rb_encoder_factory, G_TYPE_OBJECT)

static void
rb_encoder_factory_init (RBEncoderFactory *factory)
{
}

static void
rb_encoder_factory_class_init (RBEncoderFactoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * RBEncoderFactory::prepare-source:
	 * @factory: the #RBEncoderFactory instance
	 * @uri: the URI for the source
	 * @source: the source object (a GstElement in fact)
	 *
	 * Emitted when creating a source to read the specified URI.
	 * Plugins can use this when just creating a GStreamer element from the URI
	 * isn't enough.  Typically this happens when there's no way to pass device
	 * information through the URI format.
	 */
	signals[PREPARE_SOURCE] =
		g_signal_new ("prepare-source",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (RBEncoderFactoryClass, prepare_source),
			      NULL, NULL,
			      rb_marshal_VOID__STRING_OBJECT,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_OBJECT);
	/**
	 * RBEncoderFactory::prepare-sink:
	 * @factory: the #RBEncoderFactory instance
	 * @uri: the URI for the sink
	 * @sink: the sink object (a GstElement in fact)
	 *
	 * Emitted when creating a sink to write to the specified URI.
	 * Plugins can use this when just creating a GStreamer element from the URI
	 * isn't enough.  Typically this happens when there's no way to pass device
	 * information through the URI format.
	 */
	signals[PREPARE_SINK] =
		g_signal_new ("prepare-sink",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (RBEncoderFactoryClass, prepare_sink),
			      NULL, NULL,
			      rb_marshal_VOID__STRING_OBJECT,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_OBJECT);
}

static void
rb_encoder_interface_init (RBEncoderIface *iface)
{
	/**
	 * RBEncoder::progress:
	 * @encoder: the #RBEncoder instance
	 * @fraction: progress as a fraction (0..1)
	 *
	 * Emitted regularly during the encoding process to provide progress updates.
	 */
	signals[PROGRESS] =
		g_signal_new ("progress",
			      G_TYPE_FROM_INTERFACE (iface),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (RBEncoderIface, progress),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__DOUBLE,
			      G_TYPE_NONE,
			      1, G_TYPE_DOUBLE);
	/**
	 * RBEncoder::completed:
	 * @encoder: the #RBEncoder instance
	 * 
	 * Emitted when the encoding process is complete.  The destination file
	 * will be closed and flushed to disk when this occurs.
	 */
	signals[COMPLETED] =
		g_signal_new ("completed",
			      G_TYPE_FROM_INTERFACE (iface),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (RBEncoderIface, completed),
			      NULL, NULL,
			      rb_marshal_VOID__UINT64,
			      G_TYPE_NONE,
			      1, G_TYPE_UINT64);
	/**
	 * RBEncoder::error:
	 * @encoder: the #RBEncoder instance
	 * @error: a #GError describing the error
	 *
	 * Emitted when an error occurs during encoding.
	 */
	signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_INTERFACE (iface),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (RBEncoderIface, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

}

GType
rb_encoder_get_type (void)
{
	static GType our_type = 0;

	if (!our_type) {
		static const GTypeInfo our_info = {
			sizeof (RBEncoderIface),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc)rb_encoder_interface_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			0,
			0,
			NULL
		};

		our_type = g_type_register_static (G_TYPE_INTERFACE, "RBEncoder", &our_info, 0);
	}

	return our_type;
}

/**
 * rb_encoder_factory_get:
 *
 * Returns the #RBEncoderFactory instance.
 *
 * Return value: the #RBEncoderFactory
 */
RBEncoderFactory *
rb_encoder_factory_get ()
{
	if (g_once_init_enter (&encoder_factory_init)) {
		the_encoder_factory = g_object_new (RB_TYPE_ENCODER_FACTORY, NULL);
		g_once_init_leave (&encoder_factory_init, 1);
	}

	return the_encoder_factory;
}

/**
 * rb_encoder_encode:
 * @encoder: the #RBEncoder
 * @entry: the #RhythmDBEntry to transcode
 * @dest: destination file URI
 * @mime_types: a #GList of target MIME types in order of preference
 *
 * Initiates encoding.  A target MIME type will be selected from the list
 * given.  If the source format is in the list, that will be chosen regardless
 * of order.  Otherwise, the first type in the list that the encoder can produce
 * will be selected.
 *
 * Encoding takes places asynchronously.  If the return value is TRUE, the caller
 * should wait for a 'completed' or 'error' signal to indicate that it has finished.
 *
 * Return value: TRUE if encoding has started
 */
gboolean
rb_encoder_encode (RBEncoder *encoder,
		   RhythmDBEntry *entry,
		   const char *dest,
		   GList *mime_types)
{
	RBEncoderIface *iface = RB_ENCODER_GET_IFACE (encoder);

	return iface->encode (encoder, entry, dest, mime_types);
}

/**
 * rb_encoder_cancel:
 * @encoder: a #RBEncoder
 *
 * Attempts to cancel any in progress encoding.  The encoder should
 * delete the destination file, if it created one.
 */
void
rb_encoder_cancel (RBEncoder *encoder)
{
	RBEncoderIface *iface = RB_ENCODER_GET_IFACE (encoder);

	iface->cancel (encoder);
}

/**
 * rb_encoder_get_preferred_mimetype:
 * @encoder: a #RBEncoder
 * @mime_types: a #GList of MIME type strings in order of preference
 * @mime: returns the selected MIME type, if any
 * @extension: returns the file extension associated with the selected MIME type, if any
 *
 * Identifies the first MIME type in the list that the encoder can actually encode to.
 * The file extension (eg. '.mp3' for audio/mpeg) associated with the selected type is
 * also returned.
 *
 * Return value: TRUE if a format was identified
 */
gboolean
rb_encoder_get_preferred_mimetype (RBEncoder *encoder,
				   GList *mime_types,
				   char **mime,
				   char **extension)
{
	RBEncoderIface *iface = RB_ENCODER_GET_IFACE (encoder);

	return iface->get_preferred_mimetype (encoder, mime_types, mime, extension);
}

/**
 * rb_encoder_new:
 *
 * Creates a new #RBEncoder instance.
 *
 * Return value: the new #RBEncoder
 */
RBEncoder*
rb_encoder_new (void)
{
	return rb_encoder_gst_new ();
}

void
_rb_encoder_emit_progress (RBEncoder *encoder, double fraction)
{
	g_signal_emit (encoder, signals[PROGRESS], 0, fraction);
}

void
_rb_encoder_emit_completed (RBEncoder *encoder, guint64 dest_size)
{
	g_signal_emit (encoder, signals[COMPLETED], 0, dest_size);
}

void
_rb_encoder_emit_error (RBEncoder *encoder, GError *error)
{
	g_signal_emit (encoder, signals[ERROR], 0, error);
}

void
_rb_encoder_emit_prepare_source (RBEncoder *encoder, const char *uri, GObject *source)
{
	g_signal_emit (rb_encoder_factory_get (), signals[PREPARE_SOURCE], 0, uri, source);
}

void
_rb_encoder_emit_prepare_sink (RBEncoder *encoder, const char *uri, GObject *sink)
{
	g_signal_emit (rb_encoder_factory_get (), signals[PREPARE_SINK], 0, uri, sink);
}

GQuark
rb_encoder_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("rb_encoder_error");

	return quark;
}

