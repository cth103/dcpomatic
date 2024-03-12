/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "dcp_timeline_view.h"


class DCPTimeline;


class DCPTimelineReelMarkerView : public DCPTimelineView
{
public:
	DCPTimelineReelMarkerView(DCPTimeline& timeline, int y_pos);

	dcpomatic::Rect<int> bbox() const override;

	dcpomatic::DCPTime time() const {
		return _time;
	}

	void set_time(dcpomatic::DCPTime time) {
		_time = time;
	}

	void set_active(bool active) {
		_active = active;
	}

	static auto constexpr HEAD_SIZE = 16;
	static auto constexpr TAIL_LENGTH = 28;
	static auto constexpr HEIGHT = HEAD_SIZE + TAIL_LENGTH;

private:
	void do_paint(wxGraphicsContext* gc) override;
	int x_pos() const;

	dcpomatic::DCPTime _time;
	int _y_pos;
	bool _active = false;
};

