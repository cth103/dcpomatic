/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_log.h"
#include "video_filter_graph.h"
#include "video_filter_graph_set.h"

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;


shared_ptr<VideoFilterGraph>
VideoFilterGraphSet::get(dcp::Size size, AVPixelFormat format)
{
	auto graph = std::find_if(
		_graphs.begin(),
		_graphs.end(),
		[size, format](shared_ptr<VideoFilterGraph> g) {
			return g->can_process(size, format);
		});

	if (graph != _graphs.end()) {
		return *graph;
	}

	auto new_graph = make_shared<VideoFilterGraph>(size, format, _frame_rate);
	new_graph->setup(_filters);
	_graphs.push_back(new_graph);

	LOG_GENERAL(N_("New graph for %1x%2, pixel format %3"), size.width, size.height, static_cast<int>(format));

	return new_graph;
}


void
VideoFilterGraphSet::clear()
{
	_graphs.clear();
}

