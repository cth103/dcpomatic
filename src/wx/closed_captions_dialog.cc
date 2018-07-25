/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "closed_captions_dialog.h"
#include "lib/string_text.h"
#include <boost/bind.hpp>

using std::list;
using std::cout;
using std::make_pair;
using boost::shared_ptr;
using boost::weak_ptr;

int const ClosedCaptionsDialog::_num_lines = 3;
int const ClosedCaptionsDialog::_num_chars_per_line = 30;

ClosedCaptionsDialog::ClosedCaptionsDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Closed captions"), wxDefaultPosition, wxDefaultSize,
#ifdef DCPOMATIC_OSX
		/* I can't get wxFRAME_FLOAT_ON_PARENT to work on OS X, and although wxSTAY_ON_TOP keeps
		   the window above all others (and not just our own) it's better than nothing for now.
		*/
		wxDEFAULT_FRAME_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxSTAY_ON_TOP
#else
		wxDEFAULT_FRAME_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxFRAME_FLOAT_ON_PARENT
#endif
		)

{
	_lines.resize (_num_lines);
	Bind (wxEVT_PAINT, boost::bind (&ClosedCaptionsDialog::paint, this));
}

void
ClosedCaptionsDialog::paint ()
{
	wxPaintDC dc (this);
	dc.SetBackground (*wxBLACK_BRUSH);
	dc.Clear ();
	dc.SetTextForeground (*wxWHITE);

	/* Choose a font which fits vertically */
	int const line_height = dc.GetSize().GetHeight() / _num_lines;
	wxFont font (*wxNORMAL_FONT);
	font.SetPixelSize (wxSize (0, line_height * 0.8));
	dc.SetFont (font);

	for (int i = 0; i < _num_lines; ++i) {
		wxString const good = _lines[i].Left (_num_chars_per_line);
		dc.DrawText (good, 8, line_height * i);
		if (_lines[i].Length() > _num_chars_per_line) {
			wxString const bad = _lines[i].Right (_lines[i].Length() - _num_chars_per_line);
			wxSize size = dc.GetTextExtent (good);
			dc.SetTextForeground (*wxRED);
			dc.DrawText (bad, 8 + size.GetWidth(), line_height * i);
			dc.SetTextForeground (*wxWHITE);
		}
	}
}

class ClosedCaptionSorter
{
public:
	bool operator() (StringText const & a, StringText const & b)
	{
		return from_top(a) < from_top(b);
	}

private:
	float from_top (StringText const & c) const
	{
		switch (c.v_align()) {
		case dcp::VALIGN_TOP:
			return c.v_position();
		case dcp::VALIGN_CENTER:
			return c.v_position() + 0.5;
		case dcp::VALIGN_BOTTOM:
			return 1.0 - c.v_position();
		}
		DCPOMATIC_ASSERT (false);
		return 0;
	}
};

void
ClosedCaptionsDialog::update (DCPTime time)
{
	shared_ptr<Player> player = _player.lock ();
	DCPOMATIC_ASSERT (player);
	list<StringText> to_show;
	BOOST_FOREACH (PlayerText i, player->closed_captions_for_frame(time)) {
		BOOST_FOREACH (StringText j, i.text) {
			to_show.push_back (j);
		}
	}

	for (int j = 0; j < _num_lines; ++j) {
		_lines[j] = "";
	}

	to_show.sort (ClosedCaptionSorter());

	list<StringText>::const_iterator j = to_show.begin();
	int k = 0;
	while (j != to_show.end() && k < _num_lines) {
		_lines[k] = j->text();
		++j;
		++k;
	}

	Refresh ();
}

void
ClosedCaptionsDialog::clear ()
{
	Refresh ();
}

void
ClosedCaptionsDialog::set_player (weak_ptr<Player> player)
{
	_player = player;
}
