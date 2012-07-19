/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

extern "C" {
#include <libavfilter/avfiltergraph.h>
}
#include "exceptions.h"

#ifdef DVDOMATIC_FFMPEG_0_8_3	

typedef struct {
	enum PixelFormat pix_fmt;
} AVSinkContext;

static int
avsink_init (AVFilterContext* ctx, const char* args, void* opaque)
{
	AVSinkContext* priv = (AVSinkContext *) ctx->priv;
	if (!opaque) {
		return AVERROR (EINVAL);
	}

	*priv = *(AVSinkContext *) opaque;
	return 0;
}

static void
null_end_frame (AVFilterLink *)
{

}

static int
avsink_query_formats (AVFilterContext* ctx)
{
	AVSinkContext* priv = (AVSinkContext *) ctx->priv;
	enum PixelFormat pix_fmts[] = {
		priv->pix_fmt,
		PIX_FMT_NONE
	};

	avfilter_set_common_formats (ctx, avfilter_make_format_list ((int *) pix_fmts));
	return 0;
}

#endif

AVFilter*
get_sink ()
{
#ifdef DVDOMATIC_FFMPEG_0_8_3	
	/* XXX does this leak stuff? */
	AVFilter* buffer_sink = new AVFilter;
	buffer_sink->name = av_strdup ("avsink");
	buffer_sink->priv_size = sizeof (AVSinkContext);
	buffer_sink->init = avsink_init;
	buffer_sink->query_formats = avsink_query_formats;
	buffer_sink->inputs = new AVFilterPad[2];
	AVFilterPad* i0 = const_cast<AVFilterPad*> (&buffer_sink->inputs[0]);
	i0->name = "default";
	i0->type = AVMEDIA_TYPE_VIDEO;
	i0->min_perms = AV_PERM_READ;
	i0->rej_perms = 0;
	i0->start_frame = 0;
	i0->get_video_buffer = 0;
	i0->get_audio_buffer = 0;
	i0->end_frame = null_end_frame;
	i0->draw_slice = 0;
	i0->filter_samples = 0;
	i0->poll_frame = 0;
	i0->request_frame = 0;
	i0->config_props = 0;
	const_cast<AVFilterPad*> (&buffer_sink->inputs[1])->name = 0;
	buffer_sink->outputs = new AVFilterPad[1];
	const_cast<AVFilterPad*> (&buffer_sink->outputs[0])->name = 0;
	return buffer_sink;
#else
	AVFilter* buffer_sink = avfilter_get_by_name("buffersink");
	if (buffer_sink == 0) {
		throw DecodeError ("Could not create buffer sink filter");
	}

	return buffer_sink;
#endif
}

#ifdef DVDOMATIC_FFMPEG_0_8_3
AVFilterInOut *
avfilter_inout_alloc ()
{
	return (AVFilterInOut *) av_malloc (sizeof (AVFilterInOut));
}
#endif
