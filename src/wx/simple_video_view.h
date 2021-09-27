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

#include "video_view.h"
#include "lib/position.h"
#include "lib/warnings.h"
#include <dcp/types.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS

class FilmViewer;

class SimpleVideoView : public VideoView
{
public:
	SimpleVideoView (FilmViewer* viewer, wxWindow* parent);

	wxWindow* get () const override {
		return _panel;
	}

	void update () override;
	void start () override;
	NextFrameResult display_next_frame (bool non_blocking) override;

private:
	void set_image (std::shared_ptr<const Image> image) {
		_image = image;
	}

	void refresh_panel ();
	void paint ();
	void timer ();

	wxPanel* _panel;
	std::shared_ptr<const Image> _image;
	wxTimer _timer;
	Position<int> _inter_position;
	dcp::Size _inter_size;
};
