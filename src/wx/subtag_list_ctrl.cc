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


#include "subtag_list_ctrl.h"
#include "lib/dcpomatic_assert.h"
#include <wx/wx.h>
#include <boost/algorithm/string.hpp>


using std::string;
using boost::optional;


SubtagListCtrl::SubtagListCtrl(wxWindow* parent)
	: wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER | wxLC_VIRTUAL)
{
	AppendColumn("", wxLIST_FORMAT_LEFT, 80);
	AppendColumn("", wxLIST_FORMAT_LEFT, 400);
}


void
SubtagListCtrl::set(dcp::LanguageTag::SubtagType type, string search, optional<dcp::LanguageTag::SubtagData> subtag)
{
	_all_subtags = dcp::LanguageTag::get_all(type);
	set_search(search);
	if (subtag) {
		auto i = find(_matching_subtags.begin(), _matching_subtags.end(), *subtag);
		if (i != _matching_subtags.end()) {
			auto item = std::distance(_matching_subtags.begin(), i);
			SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			EnsureVisible(item);
		}
	} else {
		if (GetItemCount() > 0) {
			/* The new list sometimes isn't visible without this */
			EnsureVisible(0);
		}
	}
}


void
SubtagListCtrl::set_search(string search)
{
	if (search == "") {
		_matching_subtags = _all_subtags;
	} else {
		_matching_subtags.clear();

		boost::algorithm::to_lower(search);
		for (auto const& i: _all_subtags) {
			if (
				(boost::algorithm::to_lower_copy(i.subtag).find(search) != string::npos) ||
				(boost::algorithm::to_lower_copy(i.description).find(search) != string::npos)) {
				_matching_subtags.push_back (i);
			}
		}
	}

	SetItemCount(_matching_subtags.size());
	if (GetItemCount() > 0) {
		RefreshItems(0, GetItemCount() - 1);
	}
}


optional<dcp::LanguageTag::SubtagData>
SubtagListCtrl::selected_subtag() const
{
	auto selected = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected == -1) {
		return {};
	}

	DCPOMATIC_ASSERT(static_cast<size_t>(selected) < _matching_subtags.size());
	return _matching_subtags[selected];
}


wxString
SubtagListCtrl::OnGetItemText(long item, long column) const
{
	if (column == 0) {
		return _matching_subtags[item].subtag;
	} else {
		return _matching_subtags[item].description;
	}
}

