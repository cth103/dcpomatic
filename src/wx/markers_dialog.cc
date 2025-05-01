/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "dcpomatic_button.h"
#include "film_viewer.h"
#include "markers.h"
#include "markers_dialog.h"
#include "static_text.h"
#include "timecode.h"
#include "wx_util.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/gbsizer.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>


using std::make_shared;
using std::shared_ptr;
using std::weak_ptr;
using boost::bind;
using boost::optional;
using dcpomatic::DCPTime;


class Marker
{
public:
	Marker(wxWindow* parent, wxGridBagSizer* grid, int row, weak_ptr<Film> film, FilmViewer const& viewer, wxString name, dcp::Marker type)
		: _film(film)
		, _viewer(viewer)
		, _type(type)
	{
		_checkbox = new CheckBox(parent, name);
		grid->Add(_checkbox, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
		_timecode = new Timecode<DCPTime>(parent);
		grid->Add(_timecode, wxGBPosition(row, 1));
		_set_button = new Button(parent, _("Set from current position"));
		grid->Add(_set_button, wxGBPosition(row, 2));

		auto f = _film.lock();
		DCPOMATIC_ASSERT (f);

		auto t = f->marker(_type);
		_checkbox->SetValue(static_cast<bool>(t));
		if (t) {
			_timecode->set(*t, f->video_frame_rate());
		}

		setup_sensitivity();

		_set_button->Bind(wxEVT_BUTTON, bind(&Marker::set, this));
		_checkbox->bind(&Marker::checkbox_clicked, this);
		_timecode->Changed.connect(bind(&Marker::changed, this));
	}

private:
	void checkbox_clicked ()
	{
		setup_sensitivity();
		changed ();
	}

	void setup_sensitivity()
	{
		_timecode->Enable(_checkbox->GetValue());
		_set_button->Enable(_checkbox->GetValue());
	}

	void set ()
	{
		auto f = _film.lock();
		DCPOMATIC_ASSERT (f);
		_timecode->set(_viewer.position(), f->video_frame_rate());
		changed ();
	}

	void changed ()
	{
		auto f = _film.lock();
		DCPOMATIC_ASSERT (f);
		auto vfr = f->video_frame_rate();
		auto tc = _timecode->get(vfr);
		if (tc >= f->length()) {
			tc = f->length() - DCPTime::from_frames(1, vfr);
			_timecode->set(tc, vfr);
		}
		if (_checkbox->GetValue()) {
			f->set_marker(_type, tc);
		} else {
			f->unset_marker(_type);
		}
	}

	weak_ptr<Film> _film;
	FilmViewer const& _viewer;
	dcp::Marker _type;
	CheckBox* _checkbox;
	Timecode<dcpomatic::DCPTime>* _timecode;
	Button* _set_button;
};


MarkersDialog::MarkersDialog(wxWindow* parent, weak_ptr<Film> film, FilmViewer const& viewer)
	: wxDialog (parent, wxID_ANY, _("Markers"))
	, _film (film)
{
	auto sizer = new wxBoxSizer (wxVERTICAL);
	auto grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	int r = 0;
	for (auto const& marker: all_editable_markers()) {
		_markers.push_back (make_shared<Marker>(this, grid, r++, film, viewer, marker.first, marker.second));
	}

	sizer->Add (grid, 0, wxALL, 8);

	auto buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (sizer);
}
