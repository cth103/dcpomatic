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


#ifndef DCPOMATIC_VIDEO_FILTER_GRAPH_SET_H
#define DCPOMATIC_VIDEO_FILTER_GRAPH_SET_H


#include <dcp/types.h>
extern "C" {
#include <libavutil/avutil.h>
}
#include <memory>
#include <vector>


class Filter;
class VideoFilterGraph;


class VideoFilterGraphSet
{
public:
	VideoFilterGraphSet(std::vector<Filter> const& filters, dcp::Fraction frame_rate)
		: _filters(filters)
		, _frame_rate(frame_rate)
	{}

	VideoFilterGraphSet(VideoFilterGraphSet const&) = delete;
	VideoFilterGraphSet& operator=(VideoFilterGraphSet const&) = delete;

	std::shared_ptr<VideoFilterGraph> get(dcp::Size size, AVPixelFormat format);

	void clear();

private:
	std::vector<Filter> _filters;
	dcp::Fraction _frame_rate;
	std::vector<std::shared_ptr<VideoFilterGraph>> _graphs;
};


#endif

