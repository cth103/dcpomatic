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

#include "filter_graph.h"
#include "filter.h"
#include "exceptions.h"
#include "image.h"
#include "safe_stringstream.h"
#include "compose.hpp"
extern "C" {
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavformat/avio.h>
}
#include <iostream>

#include "i18n.h"

using std::string;
using std::list;
using std::pair;
using std::make_pair;
using std::cout;
using std::vector;
using boost::shared_ptr;
using boost::weak_ptr;
using dcp::Size;

/** Construct a FilterGraph for the settings in a piece of content.
 *  @param content Content.
 *  @param s Size of the images to process.
 *  @param p Pixel format of the images to process.
 */
FilterGraph::FilterGraph ()
	: _copy (false)
	, _buffer_src_context (0)
	, _buffer_sink_context (0)
	, _frame (0)
{

}

void
FilterGraph::setup (vector<Filter const *> filters)
{
	string const filters_string = Filter::ffmpeg_string (filters);
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

	if (avfilter_graph_create_filter (&_buffer_src_context, buffer_src, "in", src_parameters().c_str(), 0, graph) < 0) {
		throw DecodeError (N_("could not create buffer source"));
	}

	AVBufferSinkParams* sink_params = sink_parameters ();

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

	if (avfilter_graph_parse (graph, filters_string.c_str(), inputs, outputs, 0) < 0) {
		throw DecodeError (N_("could not set up filter graph."));
	}

	if (avfilter_graph_config (graph, 0) < 0) {
		throw DecodeError (N_("could not configure filter graph."));
	}
}

FilterGraph::~FilterGraph ()
{
	if (_frame) {
		av_frame_free (&_frame);
	}
}
