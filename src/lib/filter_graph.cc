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
#include "ffmpeg_content.h"
#include "safe_stringstream.h"

#include "i18n.h"

using std::string;
using std::list;
using std::pair;
using std::make_pair;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;
using dcp::Size;

/** Construct a FilterGraph for the settings in a piece of content.
 *  @param content Content.
 *  @param s Size of the images to process.
 *  @param p Pixel format of the images to process.
 */
FilterGraph::FilterGraph (shared_ptr<const FFmpegContent> content, dcp::Size s, AVPixelFormat p)
	: _copy (false)
	, _buffer_src_context (0)
	, _buffer_sink_context (0)
	, _size (s)
	, _pixel_format (p)
	, _frame (0)
{
	string const filters = Filter::ffmpeg_string (content->filters());
	if (filters.empty ()) {
		_copy = true;
		return;
	}

	_frame = av_frame_alloc ();
	
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

	SafeStringStream a;
	a << "video_size=" << _size.width << "x" << _size.height << ":"
	  << "pix_fmt=" << _pixel_format << ":"
	  << "time_base=1/1:"
	  << "pixel_aspect=1/1";

	if (avfilter_graph_create_filter (&_buffer_src_context, buffer_src, "in", a.str().c_str(), 0, graph) < 0) {
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

	av_free (sink_params);

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

	if (avfilter_graph_parse (graph, filters.c_str(), inputs, outputs, 0) < 0) {
		throw DecodeError (N_("could not set up filter graph."));
	}
	
	if (avfilter_graph_config (graph, 0) < 0) {
		throw DecodeError (N_("could not configure filter graph."));
	}

	avfilter_inout_free (&inputs);
	avfilter_inout_free (&outputs);
}

FilterGraph::~FilterGraph ()
{
	if (_frame) {
		av_frame_free (&_frame);
	}
}

/** Take an AVFrame and process it using our configured filters, returning a
 *  set of Images.  Caller handles memory management of the input frame.
 */
list<pair<shared_ptr<Image>, int64_t> >
FilterGraph::process (AVFrame* frame)
{
	list<pair<shared_ptr<Image>, int64_t> > images;

	if (_copy) {
		images.push_back (make_pair (shared_ptr<Image> (new Image (frame)), av_frame_get_best_effort_timestamp (frame)));
	} else {
		int r = av_buffersrc_write_frame (_buffer_src_context, frame);
		if (r < 0) {
			throw DecodeError (String::compose (N_("could not push buffer into filter chain (%1)."), r));
		}
		
		while (true) {
			if (av_buffersink_get_frame (_buffer_sink_context, _frame) < 0) {
				break;
			}
			
			images.push_back (make_pair (shared_ptr<Image> (new Image (_frame)), av_frame_get_best_effort_timestamp (_frame)));
			av_frame_unref (_frame);
		}
	}
		
	return images;
}

/** @param s Image size.
 *  @param p Pixel format.
 *  @return true if this chain can process images with `s' and `p', otherwise false.
 */
bool
FilterGraph::can_process (dcp::Size s, AVPixelFormat p) const
{
	return (_size == s && _pixel_format == p);
}
