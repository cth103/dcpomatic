/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file src/lib/filter_graph.cc
 *  @brief A graph of FFmpeg filters.
 */

#include "filter_graph.h"
#include "filter.h"
#include "exceptions.h"
#include "image.h"
#include "compose.hpp"
extern "C" {
#include <libavfilter/buffersrc.h>
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

/** Construct a FilterGraph for the settings in a piece of content */
FilterGraph::FilterGraph ()
	: _graph (0)
	, _copy (false)
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

	_graph = avfilter_graph_alloc();
	if (!_graph) {
		throw DecodeError (N_("could not create filter graph."));
	}

	AVFilter const * buffer_src = avfilter_get_by_name (src_name().c_str());
	if (!buffer_src) {
		throw DecodeError (N_("could not find buffer src filter"));
	}

	AVFilter const * buffer_sink = avfilter_get_by_name (sink_name().c_str());
	if (!buffer_sink) {
		throw DecodeError (N_("Could not create buffer sink filter"));
	}

	if (avfilter_graph_create_filter (&_buffer_src_context, buffer_src, N_("in"), src_parameters().c_str(), 0, _graph) < 0) {
		throw DecodeError (N_("could not create buffer source"));
	}

	void* sink_params = sink_parameters ();

	if (avfilter_graph_create_filter (&_buffer_sink_context, buffer_sink, N_("out"), 0, sink_params, _graph) < 0) {
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

	if (avfilter_graph_parse (_graph, filters_string.c_str(), inputs, outputs, 0) < 0) {
		throw DecodeError (N_("could not set up filter graph."));
	}

	if (avfilter_graph_config (_graph, 0) < 0) {
		throw DecodeError (N_("could not configure filter graph."));
	}
}

FilterGraph::~FilterGraph ()
{
	if (_frame) {
		av_frame_free (&_frame);
	}

	if (_graph) {
		avfilter_graph_free (&_graph);
	}
}

AVFilterContext *
FilterGraph::get (string name)
{
	return avfilter_graph_get_filter (_graph, name.c_str ());
}
