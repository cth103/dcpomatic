/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_METADATA_DIALOG_H
#define DCPOMATIC_METADATA_DIALOG_H


#include "lib/change_signaller.h"
#include "lib/film.h"
#include "lib/warnings.h"
#include "lib/weak_film.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS


class Button;


class MetadataDialog : public wxDialog, public WeakFilm
{
public:
	MetadataDialog (wxWindow* parent, std::weak_ptr<Film> film);

	virtual void setup ();

protected:
	virtual void setup_standard (wxPanel*, wxSizer*);
	virtual void setup_advanced (wxPanel*, wxSizer*);
	virtual void film_changed (ChangeType type, Film::Property property);
	virtual void setup_sensitivity ();

private:
	void edit_release_territory ();
	void enable_release_territory_changed ();
	void facility_changed ();
	void enable_facility_changed ();
	void studio_changed ();
	void enable_studio_changed ();
	void temp_version_changed ();
	void pre_release_changed ();
	void red_band_changed ();
	void two_d_version_of_three_d_changed ();

	wxCheckBox* _enable_release_territory;
	/** The current release territory displayed in the UI; since we can't easily convert
	 *  the string in _release_territory_text to a RegionSubtag we just store the RegionSubtag
	 *  alongside.
	 */
	boost::optional<dcp::LanguageTag::RegionSubtag> _release_territory;
	wxStaticText* _release_territory_text;
	Button* _edit_release_territory;
	wxCheckBox* _enable_facility;
	wxTextCtrl* _facility;
	wxCheckBox* _enable_studio;
	wxTextCtrl* _studio;
	wxCheckBox* _temp_version;
	wxCheckBox* _pre_release;
	wxCheckBox* _red_band;
	wxCheckBox* _two_d_version_of_three_d;

	boost::signals2::scoped_connection _film_changed_connection;
};


#endif

