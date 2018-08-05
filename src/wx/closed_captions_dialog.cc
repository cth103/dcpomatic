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
#include "lib/butler.h"
#include <boost/bind.hpp>

using std::list;
using std::max;
using std::cout;
using std::pair;
using std::make_pair;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::optional;

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
	, _current_in_lines (false)
{
	_lines.resize (CLOSED_CAPTION_LINES);
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
	int const line_height = max (8, dc.GetSize().GetHeight() / CLOSED_CAPTION_LINES);
	wxFont font (*wxNORMAL_FONT);
	font.SetPixelSize (wxSize (0, line_height * 0.8));
	dc.SetFont (font);

	for (int i = 0; i < CLOSED_CAPTION_LINES; ++i) {
		wxString const good = _lines[i].Left (CLOSED_CAPTION_LENGTH);
		dc.DrawText (good, 8, line_height * i);
		if (_lines[i].Length() > CLOSED_CAPTION_LENGTH) {
			wxString const bad = _lines[i].Right (_lines[i].Length() - CLOSED_CAPTION_LENGTH);
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
	if (_current_in_lines && _current->second.to > time) {
		/* Current one is fine */
		return;
	}

	if (_current && _current->second.to < time) {
		/* Current one has finished; clear out */
		for (int j = 0; j < CLOSED_CAPTION_LINES; ++j) {
			_lines[j] = "";
		}
		Refresh ();
		_current = optional<pair<PlayerText, DCPTimePeriod> >();
	}

	if (!_current) {
		/* We have no current one: get another */
		shared_ptr<Butler> butler = _butler.lock ();
		DCPOMATIC_ASSERT (butler);
		_current = butler->get_closed_caption ();
		_current_in_lines = false;
	}

	if (_current && _current->second.contains(time)) {
		/* We need to set this new one up */

		list<StringText> to_show = _current->first.string;

		for (int j = 0; j < CLOSED_CAPTION_LINES; ++j) {
			_lines[j] = "";
		}

		to_show.sort (ClosedCaptionSorter());

		list<StringText>::const_iterator j = to_show.begin();
		int k = 0;
		while (j != to_show.end() && k < CLOSED_CAPTION_LINES) {
			_lines[k] = j->text();
			++j;
			++k;
		}

		Refresh ();
		_current_in_lines = true;
	}
}

void
ClosedCaptionsDialog::clear ()
{
	_current = optional<pair<PlayerText, DCPTimePeriod> >();
	_current_in_lines = false;
	Refresh ();
}

void
ClosedCaptionsDialog::set_butler (weak_ptr<Butler> butler)
{
	_butler = butler;
}
