/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "filter_graph.h"

class VideoFilterGraph : public FilterGraph
{
public:
	VideoFilterGraph (dcp::Size s, AVPixelFormat p, dcp::Fraction r);

	bool can_process (dcp::Size s, AVPixelFormat p) const;
	std::list<std::pair<boost::shared_ptr<Image>, int64_t> > process (AVFrame * frame);

protected:
	std::string src_parameters () const;
	std::string src_name () const;
	void* sink_parameters () const;
	std::string sink_name () const;

private:
	dcp::Size _size; ///< size of the images that this chain can process
	AVPixelFormat _pixel_format; ///< pixel format of the images that this chain can process
	dcp::Fraction _frame_rate;
};
