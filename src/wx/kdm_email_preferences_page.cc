/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "email_dialog.h"
#include "kdm_email_preferences_page.h"
#include "wx_util.h"


using std::string;
using std::vector;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::preferences;


KDMEmailPage::KDMEmailPage(wxSize panel_size, int border)
#ifdef DCPOMATIC_OSX
	/* We have to force both width and height of this one */
	: Page(wxSize(panel_size.GetWidth(), 128), border)
#else
	: Page(panel_size, border)
#endif
{

}


wxString
KDMEmailPage::GetName() const
{
	return _("KDM Email");
}


#ifdef DCPOMATIC_OSX
wxBitmap
KDMEmailPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("kdm_email"), wxBITMAP_TYPE_PNG);
}
#endif


void
KDMEmailPage::setup()
{
	auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol(1, 1);
	_panel->GetSizer()->Add(table, 0, wxEXPAND | wxALL, _border);

	add_label_to_sizer(table, _panel, _("Subject"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_subject = new wxTextCtrl(_panel, wxID_ANY);
	table->Add(_subject, 1, wxEXPAND | wxALL);

	add_label_to_sizer(table, _panel, _("From address"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_from = new wxTextCtrl(_panel, wxID_ANY);
	table->Add(_from, 1, wxEXPAND | wxALL);

	vector<EditableListColumn> columns;
	columns.push_back(EditableListColumn(_("Address")));
	add_label_to_sizer(table, _panel, _("CC addresses"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_cc = new EditableList<string, EmailDialog>(
		_panel,
		columns,
		bind(&Config::kdm_cc, Config::instance()),
		bind(&Config::set_kdm_cc, Config::instance(), _1),
		[] (string s, int) {
			return s;
		},
		EditableListTitle::VISIBLE,
		EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
		);
	table->Add(_cc, 1, wxEXPAND | wxALL);

	add_label_to_sizer(table, _panel, _("BCC address"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_bcc = new wxTextCtrl(_panel, wxID_ANY);
	table->Add(_bcc, 1, wxEXPAND | wxALL);

	_email = new wxTextCtrl(_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 200), wxTE_MULTILINE);
	_panel->GetSizer()->Add(_email, 0, wxEXPAND | wxALL, _border);

	_reset_email = new Button(_panel, _("Reset to default subject and text"));
	_panel->GetSizer()->Add(_reset_email, 0, wxEXPAND | wxALL, _border);

	_cc->layout();

	_subject->Bind(wxEVT_TEXT, boost::bind(&KDMEmailPage::kdm_subject_changed, this));
	_from->Bind(wxEVT_TEXT, boost::bind(&KDMEmailPage::kdm_from_changed, this));
	_bcc->Bind(wxEVT_TEXT, boost::bind(&KDMEmailPage::kdm_bcc_changed, this));
	_email->Bind(wxEVT_TEXT, boost::bind(&KDMEmailPage::kdm_email_changed, this));
	_reset_email->Bind(wxEVT_BUTTON, boost::bind(&KDMEmailPage::reset_email, this));
}


void
KDMEmailPage::config_changed()
{
	auto config = Config::instance();

	checked_set(_subject, config->kdm_subject());
	checked_set(_from, config->kdm_from());
	checked_set(_bcc, config->kdm_bcc());
	checked_set(_email, Config::instance()->kdm_email());
}


void
KDMEmailPage::kdm_subject_changed()
{
	Config::instance()->set_kdm_subject(wx_to_std(_subject->GetValue()));
}


void
KDMEmailPage::kdm_from_changed()
{
	Config::instance()->set_kdm_from(wx_to_std(_from->GetValue()));
}


void
KDMEmailPage::kdm_bcc_changed()
{
	Config::instance()->set_kdm_bcc(wx_to_std(_bcc->GetValue()));
}


void
KDMEmailPage::kdm_email_changed()
{
	if (_email->GetValue().IsEmpty()) {
		/* Sometimes we get sent an erroneous notification that the email
		   is empty; I don't know why.
		*/
		return;
	}
	Config::instance()->set_kdm_email(wx_to_std(_email->GetValue()));
}


void
KDMEmailPage::reset_email()
{
	Config::instance()->reset_kdm_email();
	checked_set(_email, Config::instance()->kdm_email());
}

