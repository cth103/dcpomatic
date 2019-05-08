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

#include "markers_dialog.h"
#include "wx_util.h"
#include "timecode.h"
#include "static_text.h"
#include "dcpomatic_button.h"
#include "check_box.h"
#include "film_viewer.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <wx/gbsizer.h>
#include <boost/bind.hpp>
#include <iostream>

using std::cout;
using boost::bind;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::optional;
using dcpomatic::DCPTime;

class Marker
{
public:
	Marker (wxWindow* parent, wxGridBagSizer* grid, int row, weak_ptr<Film> film_, weak_ptr<FilmViewer> viewer_, wxString name, dcp::Marker type_)
		: film (film_)
		, viewer (viewer_)
		, type (type_)
	{
		checkbox = new CheckBox(parent, name);
		grid->Add (checkbox, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
		timecode = new Timecode<DCPTime> (parent);
		grid->Add (timecode, wxGBPosition(row, 1));
		set_button = new Button (parent, _("Set from current position"));
		grid->Add (set_button, wxGBPosition(row, 2));

		shared_ptr<Film> f = film.lock ();
		DCPOMATIC_ASSERT (f);

		optional<DCPTime> t = f->marker (type);
		checkbox->SetValue (static_cast<bool>(t));
		if (t) {
			timecode->set (*t, f->video_frame_rate());
		}

		set_sensitivity ();

		set_button->Bind (wxEVT_BUTTON, bind(&Marker::set, this));
		checkbox->Bind (wxEVT_CHECKBOX, bind(&Marker::set_sensitivity, this));
		timecode->Changed.connect (bind(&Marker::changed, this));
	}

private:
	void set_sensitivity ()
	{
		timecode->Enable (checkbox->GetValue());
		set_button->Enable (checkbox->GetValue());
	}

	void set ()
	{
		shared_ptr<Film> f = film.lock ();
		DCPOMATIC_ASSERT (f);
		shared_ptr<FilmViewer> v = viewer.lock ();
		DCPOMATIC_ASSERT (v);
		timecode->set (v->position(), f->video_frame_rate());
		changed ();
	}

	void changed ()
	{
		shared_ptr<Film> f = film.lock ();
		DCPOMATIC_ASSERT (f);
		if (checkbox->GetValue()) {
			f->set_marker (type, timecode->get(f->video_frame_rate()));
		} else {
			f->unset_marker (type);
		}
	}

	weak_ptr<Film> film;
	weak_ptr<FilmViewer> viewer;
	dcp::Marker type;
	CheckBox* checkbox;
	Timecode<dcpomatic::DCPTime>* timecode;
	Button* set_button;
};

MarkersDialog::MarkersDialog (wxWindow* parent, weak_ptr<Film> film, weak_ptr<FilmViewer> viewer)
	: wxDialog (parent, wxID_ANY, _("Markers"))
	, _film (film)
{
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	int r = 0;
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("First frame of composition"), dcp::FFOC)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("Last frame of composition"), dcp::LFOC)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("First frame of title credits"), dcp::FFTC)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("Last frame of title credits"), dcp::LFTC)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("First frame of intermission"), dcp::FFOI)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("Last frame of intermission"), dcp::LFOI)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("First frame of end credits"), dcp::FFEC)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("Last frame of end credits"), dcp::LFEC)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("First frame of moving credits"), dcp::FFMC)));
	_markers.push_back (shared_ptr<Marker>(new Marker(this, grid, r++, film, viewer, _("Last frame of moving credits"), dcp::LFMC)));

	sizer->Add (grid, 0, wxALL, 8);
	SetSizerAndFit (sizer);
}
