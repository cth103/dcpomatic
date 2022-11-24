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


#include "language_subtag_panel.h"
#include "region_subtag_dialog.h"
#include <wx/wx.h>


using boost::optional;


RegionSubtagDialog::RegionSubtagDialog(wxWindow* parent, dcp::LanguageTag::RegionSubtag region)
	: wxDialog(parent, wxID_ANY, _("Region"), wxDefaultPosition, wxSize(-1, 500))
	, _panel(new LanguageSubtagPanel (this))
{
	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add (_panel, 1);

	auto buttons = CreateSeparatedButtonSizer(wxOK);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer(sizer);

	_panel->set(dcp::LanguageTag::SubtagType::REGION, "", *dcp::LanguageTag::get_subtag_data(region));
}


optional<dcp::LanguageTag::RegionSubtag>
RegionSubtagDialog::get () const
{
	return _panel->get();
}


