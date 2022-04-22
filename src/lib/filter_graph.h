/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file src/lib/filter_graph.h
 *  @brief A graph of FFmpeg filters.
 */


#ifndef DCPOMATIC_FILTER_GRAPH_H
#define DCPOMATIC_FILTER_GRAPH_H


#include "util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavfilter/buffersink.h>
}
LIBDCP_ENABLE_WARNINGS


struct AVFilterContext;
struct AVFrame;
class Image;
class Filter;


/** @class FilterGraph
 *  @brief A graph of FFmpeg filters.
 */
class FilterGraph
{
public:
	FilterGraph() = default;
	virtual ~FilterGraph ();

	FilterGraph (FilterGraph const&) = delete;
	FilterGraph& operator== (FilterGraph const&) = delete;

	void setup (std::vector<Filter const *>);
	AVFilterContext* get (std::string name);

protected:
	virtual std::string src_parameters () const = 0;
	virtual std::string src_name () const = 0;
	virtual void set_parameters (AVFilterContext* context) const = 0;
	virtual std::string sink_name () const = 0;

	AVFilterGraph* _graph = nullptr;
	/** true if this graph has no filters in, so it just copies stuff straight through */
	bool _copy = false;
	AVFilterContext* _buffer_src_context = nullptr;
	AVFilterContext* _buffer_sink_context = nullptr;
	AVFrame* _frame = nullptr;
};


#endif
