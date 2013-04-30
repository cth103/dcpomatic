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
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavformat/avio.h>
}
#include "decoder.h"
#include "filter_graph.h"
#include "filter.h"
#include "exceptions.h"
#include "image.h"
#include "film.h"
#include "ffmpeg_decoder.h"

#include "i18n.h"

using std::stringstream;
using std::string;
using std::list;
using boost::shared_ptr;
using libdcp::Size;

/** Construct a FilterGraph for the settings in a film.
 *  @param film Film.
 *  @param decoder Decoder that we are using.
 *  @param s Size of the images to process.
 *  @param p Pixel format of the images to process.
 */
FilterGraph::FilterGraph (shared_ptr<Film> film, FFmpegDecoder* decoder, libdcp::Size s, AVPixelFormat p)
	: _buffer_src_context (0)
	, _buffer_sink_context (0)
	, _size (s)
	, _pixel_format (p)
{
	string filters = Filter::ffmpeg_strings (film->filters()).first;
	if (!filters.empty ()) {
		filters += N_(",");
	}

	filters += crop_string (Position (film->crop().left, film->crop().top), film->cropped_size (decoder->native_size()));

	AVFilterGraph* graph = avfilter_graph_alloc();
	if (graph == 0) {
		throw DecodeError (N_("could not create filter graph."));
	}

	AVFilter* buffer_src = avfilter_get_by_name(N_("buffer"));
	if (buffer_src == 0) {
		throw DecodeError (N_("could not find buffer src filter"));
	}

	AVFilter* buffer_sink = avfilter_get_by_name(N_("buffersink"));
	if (buffer_sink == 0) {
		throw DecodeError (N_("Could not create buffer sink filter"));
	}

	stringstream a;
	a << _size.width << N_(":")
	  << _size.height << N_(":")
	  << _pixel_format << N_(":")
	  << decoder->time_base_numerator() << N_(":")
	  << decoder->time_base_denominator() << N_(":")
	  << decoder->sample_aspect_ratio_numerator() << N_(":")
	  << decoder->sample_aspect_ratio_denominator();

	int r;

	if ((r = avfilter_graph_create_filter (&_buffer_src_context, buffer_src, N_("in"), a.str().c_str(), 0, graph)) < 0) {
		throw DecodeError (N_("could not create buffer source"));
	}

	AVBufferSinkParams* sink_params = av_buffersink_params_alloc ();
	PixelFormat* pixel_fmts = new PixelFormat[2];
	pixel_fmts[0] = _pixel_format;
	pixel_fmts[1] = PIX_FMT_NONE;
	sink_params->pixel_fmts = pixel_fmts;
	
	if (avfilter_graph_create_filter (&_buffer_sink_context, buffer_sink, N_("out"), 0, sink_params, graph) < 0) {
		throw DecodeError (N_("could not create buffer sink."));
	}

	AVFilterInOut* outputs = avfilter_inout_alloc ();
	outputs->name = av_strdup(N_("in"));
	outputs->filter_ctx = _buffer_src_context;
	outputs->pad_idx = 0;
	outputs->next = 0;

	AVFilterInOut* inputs = avfilter_inout_alloc ();
	inputs->name = av_strdup(N_("out"));
	inputs->filter_ctx = _buffer_sink_context;
	inputs->pad_idx = 0;
	inputs->next = 0;

	if (avfilter_graph_parse (graph, filters.c_str(), &inputs, &outputs, 0) < 0) {
		throw DecodeError (N_("could not set up filter graph."));
	}
	
	if (avfilter_graph_config (graph, 0) < 0) {
		throw DecodeError (N_("could not configure filter graph."));
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
	

	if (av_buffersrc_write_frame (_buffer_src_context, frame) < 0) {
		throw DecodeError (N_("could not push buffer into filter chain."));
	}

	while (av_buffersink_read (_buffer_sink_context, 0)) {
		AVFilterBufferRef* filter_buffer;
		if (av_buffersink_get_buffer_ref (_buffer_sink_context, &filter_buffer, 0) < 0) {
			filter_buffer = 0;
		}

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
FilterGraph::can_process (libdcp::Size s, AVPixelFormat p) const
{
	return (_size == s && _pixel_format == p);
}
