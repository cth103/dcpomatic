/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "audio_filter_graph.h"
#include "audio_buffers.h"
#include "compose.hpp"
extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

AudioFilterGraph::AudioFilterGraph (int sample_rate, int64_t channel_layout)
	: _sample_rate (sample_rate)
	, _channel_layout (channel_layout)
{
	_in_frame = av_frame_alloc ();
}

AudioFilterGraph::~AudioFilterGraph()
{
	av_frame_free (&_in_frame);
}

string
AudioFilterGraph::src_parameters () const
{
	SafeStringStream a;

	char buffer[64];
	av_get_channel_layout_string (buffer, sizeof(buffer), 0, _channel_layout);

	a << "time_base=1/1:sample_rate=" << _sample_rate << ":"
	  << "sample_fmt=" << av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP) << ":"
	  << "channel_layout=" << buffer;

	return a.str ();
}

void *
AudioFilterGraph::sink_parameters () const
{
	AVABufferSinkParams* sink_params = av_abuffersink_params_alloc ();

	AVSampleFormat* sample_fmts = new AVSampleFormat[2];
	sample_fmts[0] = AV_SAMPLE_FMT_FLTP;
	sample_fmts[1] = AV_SAMPLE_FMT_NONE;
	sink_params->sample_fmts = sample_fmts;

	int64_t* channel_layouts = new int64_t[2];
	channel_layouts[0] = _channel_layout;
	channel_layouts[1] = -1;
	sink_params->channel_layouts = channel_layouts;

	sink_params->sample_rates = new int[2];
	sink_params->sample_rates[0] = _sample_rate;
	sink_params->sample_rates[1] = -1;

	return sink_params;
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
AudioFilterGraph::process (shared_ptr<const AudioBuffers> buffers)
{
	_in_frame->extended_data = new uint8_t*[buffers->channels()];
	for (int i = 0; i < buffers->channels(); ++i) {
		if (i < AV_NUM_DATA_POINTERS) {
			_in_frame->data[i] = reinterpret_cast<uint8_t*> (buffers->data(i));
		}
		_in_frame->extended_data[i] = reinterpret_cast<uint8_t*> (buffers->data(i));
	}

	_in_frame->nb_samples = buffers->frames ();
	_in_frame->format = AV_SAMPLE_FMT_FLTP;
	_in_frame->sample_rate = _sample_rate;
	_in_frame->channel_layout = _channel_layout;
	_in_frame->channels = av_get_channel_layout_nb_channels (_channel_layout);

	int r = av_buffersrc_write_frame (_buffer_src_context, _in_frame);

	delete[] _in_frame->extended_data;
	/* Reset extended_data to its original value so that av_frame_free
	   does not try to free it.
	*/
	_in_frame->extended_data = _in_frame->data;

	if (r < 0) {
		char buffer[256];
		av_strerror (r, buffer, sizeof(buffer));
		throw DecodeError (String::compose (N_("could not push buffer into filter chain (%1)"), buffer));
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
