/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "video_filter_graph.h"
#include "image.h"
#include "compose.hpp"
extern "C" {
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#include "i18n.h"

using std::list;
using std::pair;
using std::vector;
using std::string;
using std::make_pair;
using boost::shared_ptr;

VideoFilterGraph::VideoFilterGraph (dcp::Size s, AVPixelFormat p)
	: _size (s)
	, _pixel_format (p)
{

}

/** Take an AVFrame and process it using our configured filters, returning a
 *  set of Images.  Caller handles memory management of the input frame.
 */
list<pair<shared_ptr<Image>, int64_t> >
VideoFilterGraph::process (AVFrame* frame)
{
	list<pair<shared_ptr<Image>, int64_t> > images;

	if (_copy) {
		images.push_back (make_pair (shared_ptr<Image> (new Image (frame)), av_frame_get_best_effort_timestamp (frame)));
	} else {
		int r = av_buffersrc_write_frame (_buffer_src_context, frame);
		if (r < 0) {
			throw DecodeError (String::compose (N_("could not push buffer into filter chain (%1)."), r));
		}

		while (true) {
			if (av_buffersink_get_frame (_buffer_sink_context, _frame) < 0) {
				break;
			}

			images.push_back (make_pair (shared_ptr<Image> (new Image (_frame)), av_frame_get_best_effort_timestamp (_frame)));
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
	SafeStringStream a;

	a << "video_size=" << _size.width << "x" << _size.height << ":"
	  << "pix_fmt=" << _pixel_format << ":"
	  << "time_base=1/1:"
	  << "pixel_aspect=1/1";

	return a.str ();
}

void *
VideoFilterGraph::sink_parameters () const
{
	AVBufferSinkParams* sink_params = av_buffersink_params_alloc ();
	AVPixelFormat* pixel_fmts = new AVPixelFormat[2];
	pixel_fmts = new AVPixelFormat[2];
	pixel_fmts[0] = _pixel_format;
	pixel_fmts[1] = AV_PIX_FMT_NONE;
	sink_params->pixel_fmts = pixel_fmts;
	return sink_params;
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

