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


#include "dcpomatic_button.h"
#include "full_language_tag_dialog.h"
#include "region_subtag_dialog.h"
#include "region_subtag_widget.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using boost::optional;


RegionSubtagWidget::RegionSubtagWidget(wxWindow* parent, wxString tooltip, optional<dcp::LanguageTag::RegionSubtag> tag, optional<wxString> size_to_fit)
	: _parent(parent)
	, _sizer(new wxBoxSizer(wxHORIZONTAL))
{
	_region = new wxStaticText(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
	_region->SetToolTip(tooltip);
	set(tag);

	if (size_to_fit) {
		int w;
		int h;
		_region->GetTextExtent(*size_to_fit, &w, &h);
		_region->SetMinSize(wxSize(w, -1));
	}

	_sizer->Add(_region, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	_edit = new Button(parent, _("Edit..."));
	_sizer->Add(_edit, 0, wxLEFT, DCPOMATIC_SIZER_GAP);

	_edit->Bind(wxEVT_BUTTON, boost::bind(&RegionSubtagWidget::edit, this));
}


void
RegionSubtagWidget::edit()
{
	RegionSubtagDialog dialog(_parent, _tag.get_value_or(dcp::LanguageTag::RegionSubtag("US")));

	if (dialog.ShowModal() == wxID_OK) {
		set(dialog.get());
		Changed(dialog.get());
	}
}


void
RegionSubtagWidget::set(optional<dcp::LanguageTag::RegionSubtag> tag)
{
	_tag = tag;
	if (tag) {
		checked_set(_region, std_to_wx(tag->subtag()));
	} else {
		checked_set(_region, std::string{});
	}
}


void
RegionSubtagWidget::enable(bool e)
{
	_region->Enable(e);
	_edit->Enable(e);
}
