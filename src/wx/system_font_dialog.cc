/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "system_font_dialog.h"
#include "wx_util.h"
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/listctrl.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


using std::string;
using boost::optional;


SystemFontDialog::SystemFontDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Choose a font"))
{
	auto sizer = new wxBoxSizer(wxVERTICAL);

	boost::filesystem::path fonts = "c:\\Windows\\Fonts";
	auto windir = getenv("windir");
	if (windir) {
		fonts = boost::filesystem::path(windir) / "Fonts";
	}

	for (auto i: dcp::filesystem::directory_iterator(fonts)) {
		auto ext = i.path().extension().string();
		transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".ttf") {
			_fonts.push_back(i.path());
		}
	}

	sort(_fonts.begin(), _fonts.end());

	_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_list->InsertColumn(0, wxString{});
	_list->SetColumnWidth(0, 512);
	sizer->Add(_list, 0, wxALL, DCPOMATIC_SIZER_X_GAP);

	int n = 0;
	for (auto i: _fonts) {
		_list->InsertItem(n++, std_to_wx(i.filename().stem().string()));
	}

	auto buttons = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit(sizer);

	_list->Bind(wxEVT_LIST_ITEM_SELECTED, boost::bind(&SystemFontDialog::setup_sensitivity, this));
	_list->Bind(wxEVT_LIST_ITEM_DESELECTED, boost::bind(&SystemFontDialog::setup_sensitivity, this));

	setup_sensitivity();
}


optional<boost::filesystem::path>
SystemFontDialog::get_font() const
{
	int const s = _list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (s == -1) {
		return {};
	}

	if (s < int(_fonts.size())) {
		return _fonts[s];
	}

	return {};
}


void
SystemFontDialog::setup_sensitivity()
{
	auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	if (ok) {
		ok->Enable(_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1);
	}
}
