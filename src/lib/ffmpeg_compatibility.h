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

struct AVFilterInOut;

extern AVFilter* get_sink ();
extern AVFilterInOut* avfilter_inout_alloc ();

#ifndef HAVE_AV_PIXEL_FORMAT
#define AVPixelFormat PixelFormat
#endif

#ifndef HAVE_AV_FRAME_GET_BEST_EFFORT_TIMESTAMP
extern int64_t av_frame_get_best_effort_timestamp (AVFrame const *);
#endif
