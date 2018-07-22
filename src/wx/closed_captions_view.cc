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

#include "closed_captions_view.h"
#include <boost/bind.hpp>

using std::list;
using std::cout;
using std::make_pair;

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
		if (_lines[i].IsEmpty()) {
			dc.DrawText (wxString::Format("Line %d", i + 1), 8, line_height * i);
		} else {
			dc.DrawText (_lines[i], 8, line_height * i);
		}
	}
}

class ClosedCaptionSorter
{
public:
	bool operator() (TextCaption const & a, TextCaption const & b)
	{
		return from_top(a) < from_top(b);
	}

private:
	float from_top (TextCaption const & c) const
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
ClosedCaptionsDialog::refresh (DCPTime time)
{
	list<TextCaption> to_show;
	list<Caption>::iterator i = _captions.begin ();
	while (i != _captions.end ()) {
		if (time > i->second.to) {
			list<Caption>::iterator tmp = i;
			++i;
			_captions.erase (tmp);
		} else if (i->second.contains (time)) {
			BOOST_FOREACH (TextCaption j, i->first.text) {
				to_show.push_back (j);
			}
			++i;
		} else {
			++i;
		}
	}

	for (int j = 0; j < _num_lines; ++j) {
		_lines[j] = "";
	}

	to_show.sort (ClosedCaptionSorter());

	list<TextCaption>::const_iterator j = to_show.begin();
	int k = 0;
	while (j != to_show.end() && k < _num_lines) {
		_lines[k] = j->text();
		++j;
		++k;
	}

	Refresh ();
}

void
ClosedCaptionsDialog::caption (PlayerCaption caption, DCPTimePeriod period)
{
	_captions.push_back (make_pair (caption, period));
}

void
ClosedCaptionsDialog::clear ()
{
	_captions.clear ();
	Refresh ();
}
