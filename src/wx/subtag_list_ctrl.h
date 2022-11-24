/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include <dcp/language_tag.h>
#include <wx/listctrl.h>
#include <vector>


class SubtagListCtrl : public wxListCtrl
{
public:
	SubtagListCtrl(wxWindow* parent);

	void set(dcp::LanguageTag::SubtagType type, std::string search, boost::optional<dcp::LanguageTag::SubtagData> subtag = boost::optional<dcp::LanguageTag::SubtagData>());
	void set_search(std::string search);
	boost::optional<dcp::LanguageTag::SubtagData> selected_subtag() const;

private:
	wxString OnGetItemText(long item, long column) const override;

	std::vector<dcp::LanguageTag::SubtagData> _all_subtags;
	std::vector<dcp::LanguageTag::SubtagData> _matching_subtags;
};




