/* GdkPixbuf library - JPEG2000 Image Loader
 *
 * Copyright © 2020 Nichlas Severinsen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define GDK_PIXBUF_ENABLE_BACKEND
	#include <gdk-pixbuf/gdk-pixbuf.h>
#undef  GDK_PIXBUF_ENABLE_BACKEND

#include <openjpeg.h>
#include <string.h>
#include <utilities.h>

static void free_buffer (guchar *pixels, gpointer data)
{
	g_free(pixels);
}

static GdkPixbuf *gdk_pixbuf__jp2_image_load(FILE *fp, GError **error)
{
	GdkPixbuf *pixbuf = NULL;
	opj_codec_t *codec = NULL;
	opj_image_t *image = NULL;
	opj_stream_t *stream = NULL;
	opj_dparameters_t parameters;

	opj_set_default_decoder_parameters(&parameters);

	stream = util_create_stream(fp);
	if(!stream)
	{
		util_destroy(codec, stream, image);
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "Failed to create stream fom file");
		return FALSE;
	}

	codec = opj_create_decompress(util_identify(fp));

	#if DEBUG == TRUE
		opj_set_info_handler(codec, info_callback, 00);
		opj_set_warning_handler(codec, warning_callback, 00);
		opj_set_error_handler(codec, error_callback, 00);
	#endif

	if(!opj_setup_decoder(codec, &parameters))
	{
		util_destroy(codec, stream, image);
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "Failed to setup decoder");
		return FALSE;
	}

	if(!opj_codec_set_threads(codec, 1))
	{
		util_destroy(codec, stream, image);
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "Failed to set thread count");
		return FALSE;
	}

	if(!opj_read_header(stream, codec, &image))
	{
		util_destroy(codec, stream, image);
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "Failed to read header");
		return FALSE;
	}

	if(!opj_decode(codec, stream, image) && opj_end_decompress(codec, stream))
	{
		util_destroy(codec, stream, image);
		g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "Failed to decode the image");
		return FALSE;
	}

	opj_stream_destroy(stream);

	// Find colorspace

	GdkColorspace colorspace = -1;

	switch(image->color_space)
	{
		case OPJ_CLRSPC_SRGB:
			colorspace = GDK_COLORSPACE_RGB;
			break;
		default:
			util_destroy(codec, stream, image);
			g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "Unsupported colorspace");
			return FALSE;
	}

	// Check if has_alpha

	gboolean has_alpha = (image->numcomps == 4 || image->numcomps == 2);

	// Get adjusts (could probably be a separate function)

	int adjustR, adjustG, adjustB, adjustA;

	if(has_alpha)
	{
		adjustA = (image->comps[image->numcomps -1].sgnd ? 1 << (image->comps[image->numcomps - 1].prec - 1) : 0);
	}
	else
	{
		adjustA = 0;
	}

	adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

	if(image->numcomps > 2)
	{
		adjustG = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
		adjustB = (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
	}
	else
	{
		adjustG = adjustB = 0;
	}

	// Get data

	guint8 *data = g_malloc(sizeof(guint8) * (int) image->comps[0].w * (int) image->comps[0].h * image->numcomps); // 8 bit * width * height * number of components

	for(int i = 0; i < (int) image->comps[0].w * (int) image->comps[0].h; i += image->numcomps)
	{
		data[i] = util_get(image, 0, i, adjustR);

		if(image->numcomps > 2)
		{
			data[i+1] = util_get(image, 1, i, adjustG);
			data[i+2] = util_get(image, 2, i, adjustB);
		}

		if(has_alpha)
		{
			data[i+3] = util_get(image, image->numcomps - 1, i, adjustA);
		}
	}

	pixbuf = gdk_pixbuf_new_from_data(
		(const guchar*) data,    // Actual data. RGB: {0, 0, 0}. RGBA: {0, 0, 0, 0}.
		colorspace,              // Colorspace
		has_alpha,               // has_alpha
		8,                       // bits_per_sample
		(int) image->comps[0].w, // width
		(int) image->comps[0].h, // height
		util_rowstride(image),   // rowstride: distance in bytes between row starts
		free_buffer,             // destroy function
		NULL                     // closure data to pass to the destroy notification function
	);

	return pixbuf;
}

static gpointer gdk_pixbuf__jp2_image_begin_load
(
	GdkPixbufModuleSizeFunc size_func,
	GdkPixbufModulePreparedFunc prepare_func,
	GdkPixbufModuleUpdatedFunc update_func,
	gpointer user_data,
	GError **error
) {
	return NULL;
}

static gboolean gdk_pixbuf__jp2_image_stop_load(gpointer context, GError **error)
{
	return TRUE;
}

static gboolean gdk_pixbuf__jp2_image_load_increment(gpointer context, const guchar *buf, guint size, GError **error)
{
	return TRUE;
}

static gboolean gdk_pixbuf__jp2_image_save
(
	FILE *f,
	GdkPixbuf *pixbuf,
	gchar **keys,
	gchar **values,
	GError **error
) {
	return TRUE;
}

static gboolean gdk_pixbuf__jp2_image_save_to_callback
(
	GdkPixbufSaveFunc save_func,
	gpointer user_data,
	GdkPixbuf *pixbuf,
	gchar **keys,
	gchar **values,
	GError **error
) {
	return TRUE;
}

/**
 * Module entry points - This is where it all starts
 */

void fill_vtable(GdkPixbufModule *module)
{
	module->load             = gdk_pixbuf__jp2_image_load;
	module->save             = gdk_pixbuf__jp2_image_save;
	module->stop_load        = gdk_pixbuf__jp2_image_stop_load;
	module->begin_load       = gdk_pixbuf__jp2_image_begin_load;
	module->load_increment   = gdk_pixbuf__jp2_image_load_increment;
	module->save_to_callback = gdk_pixbuf__jp2_image_save_to_callback;
}

void fill_info(GdkPixbufFormat *info)
{
	static GdkPixbufModulePattern signature[] =
	{
		{ "    jP", "!!!!  ", 100 },		/* file begins with 'jP' at offset 4 */
		{ "\xff\x4f\xff\x51\x00", NULL, 100 },	/* file starts with FF 4F FF 51 00 */
		{ NULL, NULL, 0 }
	};

	static gchar *mime_types[] =
	{
		"image/jp2",
		"image/jpm",
		"image/jpx",
		"image/jpeg2000",
		NULL
	};

	static gchar *extensions[] =
	{
		"j2k",
		"jp2",
		"jpc",
		"jpf",
		"jpm",
		"jpx",
		NULL
	};

	info->description = "JPEG2000";
	info->extensions  = extensions;
	info->flags       = GDK_PIXBUF_FORMAT_WRITABLE | GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license     = "LGPL";
	info->mime_types  = mime_types;
	info->name        = "jp2";
	info->signature   = signature;
}