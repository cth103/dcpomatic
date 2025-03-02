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


#include "dcpomatic_button.h"
#include "email_preferences_page.h"
#include "password_entry.h"
#include "send_test_email_dialog.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/email.h"
#include "lib/variant.h"
#include <wx/spinctrl.h>


using namespace dcpomatic::preferences;


EmailPage::EmailPage(wxSize panel_size, int border)
	: Page(panel_size, border)
{

}


wxString
EmailPage::GetName() const
{
	return _("Email");
}


#ifdef DCPOMATIC_OSX
wxBitmap
EmailPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("email"), wxBITMAP_TYPE_PNG);
}
#endif


void
EmailPage::setup()
{
	auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol(1, 1);
	_panel->GetSizer()->Add(table, 1, wxEXPAND | wxALL, _border);

	add_label_to_sizer(table, _panel, _("Outgoing mail server"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	{
		wxBoxSizer* s = new wxBoxSizer(wxHORIZONTAL);
		_server = new wxTextCtrl(_panel, wxID_ANY);
		s->Add(_server, 1, wxEXPAND | wxALL);
		add_label_to_sizer(s, _panel, _("port"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_port = new wxSpinCtrl(_panel, wxID_ANY);
		_port->SetRange(0, 65535);
		s->Add(_port);
		add_label_to_sizer(s, _panel, _("protocol"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_protocol = new wxChoice(_panel, wxID_ANY);
		/* Make sure this matches the switches in config_changed and port_changed below */
		_protocol->Append(_("Auto"));
		_protocol->Append(_("Plain"));
		_protocol->Append(_("STARTTLS"));
		_protocol->Append(_("SSL"));
		s->Add(_protocol, 1, wxALIGN_CENTER_VERTICAL);
		table->Add(s, 1, wxEXPAND | wxALL);
	}

	add_label_to_sizer(table, _panel, _("User name"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_user = new wxTextCtrl(_panel, wxID_ANY);
	table->Add(_user, 1, wxEXPAND | wxALL);

	add_label_to_sizer(table, _panel, _("Password"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_password = new PasswordEntry(_panel);
	table->Add(_password->get_panel(), 1, wxEXPAND | wxALL);

	table->AddSpacer(0);
	_send_test_email = new Button(_panel, _("Send test email..."));
	table->Add(_send_test_email);

	_server->Bind(wxEVT_TEXT, boost::bind(&EmailPage::server_changed, this));
	_port->Bind(wxEVT_SPINCTRL, boost::bind(&EmailPage::port_changed, this));
	_protocol->Bind(wxEVT_CHOICE, boost::bind(&EmailPage::protocol_changed, this));
	_user->Bind(wxEVT_TEXT, boost::bind(&EmailPage::user_changed, this));
	_password->Changed.connect(boost::bind(&EmailPage::password_changed, this));
	_send_test_email->Bind(wxEVT_BUTTON, boost::bind(&EmailPage::send_test_email_clicked, this));
}


void
EmailPage::config_changed()
{
	auto config = Config::instance();

	checked_set(_server, config->mail_server());
	checked_set(_port, config->mail_port());
	switch(config->mail_protocol()) {
	case EmailProtocol::AUTO:
		checked_set(_protocol, 0);
		break;
	case EmailProtocol::PLAIN:
		checked_set(_protocol, 1);
		break;
	case EmailProtocol::STARTTLS:
		checked_set(_protocol, 2);
		break;
	case EmailProtocol::SSL:
		checked_set(_protocol, 3);
		break;
	}
	checked_set(_user, config->mail_user());
	checked_set(_password, config->mail_password());
}


void
EmailPage::server_changed()
{
	Config::instance()->set_mail_server(wx_to_std(_server->GetValue()));
}


void
EmailPage::port_changed()
{
	Config::instance()->set_mail_port(_port->GetValue());
}


void
EmailPage::protocol_changed()
{
	switch (_protocol->GetSelection()) {
	case 0:
		Config::instance()->set_mail_protocol(EmailProtocol::AUTO);
		break;
	case 1:
		Config::instance()->set_mail_protocol(EmailProtocol::PLAIN);
		break;
	case 2:
		Config::instance()->set_mail_protocol(EmailProtocol::STARTTLS);
		break;
	case 3:
		Config::instance()->set_mail_protocol(EmailProtocol::SSL);
		break;
	}
}

void
EmailPage::user_changed()
{
	Config::instance()->set_mail_user(wx_to_std(_user->GetValue()));
}


void
EmailPage::password_changed()
{
	Config::instance()->set_mail_password(_password->get());
}


void
EmailPage::send_test_email_clicked()
{
	SendTestEmailDialog dialog(_panel);
	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	Email email(
		wx_to_std(dialog.from()),
		{ wx_to_std(dialog.to()) },
		wx_to_std(variant::wx::insert_dcpomatic(_("%s test email"))),
		wx_to_std(variant::wx::insert_dcpomatic(_("This is a test email from %s.")))
		);
	auto config = Config::instance();
	try {
		email.send(config->mail_server(), config->mail_port(), config->mail_protocol(), config->mail_user(), config->mail_password());
	} catch (NetworkError& e) {
		error_dialog(_panel, std_to_wx(e.summary()), std_to_wx(e.detail().get_value_or("")));
		return;
	} catch (std::exception& e) {
		error_dialog(_panel, _("Test email sending failed."), std_to_wx(e.what()));
		return;
	} catch (...) {
		error_dialog(_panel, _("Test email sending failed."));
		return;
	}
	message_dialog(_panel, _("Test email sent."));
}
