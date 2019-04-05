/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/dcpomatic_time.h"
#include "lib/player.h"
#include "lib/text_ring_buffers.h"
#include <wx/wx.h>

class Butler;
class FilmViewer;

class ClosedCaptionsDialog : public wxDialog
{
public:
	explicit ClosedCaptionsDialog (wxWindow* parent, FilmViewer* viewer);

	void update (DCPTime);
	void clear ();
	void set_film_and_butler (boost::shared_ptr<Film>, boost::weak_ptr<Butler>);

private:
	void paint ();
	void track_selected ();

	FilmViewer* _viewer;
	wxPanel* _display;
	wxChoice* _track;
	boost::optional<TextRingBuffers::Data> _current;
	bool _current_in_lines;
	std::vector<wxString> _lines;
	std::vector<DCPTextTrack> _tracks;
	boost::weak_ptr<Butler> _butler;
	DCPTime _last_update;
};
