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


#include "invalid_certificate_period_dialog.h"
#include "wx_util.h"
#include "lib/kdm_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/listctrl.h>
#include <wx/scrolwin.h>
LIBDCP_ENABLE_WARNINGS


InvalidCertificatePeriodDialog::InvalidCertificatePeriodDialog(wxWindow* parent, std::vector<KDMCertificatePeriod> const& periods)
	: wxDialog(parent, wxID_ANY, _("Invalid certificates"))
	, _list(new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT))
{
	{
		wxListItem ip;
		ip.SetId(0);
		ip.SetText(_("Cinema"));
		ip.SetWidth(200);
		_list->InsertColumn(0, ip);
	}

	{
		wxListItem ip;
		ip.SetId(1);
		ip.SetText(_("Screen"));
		ip.SetWidth(50);
		_list->InsertColumn(1, ip);
	}

	{
		wxListItem ip;
		ip.SetId(2);
		ip.SetText(_("Certificate start"));
		ip.SetWidth(200);
		_list->InsertColumn(2, ip);
	}

	{
		wxListItem ip;
		ip.SetId(3);
		ip.SetText(_("Certificate end"));
		ip.SetWidth(200);
		_list->InsertColumn(3, ip);
	}

	int id = 0;
	for (auto const& period: periods) {
		wxListItem item;
		item.SetId(id);
		_list->InsertItem(item);
		_list->SetItem(0, 0, std_to_wx(period.cinema_name));
		_list->SetItem(0, 1, std_to_wx(period.screen_name));
		_list->SetItem(0, 2, std_to_wx(period.from.as_string()));
		_list->SetItem(0, 3, std_to_wx(period.to.as_string()));
	}

	auto overall_sizer = new wxBoxSizer(wxVERTICAL);

	auto constexpr width = 700;

	auto question = new wxStaticText(this, wxID_ANY, _("Some KDMs would have validity periods which are outside the recipient certificate validity periods.  What do you want to do?"));
	question->Wrap(width);
	overall_sizer->Add(
		question,
		0,
		wxALL,
		DCPOMATIC_DIALOG_BORDER
		);

	_list->SetSize({width, -1});
	overall_sizer->Add(_list, 1, wxALL | wxEXPAND, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateStdDialogButtonSizer(0);
	if (buttons) {
		overall_sizer->Add(CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
		buttons->SetAffirmativeButton(new wxButton(this, wxID_OK, _("Create KDMs anyway")));
		buttons->SetCancelButton(new wxButton(this, wxID_CANCEL, _("Cancel")));
		buttons->Realize();
	}

	overall_sizer->Layout();
	SetSizerAndFit(overall_sizer);
}

