/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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
#include <dcp/language_tag.h>
#include <dcp/types.h>
#include <wx/wx.h>
#include <vector>

class Film;
class LanguageTagWidget;
class RatingDialog;


class InteropMetadataDialog : public wxDialog
{
public:
	InteropMetadataDialog (wxWindow* parent, std::weak_ptr<Film> film);

private:
	std::vector<dcp::Rating> ratings () const;
	void set_ratings (std::vector<dcp::Rating> r);
	void content_version_changed ();
	void setup_sensitivity ();
	void subtitle_language_changed (dcp::LanguageTag tag);

	std::weak_ptr<Film> _film;
	wxCheckBox* _enable_subtitle_language;
	LanguageTagWidget* _subtitle_language;
	EditableList<dcp::Rating, RatingDialog>* _ratings;
	wxTextCtrl* _content_version;
};
