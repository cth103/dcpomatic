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

#include "editable_list.h"
#include <dcp/types.h>
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <vector>

class Film;
class RatingDialog;

class MetadataDialog : public wxDialog
{
public:
	MetadataDialog (wxWindow* parent, boost::weak_ptr<Film> film);

private:
	std::vector<dcp::Rating> ratings () const;
	void set_ratings (std::vector<dcp::Rating> r);
	void content_version_changed ();

	boost::weak_ptr<Film> _film;
	EditableList<dcp::Rating, RatingDialog>* _ratings;
	wxTextCtrl* _content_version;
};
