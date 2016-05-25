/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
extern "C" {
#include <libavfilter/buffersink.h>
}

struct AVFilterContext;
struct AVFrame;
class Image;
class Filter;

/** @class FilterGraph
 *  @brief A graph of FFmpeg filters.
 */
class FilterGraph : public boost::noncopyable
{
public:
	FilterGraph ();
	virtual ~FilterGraph ();

	void setup (std::vector<Filter const *>);
	AVFilterContext* get (std::string name);

protected:
	virtual std::string src_parameters () const = 0;
	virtual std::string src_name () const = 0;
	virtual void* sink_parameters () const = 0;
	virtual std::string sink_name () const = 0;

	AVFilterGraph* _graph;
	/** true if this graph has no filters in, so it just copies stuff straight through */
	bool _copy;
	AVFilterContext* _buffer_src_context;
	AVFilterContext* _buffer_sink_context;
	AVFrame* _frame;
};

#endif
