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


#include "audio_buffers.h"
#include "audio_filter_graph.h"
#include "compose.hpp"
extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}
#include <iostream>

#include "i18n.h"


using std::cout;
using std::make_shared;
using std::shared_ptr;
using std::string;


AudioFilterGraph::AudioFilterGraph (int sample_rate, int channels)
	: _sample_rate (sample_rate)
	, _channels (channels)
{
	/* FFmpeg doesn't know any channel layouts for any counts between 8 and 16,
	   so we need to tell it we're using 16 channels if we are using more than 8.
	*/
	av_channel_layout_default(&_channel_layout, _channels > 8 ? 16 : _channels);

	_in_frame = av_frame_alloc ();
	if (_in_frame == nullptr) {
		throw std::bad_alloc();
	}
}

AudioFilterGraph::~AudioFilterGraph()
{
	av_frame_free (&_in_frame);
}

string
AudioFilterGraph::src_parameters () const
{
	char layout[64];
	av_channel_layout_describe(&_channel_layout, layout, sizeof(layout));

	char buffer[256];
	snprintf (
		buffer, sizeof(buffer), "time_base=1/1:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
		_sample_rate, av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP), layout
		);

	return buffer;
}


void
AudioFilterGraph::set_parameters (AVFilterContext* context) const
{
	AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
	int r = av_opt_set_int_list (context, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	DCPOMATIC_ASSERT (r >= 0);

	char ch_layout[64];
	av_channel_layout_describe(&_channel_layout, ch_layout, sizeof(ch_layout));
	r = av_opt_set(context, "ch_layouts", ch_layout, AV_OPT_SEARCH_CHILDREN);
	DCPOMATIC_ASSERT (r >= 0);

	int sample_rates[] = { _sample_rate, -1 };
	r = av_opt_set_int_list (context, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
	DCPOMATIC_ASSERT (r >= 0);
}


string
AudioFilterGraph::src_name () const
{
	return "abuffer";
}

string
AudioFilterGraph::sink_name () const
{
	return "abuffersink";
}

void
AudioFilterGraph::process (shared_ptr<AudioBuffers> buffers)
{
	DCPOMATIC_ASSERT (buffers->frames() > 0);
	int const process_channels = _channel_layout.nb_channels;
	DCPOMATIC_ASSERT (process_channels >= buffers->channels());

	if (buffers->channels() < process_channels) {
		/* We are processing more data than we actually have (see the hack in
		   the constructor) so we need to create new buffers with some extra
		   silent channels.
		*/
		auto extended_buffers = make_shared<AudioBuffers>(process_channels, buffers->frames());
		for (int i = 0; i < buffers->channels(); ++i) {
			extended_buffers->copy_channel_from (buffers.get(), i, i);
		}
		for (int i = buffers->channels(); i < process_channels; ++i) {
			extended_buffers->make_silent (i);
		}

		buffers = extended_buffers;
	}

	_in_frame->extended_data = new uint8_t*[process_channels];
	for (int i = 0; i < buffers->channels(); ++i) {
		if (i < AV_NUM_DATA_POINTERS) {
			_in_frame->data[i] = reinterpret_cast<uint8_t*> (buffers->data(i));
		}
		_in_frame->extended_data[i] = reinterpret_cast<uint8_t*> (buffers->data(i));
	}

	_in_frame->nb_samples = buffers->frames ();
	_in_frame->format = AV_SAMPLE_FMT_FLTP;
	_in_frame->sample_rate = _sample_rate;
	_in_frame->ch_layout = _channel_layout;
LIBDCP_DISABLE_WARNINGS
	_in_frame->channels = process_channels;
LIBDCP_ENABLE_WARNINGS

	int r = av_buffersrc_write_frame (_buffer_src_context, _in_frame);

	delete[] _in_frame->extended_data;
	/* Reset extended_data to its original value so that av_frame_free
	   does not try to free it.
	*/
	_in_frame->extended_data = _in_frame->data;

	if (r < 0) {
		char buffer[256];
		av_strerror (r, buffer, sizeof(buffer));
		throw DecodeError (String::compose (N_("could not push buffer into filter chain (%1)"), &buffer[0]));
	}

	while (true) {
		if (av_buffersink_get_frame (_buffer_sink_context, _frame) < 0) {
			break;
		}

		/* We don't extract audio data here, since the only use of this class
		   is for ebur128.
		*/

		av_frame_unref (_frame);
	}
}
