/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "timeline_view.h"


class wxWindow;


class TimelineLabelsView : public TimelineView
{
public:
	explicit TimelineLabelsView (Timeline& tl);

	dcpomatic::Rect<int> bbox () const;

	void set_video_tracks (int n);
	void set_audio_tracks (int n);
	void set_text_tracks (int n);
	void set_atmos (bool s);

private:
	void do_paint (wxGraphicsContext* gc, std::list<dcpomatic::Rect<int>> overlaps);

	int _width = 0;
	int _video_tracks = 0;
	int _audio_tracks = 0;
	int _text_tracks = 0;
	bool _atmos = true;
};
