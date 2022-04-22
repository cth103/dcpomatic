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


#include "editable_list.h"
#include "full_language_tag_dialog.h"
#include "metadata_dialog.h"
#include "lib/film.h"
#include "lib/weak_film.h"
#include <dcp/language_tag.h>
#include <dcp/types.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <vector>


class ContentVersionDialog;
class Film;
class LanguageTagDialog;
class LanguageTagWidget;
class RatingDialog;


class SMPTEMetadataDialog : public MetadataDialog
{
public:
	SMPTEMetadataDialog (wxWindow* parent, std::weak_ptr<Film> film);

	void setup () override;

private:
	void setup_standard (wxPanel* parent, wxSizer* sizer) override;
	void setup_advanced (wxPanel* parent, wxSizer* sizer) override;
	void film_changed (ChangeType type, Film::Property property) override;
	void setup_sensitivity () override;

	std::vector<dcp::Rating> ratings () const;
	void set_ratings (std::vector<dcp::Rating> r);
	std::vector<std::string> content_versions () const;
	void set_content_versions (std::vector<std::string> v);
	void name_language_changed (dcp::LanguageTag tag);
	void version_number_changed ();
	void status_changed ();
	void distributor_changed ();
	void enable_distributor_changed ();

	LanguageTagWidget* _name_language;
	wxSpinCtrl* _version_number;
	wxChoice* _status;
	wxCheckBox* _enable_distributor;
	wxTextCtrl* _distributor;
	EditableList<dcp::Rating, RatingDialog>* _ratings;
	EditableList<std::string, ContentVersionDialog>* _content_versions;
};
