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

/** @file src/lib/filter_graph.cc
 *  @brief A graph of FFmpeg filters.
 */

extern "C" {
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersrc.h>
#if (LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 53 && LIBAVFILTER_VERSION_MINOR <= 77) || LIBAVFILTER_VERSION_MAJOR == 3
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#elif LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR == 15
#include <libavfilter/vsrc_buffer.h>
#endif
#include <libavformat/avio.h>
}
#include "decoder.h"
#include "filter_graph.h"
#include "ffmpeg_compatibility.h"
#include "filter.h"
#include "exceptions.h"
#include "image.h"
#include "film.h"
#include "ffmpeg_decoder.h"

using std::stringstream;
using std::string;
using std::list;
using boost::shared_ptr;

/** Construct a FilterGraph for the settings in a film.
 *  @param film Film.
 *  @param decoder Decoder that we are using.
 *  @param crop true to apply crop, otherwise false.
 *  @param s Size of the images to process.
 *  @param p Pixel format of the images to process.
 */
FilterGraph::FilterGraph (shared_ptr<Film> film, FFmpegDecoder* decoder, bool crop, Size s, AVPixelFormat p)
	: _buffer_src_context (0)
	, _buffer_sink_context (0)
	, _size (s)
	, _pixel_format (p)
{
	string filters = Filter::ffmpeg_strings (film->filters()).first;
	if (!filters.empty ()) {
		filters += ",";
	}

	if (crop) {
		filters += crop_string (Position (film->crop().left, film->crop().top), film->cropped_size (decoder->native_size()));
	} else {
		filters += crop_string (Position (0, 0), decoder->native_size());
	}

	avfilter_register_all ();
	
	AVFilterGraph* graph = avfilter_graph_alloc();
	if (graph == 0) {
		throw DecodeError ("Could not create filter graph.");
	}

	AVFilter* buffer_src = avfilter_get_by_name("buffer");
	if (buffer_src == 0) {
		throw DecodeError ("Could not find buffer src filter");
	}

	AVFilter* buffer_sink = get_sink ();

	stringstream a;
	a << _size.width << ":"
	  << _size.height << ":"
	  << _pixel_format << ":"
	  << decoder->time_base_numerator() << ":"
	  << decoder->time_base_denominator() << ":"
	  << decoder->sample_aspect_ratio_numerator() << ":"
	  << decoder->sample_aspect_ratio_denominator();

	int r;

	if ((r = avfilter_graph_create_filter (&_buffer_src_context, buffer_src, "in", a.str().c_str(), 0, graph)) < 0) {
		throw DecodeError ("could not create buffer source");
	}

	AVBufferSinkParams* sink_params = av_buffersink_params_alloc ();
	PixelFormat* pixel_fmts = new PixelFormat[2];
	pixel_fmts[0] = _pixel_format;
	pixel_fmts[1] = PIX_FMT_NONE;
	sink_params->pixel_fmts = pixel_fmts;
	
	if (avfilter_graph_create_filter (&_buffer_sink_context, buffer_sink, "out", 0, sink_params, graph) < 0) {
		throw DecodeError ("could not create buffer sink.");
	}

	AVFilterInOut* outputs = avfilter_inout_alloc ();
	outputs->name = av_strdup("in");
	outputs->filter_ctx = _buffer_src_context;
	outputs->pad_idx = 0;
	outputs->next = 0;

	AVFilterInOut* inputs = avfilter_inout_alloc ();
	inputs->name = av_strdup("out");
	inputs->filter_ctx = _buffer_sink_context;
	inputs->pad_idx = 0;
	inputs->next = 0;

#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR == 15
	if (avfilter_graph_parse (graph, filters.c_str(), inputs, outputs, 0) < 0) {
		throw DecodeError ("could not set up filter graph.");
	}
#else	
	if (avfilter_graph_parse (graph, filters.c_str(), &inputs, &outputs, 0) < 0) {
		throw DecodeError ("could not set up filter graph.");
	}
#endif	
	
	if (avfilter_graph_config (graph, 0) < 0) {
		throw DecodeError ("could not configure filter graph.");
	}

	/* XXX: leaking `inputs' / `outputs' ? */
}

/** Take an AVFrame and process it using our configured filters, returning a
 *  set of Images.
 */
list<shared_ptr<Image> >
FilterGraph::process (AVFrame const * frame)
{
	list<shared_ptr<Image> > images;
	
#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 53 && LIBAVFILTER_VERSION_MINOR <= 61

	if (av_vsrc_buffer_add_frame (_buffer_src_context, frame, 0) < 0) {
		throw DecodeError ("could not push buffer into filter chain.");
	}

#elif LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR == 15

	AVRational par;
	par.num = sample_aspect_ratio_numerator ();
	par.den = sample_aspect_ratio_denominator ();

	if (av_vsrc_buffer_add_frame (_buffer_src_context, frame, 0, par) < 0) {
		throw DecodeError ("could not push buffer into filter chain.");
	}

#else

	if (av_buffersrc_write_frame (_buffer_src_context, frame) < 0) {
		throw DecodeError ("could not push buffer into filter chain.");
	}

#endif	
	
#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 15 && LIBAVFILTER_VERSION_MINOR <= 61	
	while (avfilter_poll_frame (_buffer_sink_context->inputs[0])) {
#else
	while (av_buffersink_read (_buffer_sink_context, 0)) {
#endif		

#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 15
		
		int r = avfilter_request_frame (_buffer_sink_context->inputs[0]);
		if (r < 0) {
			throw DecodeError ("could not request filtered frame");
		}
		
		AVFilterBufferRef* filter_buffer = _buffer_sink_context->inputs[0]->cur_buf;
		
#else

		AVFilterBufferRef* filter_buffer;
		if (av_buffersink_get_buffer_ref (_buffer_sink_context, &filter_buffer, 0) < 0) {
			filter_buffer = 0;
		}

#endif		
		
		if (filter_buffer) {
			/* This takes ownership of filter_buffer */
			images.push_back (shared_ptr<Image> (new FilterBufferImage ((PixelFormat) frame->format, filter_buffer)));
		}
	}
	
	return images;
}

/** @param s Image size.
 *  @param p Pixel format.
 *  @return true if this chain can process images with `s' and `p', otherwise false.
 */
bool
FilterGraph::can_process (Size s, AVPixelFormat p) const
{
	return (_size == s && _pixel_format == p);
}
