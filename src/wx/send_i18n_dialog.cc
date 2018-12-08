/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "send_i18n_dialog.h"
#include "wx_util.h"
#include "i18n_hook.h"
#include <wx/listctrl.h>

using std::string;
using std::map;

SendI18NDialog::SendI18NDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Send translations"))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, _("Your name"), true);
	_name = new wxTextCtrl (this, wxID_ANY);
	table->Add (_name, 0, wxEXPAND);

	add_label_to_sizer (table, this, _("Your email"), true);
	_email = new wxTextCtrl (this, wxID_ANY);
	table->Add (_email, 0, wxEXPAND);

	add_label_to_sizer (table, this, _("Language"), true);
	_language = new wxTextCtrl (this, wxID_ANY);
	table->Add (_language, 0, wxEXPAND);

	wxListCtrl* list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize(800, -1), wxLC_REPORT | wxLC_NO_HEADER);
	list->AppendColumn(wxT(""), wxLIST_FORMAT_LEFT, 400);
	list->AppendColumn(wxT(""), wxLIST_FORMAT_LEFT, 400);

	map<string, string> translations = I18NHook::translations ();
	int N = 0;
	for (map<string, string>::const_iterator i = translations.begin(); i != translations.end(); ++i) {
		wxListItem it;
		it.SetId(N);
		it.SetColumn(0);
		it.SetText(std_to_wx(i->first));
		list->InsertItem(it);
		it.SetColumn(1);
		it.SetText(std_to_wx(i->second));
		list->SetItem(it);
		++N;
	}

	overall_sizer->Add (table, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);
	overall_sizer->Add (list, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);
}
