/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/lib/filter_graph.h
 *  @brief A graph of FFmpeg filters.
 */

#ifndef DVDOMATIC_FILTER_GRAPH_H
#define DVDOMATIC_FILTER_GRAPH_H

#include "util.h"

class Image;
class VideoFilter;
class FFmpegDecoder;

class FilterGraph
{
public:
	virtual ~FilterGraph () {}
	virtual bool can_process (libdcp::Size, AVPixelFormat) const = 0;
	virtual std::list<boost::shared_ptr<Image> > process (AVFrame *) = 0;
};

class EmptyFilterGraph : public FilterGraph
{
public:
	bool can_process (libdcp::Size, AVPixelFormat) const {
		return true;
	}

	std::list<boost::shared_ptr<Image> > process (AVFrame *);
};

/** @class FFmpegFilterGraph
 *  @brief A graph of FFmpeg filters.
 */
class FFmpegFilterGraph : public FilterGraph
{
public:
	FFmpegFilterGraph (boost::shared_ptr<Film> film, FFmpegDecoder* decoder, libdcp::Size s, AVPixelFormat p);
	~FFmpegFilterGraph ();

	bool can_process (libdcp::Size s, AVPixelFormat p) const;
	std::list<boost::shared_ptr<Image> > process (AVFrame * frame);

private:
	AVFilterContext* _buffer_src_context;
	AVFilterContext* _buffer_sink_context;
	libdcp::Size _size; ///< size of the images that this chain can process
	AVPixelFormat _pixel_format; ///< pixel format of the images that this chain can process
	AVFrame* _frame;
};

boost::shared_ptr<FilterGraph> filter_graph_factory (boost::shared_ptr<Film>, FFmpegDecoder *, libdcp::Size, AVPixelFormat);

#endif
