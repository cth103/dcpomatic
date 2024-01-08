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


#include "compose.hpp"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "image.h"
#include "video_filter_graph.h"
#include <dcp/scope_guard.h>
extern "C" {
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
}

#include "i18n.h"


using std::list;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;


VideoFilterGraph::VideoFilterGraph (dcp::Size s, AVPixelFormat p, dcp::Fraction r)
	: _size (s)
	, _pixel_format (p)
	, _frame_rate (r)
{

}


list<shared_ptr<const Image>>
VideoFilterGraph::process(shared_ptr<const Image> image)
{
	if (_copy) {
		return { image };
	}

	auto frame = av_frame_alloc();
	if (!frame) {
		throw std::bad_alloc();
	}

	dcp::ScopeGuard sg = [&frame]() { av_frame_free(&frame); };

	for (int i = 0; i < image->planes(); ++i) {
		frame->data[i] = image->data()[i];
		frame->linesize[i] = image->stride()[i];
	}

	frame->width = image->size().width;
	frame->height = image->size().height;
	frame->format = image->pixel_format();

	int r = av_buffersrc_write_frame(_buffer_src_context, frame);
	if (r < 0) {
		throw DecodeError(String::compose(N_("could not push buffer into filter chain (%1)."), r));
	}

	list<shared_ptr<const Image>> images;

	while (true) {
		if (av_buffersink_get_frame(_buffer_sink_context, _frame) < 0) {
			break;
		}

		images.push_back(make_shared<Image>(_frame, Image::Alignment::PADDED));
		av_frame_unref (_frame);
	}

	return images;
}


/** Take an AVFrame and process it using our configured filters, returning a
 *  set of Images.  Caller handles memory management of the input frame.
 */
list<pair<shared_ptr<const Image>, int64_t>>
VideoFilterGraph::process (AVFrame* frame)
{
	list<pair<shared_ptr<const Image>, int64_t>> images;

	if (_copy) {
		images.push_back (make_pair(make_shared<Image>(frame, Image::Alignment::PADDED), frame->best_effort_timestamp));
	} else {
		int r = av_buffersrc_write_frame (_buffer_src_context, frame);
		if (r < 0) {
			throw DecodeError (String::compose(N_("could not push buffer into filter chain (%1)."), r));
		}

		while (true) {
			if (av_buffersink_get_frame (_buffer_sink_context, _frame) < 0) {
				break;
			}

			images.push_back (make_pair(make_shared<Image>(_frame, Image::Alignment::PADDED), frame->best_effort_timestamp));
			av_frame_unref (_frame);
		}
	}

	return images;
}


/** @param s Image size.
 *  @param p Pixel format.
 *  @return true if this chain can process images with `s' and `p', otherwise false.
 */
bool
VideoFilterGraph::can_process (dcp::Size s, AVPixelFormat p) const
{
	return (_size == s && _pixel_format == p);
}


string
VideoFilterGraph::src_parameters () const
{
	char buffer[256];
	snprintf (
		buffer, sizeof(buffer),
		"video_size=%dx%d:pix_fmt=%d:frame_rate=%d/%d:time_base=1/1:pixel_aspect=1/1",
		_size.width, _size.height,
		_pixel_format,
		_frame_rate.numerator, _frame_rate.denominator
		);
	return buffer;
}


void
VideoFilterGraph::set_parameters (AVFilterContext* context) const
{
	AVPixelFormat pix_fmts[] = { _pixel_format, AV_PIX_FMT_NONE };
	int r = av_opt_set_int_list (context, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	DCPOMATIC_ASSERT (r >= 0);
}


string
VideoFilterGraph::src_name () const
{
	return "buffer";
}


string
VideoFilterGraph::sink_name () const
{
	return "buffersink";
}
