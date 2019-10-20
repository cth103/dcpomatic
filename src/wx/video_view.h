/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_VIDEO_VIEW_H
#define DCPOMATIC_VIDEO_VIEW_H

#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class Image;
class wxWindow;
class FilmViewer;

class VideoView
{
public:
	VideoView (FilmViewer* viewer)
		: _viewer (viewer)
#ifdef DCPOMATIC_VARIANT_SWAROOP
		, _in_watermark (false)
#endif
	{}

	virtual ~VideoView () {}

	virtual void set_image (boost::shared_ptr<const Image> image) = 0;
	virtual wxWindow* get () const = 0;
	virtual void update () = 0;

	/* XXX_b: make pure */
	virtual void start () {}

	boost::signals2::signal<void()> Sized;

	/* XXX_b: to remove */
	virtual bool get (bool) {
		return true;
	}
	/* XXX_b: to remove */
	virtual void display_player_video () {}

protected:
	FilmViewer* _viewer;

#ifdef DCPOMATIC_VARIANT_SWAROOP
	bool _in_watermark;
	int _watermark_x;
	int _watermark_y;
#endif
};

#endif
