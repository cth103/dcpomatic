/*
    Copyright (C) 2019-2020 Carl Hetherington <cth@carlh.net>

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

#include "editable_list.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <vector>


class Film;
class RatingDialog;
class ContentVersionDialog;


class SMPTEMetadataDialog : public wxDialog
{
public:
	SMPTEMetadataDialog (wxWindow* parent, boost::weak_ptr<Film> film);

private:
	std::vector<dcp::Rating> ratings () const;
	void set_ratings (std::vector<dcp::Rating> r);
	std::vector<std::string> content_versions () const;
	void set_content_versions (std::vector<std::string> v);
	void edit_name_language ();
	void edit_release_territory ();
	void version_number_changed ();
	void status_changed ();
	void chain_changed ();
	void distributor_changed ();
	void facility_changed ();
	void luminance_changed ();
	void film_changed (ChangeType type, Film::Property property);
	boost::shared_ptr<Film> film () const;

	boost::weak_ptr<Film> _film;
	wxStaticText* _name_language;
	wxStaticText* _release_territory;
	wxSpinCtrl* _version_number;
	wxChoice* _status;
	wxTextCtrl* _chain;
	wxTextCtrl* _distributor;
	wxTextCtrl* _facility;
	wxSpinCtrlDouble* _luminance_value;
	wxChoice* _luminance_unit;
	EditableList<dcp::Rating, RatingDialog>* _ratings;
	EditableList<std::string, ContentVersionDialog>* _content_versions;

	boost::signals2::scoped_connection _film_changed_connection;
};
