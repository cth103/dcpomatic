/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/enum_indexed_vector.h"
#include "lib/film.h"
#include <wx/wx.h>
#include <memory>
#include <vector>


class CheckBox;
class DCPContent;
class Film;
class wxBoxSizer;
class wxGridBagSizer;


class DCPReferencingDialog : public wxDialog
{
public:
	DCPReferencingDialog(wxWindow* parent, std::shared_ptr<const Film> film);

private:
	enum class Part {
		VIDEO,
		AUDIO,
		SUBTITLES,
		CLOSED_CAPTIONS,
		COUNT
	};

	void setup();
	void checkbox_changed(std::weak_ptr<DCPContent> content, CheckBox* check_box, Part part);
	void film_changed(ChangeType type, FilmProperty property);
	void film_content_changed(ChangeType type, int property);

	std::shared_ptr<const Film> _film;

	wxGridBagSizer* _dcp_grid;
	wxBoxSizer* _overall_sizer;

	struct DCP {
		std::shared_ptr<DCPContent> content;
		EnumIndexedVector<CheckBox*, Part> check_box;
	};

	std::vector<DCP> _dcps;

	boost::signals2::scoped_connection _film_connection;
	boost::signals2::scoped_connection _film_content_connection;
};

