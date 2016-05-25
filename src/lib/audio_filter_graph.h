/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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
extern "C" {
#include <libavfilter/buffersink.h>
}

class AudioBuffers;

class AudioFilterGraph : public FilterGraph
{
public:
	AudioFilterGraph (int sample_rate, int channels);
	~AudioFilterGraph ();

	void process (boost::shared_ptr<const AudioBuffers> audio);

protected:
	std::string src_parameters () const;
	std::string src_name () const;
	void* sink_parameters () const;
	std::string sink_name () const;

private:
	int _sample_rate;
	int _channels;
	int64_t _channel_layout;
	AVFrame* _in_frame;
};
