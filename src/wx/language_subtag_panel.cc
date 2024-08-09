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


#include "language_subtag_panel.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/srchctrl.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using std::string;
using boost::optional;


LanguageSubtagPanel::LanguageSubtagPanel(wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
#ifdef __WXGTK3__
	int const height = 30;
#else
	int const height = -1;
#endif

	_search = new wxSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200, height));
	_list = new SubtagListCtrl(this);

	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(_search, 0, wxALL, 8);
	sizer->Add(_list, 1, wxALL, 8);
	SetSizer(sizer);

	_search->Bind(wxEVT_TEXT, boost::bind(&LanguageSubtagPanel::search_changed, this));
	_list->Bind(wxEVT_LIST_ITEM_SELECTED, boost::bind(&LanguageSubtagPanel::selection_changed, this));
	_list->Bind(wxEVT_LIST_ITEM_DESELECTED, boost::bind(&LanguageSubtagPanel::selection_changed, this));
}


void
LanguageSubtagPanel::set(dcp::LanguageTag::SubtagType type, string search, optional<dcp::LanguageTag::SubtagData> subtag)
{
	_list->set(type, search, subtag);
	_search->SetValue(std_to_wx(search));
}


optional<dcp::LanguageTag::RegionSubtag>
LanguageSubtagPanel::get() const
{
	if (!_list->selected_subtag()) {
		return {};
	}

	return dcp::LanguageTag::RegionSubtag(_list->selected_subtag()->subtag);
}


void
LanguageSubtagPanel::search_changed()
{
	auto search = _search->GetValue();
	_list->set_search(wx_to_std(search));
	if (search.Length() > 0 && _list->GetItemCount() > 0) {
		_list->EnsureVisible (0);
	}
	SearchChanged(wx_to_std(_search->GetValue()));
}


void
LanguageSubtagPanel::selection_changed()
{
	SelectionChanged(_list->selected_subtag());
}

