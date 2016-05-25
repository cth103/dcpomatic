/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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
#include <wx/listctrl.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using std::string;
using boost::optional;

SystemFontDialog::SystemFontDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Choose a font"))
{
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	boost::filesystem::path fonts = "c:\\Windows\\Fonts";
	char* windir = getenv ("windir");
	if (windir) {
		fonts = boost::filesystem::path (windir) / "Fonts";
	}

	for (
		boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (fonts);
		i != boost::filesystem::directory_iterator ();
		++i
		) {

		string ext = i->path().extension().string ();
		transform (ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".ttf") {
			_fonts.push_back (i->path());
		}
	}

	sort (_fonts.begin (), _fonts.end ());

	_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_list->InsertColumn (0, wxT (""));
	_list->SetColumnWidth (0, 512);
	sizer->Add (_list, 0, wxALL, DCPOMATIC_SIZER_X_GAP);

	int n = 0;
	BOOST_FOREACH (boost::filesystem::path i, _fonts) {
		_list->InsertItem (n++, std_to_wx (i.leaf().stem().string ()));
	}

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (sizer);

	_list->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&SystemFontDialog::setup_sensitivity, this));
	_list->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&SystemFontDialog::setup_sensitivity, this));

	setup_sensitivity ();
}

optional<boost::filesystem::path>
SystemFontDialog::get_font () const
{
	int const s = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (s == -1) {
		return optional<boost::filesystem::path> ();
	}

	if (s < int (_fonts.size ())) {
		return _fonts[s];
	}

	return optional<boost::filesystem::path> ();
}

void
SystemFontDialog::setup_sensitivity ()
{
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (_list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1);
	}
}
