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

#include "closed_captions_dialog.h"
#include "wx_util.h"
#include "film_viewer.h"
#include "lib/string_text.h"
#include "lib/butler.h"
#include "lib/text_content.h"
#include "lib/compose.hpp"
#include <boost/bind/bind.hpp>

using std::list;
using std::max;
using std::cout;
using std::pair;
using std::make_pair;
using std::shared_ptr;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;

ClosedCaptionsDialog::ClosedCaptionsDialog (wxWindow* parent, FilmViewer* viewer)
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
	, _viewer (viewer)
	  /* XXX: empirical and probably unhelpful default size here; needs to be related to font metrics */
        , _display (new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(640, (640 / 10) + 64)))
	, _track (new wxChoice(this, wxID_ANY))
	, _current_in_lines (false)
	, _timer (this)
{
	_lines.resize (MAX_CLOSED_CAPTION_LINES);

	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	wxBoxSizer* track_sizer = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (track_sizer, this, _("Track"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	track_sizer->Add (_track, 0, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_X_GAP);

	sizer->Add (track_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);
	sizer->Add (_display, 1, wxEXPAND);

	Bind (wxEVT_SHOW, boost::bind(&ClosedCaptionsDialog::shown, this, _1));
	Bind (wxEVT_TIMER, boost::bind(&ClosedCaptionsDialog::update, this));
	_display->Bind (wxEVT_PAINT, boost::bind(&ClosedCaptionsDialog::paint, this));
	_track->Bind (wxEVT_CHOICE, boost::bind(&ClosedCaptionsDialog::track_selected, this));

	SetSizerAndFit (sizer);
}

void
ClosedCaptionsDialog::shown (wxShowEvent ev)
{
	if (ev.IsShown ()) {
		_timer.Start (40);
	} else {
		_timer.Stop ();
	}
}

void
ClosedCaptionsDialog::track_selected ()
{
	_current = optional<TextRingBuffers::Data> ();
	_viewer->slow_refresh ();
	update ();
}

void
ClosedCaptionsDialog::paint ()
{
	wxPaintDC dc (_display);
	dc.SetBackground (*wxBLACK_BRUSH);
	dc.Clear ();
	dc.SetTextForeground (*wxWHITE);

	/* Choose a font which fits vertically */
	int const line_height = max (8, dc.GetSize().GetHeight() / MAX_CLOSED_CAPTION_LINES);
	wxFont font (*wxNORMAL_FONT);
	font.SetPixelSize (wxSize (0, line_height * 0.8));
	dc.SetFont (font);

	for (int i = 0; i < MAX_CLOSED_CAPTION_LINES; ++i) {
		wxString const good = _lines[i].Left (MAX_CLOSED_CAPTION_LENGTH);
		dc.DrawText (good, 8, line_height * i);
		if (_lines[i].Length() > MAX_CLOSED_CAPTION_LENGTH) {
			wxString const bad = _lines[i].Right (_lines[i].Length() - MAX_CLOSED_CAPTION_LENGTH);
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
		case dcp::VAlign::TOP:
			return c.v_position();
		case dcp::VAlign::CENTER:
			return c.v_position() + 0.5;
		case dcp::VAlign::BOTTOM:
			return 1.0 - c.v_position();
		}
		DCPOMATIC_ASSERT (false);
		return 0;
	}
};

void
ClosedCaptionsDialog::update ()
{
	DCPTime const time = _viewer->time ();

	if (_current_in_lines && _current && _current->period.to > time) {
		/* Current one is fine */
		return;
	}

	if (_current && _current->period.to < time) {
		/* Current one has finished; clear out */
		for (int j = 0; j < MAX_CLOSED_CAPTION_LINES; ++j) {
			_lines[j] = "";
		}
		Refresh ();
		_current = optional<TextRingBuffers::Data>();
	}

	if (!_current && !_tracks.empty()) {
		/* We have no current one: get another */
		shared_ptr<Butler> butler = _butler.lock ();
		DCPOMATIC_ASSERT (_track->GetSelection() >= 0);
		DCPOMATIC_ASSERT (_track->GetSelection() < int(_tracks.size()));
		DCPTextTrack track = _tracks[_track->GetSelection()];
		if (butler) {
			while (true) {
				optional<TextRingBuffers::Data> d = butler->get_closed_caption ();
				if (!d) {
					break;
				}
				if (d->track == track) {
					_current = d;
					break;
				}
			}

			_current_in_lines = false;
		}
	}

	if (_current && _current->period.contains(time)) {
		/* We need to set this new one up */

		list<StringText> to_show = _current->text.string;

		for (int j = 0; j < MAX_CLOSED_CAPTION_LINES; ++j) {
			_lines[j] = "";
		}

		to_show.sort (ClosedCaptionSorter());

		list<StringText>::const_iterator j = to_show.begin();
		int k = 0;
		while (j != to_show.end() && k < MAX_CLOSED_CAPTION_LINES) {
			_lines[k] = j->text();
			++j;
			++k;
		}

		Refresh ();
		_current_in_lines = true;
	}

	if (!_current && _tracks.empty()) {
		for (int i = 0; i < MAX_CLOSED_CAPTION_LINES; ++i) {
			_lines[i] = wxString();
		}
	}
}

void
ClosedCaptionsDialog::clear ()
{
	_current = optional<TextRingBuffers::Data>();
	_current_in_lines = false;
	Refresh ();
}


void
ClosedCaptionsDialog::set_butler (weak_ptr<Butler> butler)
{
	_butler = butler;
}

void
ClosedCaptionsDialog::update_tracks (shared_ptr<const Film> film)
{
	_tracks.clear ();

	for (auto i: film->content()) {
		for (auto j: i->text) {
			if (j->use() && j->type() == TEXT_CLOSED_CAPTION && j->dcp_track()) {
				if (find(_tracks.begin(), _tracks.end(), j->dcp_track()) == _tracks.end()) {
					_tracks.push_back (*j->dcp_track());
				}
			}
		}
	}

	_track->Clear ();
	for (auto const& i: _tracks) {
		_track->Append (std_to_wx(String::compose("%1 (%2)", i.name, i.language)));
	}

	if (_track->GetCount() > 0) {
		_track->SetSelection (0);
	}

	track_selected ();
}
