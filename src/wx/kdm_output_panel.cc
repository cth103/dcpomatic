/*
    Copyright (C) 2015-2022 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "confirm_kdm_email_dialog.h"
#include "dcpomatic_button.h"
#include "dcpomatic_choice.h"
#include "extra_kdm_email_dialog.h"
#include "kdm_advanced_dialog.h"
#include "kdm_choice.h"
#include "kdm_output_panel.h"
#include "kdm_timing_panel.h"
#include "name_format_editor.h"
#include "wx_util.h"
#include "lib/cinema.h"
#include "lib/config.h"
#include "lib/send_kdm_email_job.h"
#include <dcp/exceptions.h>
#include <dcp/types.h>
#include <dcp/warnings.h>
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#else
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
LIBDCP_ENABLE_WARNINGS
#endif
LIBDCP_DISABLE_WARNINGS
#include <wx/stdpaths.h>
LIBDCP_ENABLE_WARNINGS


using std::exception;
using std::function;
using std::list;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


KDMOutputPanel::KDMOutputPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
	, _forensic_mark_video (true)
	, _forensic_mark_audio (true)
	, _forensic_mark_audio_up_to (12)
{


}


void
KDMOutputPanel::create_destination_widgets(wxWindow* parent)
{
	_write_to = new CheckBox(parent, _("Write to"));

#ifdef DCPOMATIC_USE_OWN_PICKER
	_folder = new DirPickerCtrl(parent);
#else
	_folder = new wxDirPickerCtrl(parent, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize(300, -1));
#endif

	auto path = Config::instance()->default_kdm_directory();
	if (path) {
		_folder->SetPath(std_to_wx(path->string()));
	} else {
		_folder->SetPath(wxStandardPaths::Get().GetDocumentsDir());
	}

	_write_collect = new Choice(parent);
	_write_collect->add_entry(_("Write all KDMs to the same folder"));
	_write_collect->add_entry(_("Write a folder for each cinema's KDMs"));
	_write_collect->add_entry(_("Write a ZIP file for each cinema's KDMs"));

	_email = new CheckBox(parent, _("Send by email"));
	_add_email_addresses = new wxButton(parent, wxID_ANY, _("Set additional email addresses..."));

	switch (Config::instance()->last_kdm_write_type().get_value_or(Config::KDM_WRITE_FLAT)) {
	case Config::KDM_WRITE_FLAT:
		_write_collect->set(0);
		break;
	case Config::KDM_WRITE_FOLDER:
		_write_collect->set(1);
		break;
	case Config::KDM_WRITE_ZIP:
		_write_collect->set(2);
		break;
	}

	_write_to->SetValue(Config::instance()->write_kdms_to_disk());
	_email->SetValue(Config::instance()->email_kdms());

	_write_to->bind(&KDMOutputPanel::write_to_changed, this);
	_email->bind(&KDMOutputPanel::email_changed, this);
	_add_email_addresses->Bind(wxEVT_BUTTON, boost::bind(&KDMOutputPanel::add_email_addresses_clicked, this));
	_write_collect->bind(&KDMOutputPanel::kdm_write_type_changed, this);
}


void
KDMOutputPanel::create_details_widgets(wxWindow* parent)
{
	_type = new KDMChoice(parent);
	_type->set(Config::instance()->default_kdm_type());
	_advanced = new Button(parent, _("Advanced..."));
	_annotation_text = new wxTextCtrl(parent, wxID_ANY);

	_advanced->Bind(wxEVT_BUTTON, boost::bind (&KDMOutputPanel::advanced_clicked, this));
}


void
KDMOutputPanel::create_name_format_widgets(wxWindow* parent, bool detailed)
{
	_container_name_format = new NameFormatEditor(parent, Config::instance()->kdm_container_name_format(), dcp::NameFormat::Map(), dcp::NameFormat::Map(), "");

	dcp::NameFormat::Map titles;
	if (detailed) {
		titles['f'] = wx_to_std (_("film name"));
		titles['c'] = wx_to_std (_("cinema"));
		titles['s'] = wx_to_std (_("screen"));
		titles['b'] = wx_to_std (_("from date/time"));
		titles['e'] = wx_to_std (_("to date/time"));
	}

	dcp::NameFormat::Map ex;
	if (detailed) {
		ex['f'] = "Bambi";
		ex['c'] = "Lumière";
		ex['s'] = "Screen 1";
		ex['b'] = "2012/03/15 12:30";
		ex['e'] = "2012/03/22 02:30";
	}

	_filename_format = new NameFormatEditor(parent, Config::instance()->kdm_filename_format(), titles, ex, ".xml");
}


void
KDMOutputPanel::write_to_changed ()
{
	Config::instance()->set_write_kdms_to_disk(_write_to->GetValue());
	setup_sensitivity ();
	MethodChanged();
}


void
KDMOutputPanel::email_changed ()
{
	Config::instance()->set_email_kdms(_email->GetValue());
	setup_sensitivity ();
	MethodChanged();
}


void
KDMOutputPanel::setup_sensitivity ()
{
	bool const write = _write_to->GetValue ();
	_folder->Enable (write);
	_write_collect->Enable(write);
}


void
KDMOutputPanel::advanced_clicked ()
{
	KDMAdvancedDialog dialog(this, _forensic_mark_video, _forensic_mark_audio, _forensic_mark_audio_up_to);
	dialog.ShowModal();
	_forensic_mark_video = dialog.forensic_mark_video();
	_forensic_mark_audio = dialog.forensic_mark_audio();
	_forensic_mark_audio_up_to = dialog.forensic_mark_audio_up_to();
}


void
KDMOutputPanel::kdm_write_type_changed ()
{
	switch (_write_collect->get().get_value_or(0)) {
	case 0:
		Config::instance()->set_last_kdm_write_type(Config::KDM_WRITE_FLAT);
		break;
	case 1:
		Config::instance()->set_last_kdm_write_type(Config::KDM_WRITE_FOLDER);
		break;
	case 2:
		Config::instance()->set_last_kdm_write_type(Config::KDM_WRITE_ZIP);
		break;
	}
}


pair<shared_ptr<Job>, int>
KDMOutputPanel::make (
	list<KDMWithMetadataPtr> kdms, string name, function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	auto const cinema_kdms = collect (kdms);

	/* Decide whether to proceed */

	bool proceed = true;

	if (_email->GetValue ()) {

		if (Config::instance()->mail_server().empty ()) {
			proceed = false;
			error_dialog (this, _("You must set up a mail server in Preferences before you can send emails."));
		}

		bool const cinemas_with_no_email = std::any_of(
			cinema_kdms.begin(), cinema_kdms.end(),
			[](list<KDMWithMetadataPtr> const& list) { return list.front()->emails().empty(); }
			);

		if (proceed && cinemas_with_no_email && !confirm_dialog (
			    this,
			    _("You have selected some cinemas that have no configured email address.  Do you want to continue?")
			    )) {
			proceed = false;
		}

		if (proceed && Config::instance()->confirm_kdm_email ()) {
			list<string> emails;
			for (auto const& i: cinema_kdms) {
				for (auto j: i.front()->emails()) {
					emails.push_back (j);
				}
			}

			if (!emails.empty ()) {
				auto d = new ConfirmKDMEmailDialog (this, emails);
				if (d->ShowModal() == wxID_CANCEL) {
					proceed = false;
				}
			}
		}
	}

	if (!proceed) {
		return {};
	}

	Config::instance()->set_kdm_filename_format (_filename_format->get ());

	int written = 0;
	shared_ptr<Job> job;

	try {

		if (_write_to->GetValue()) {
			switch (_write_collect->get().get_value_or(0)) {
			case 0:
				written = write_files (
					kdms,
					directory(),
					_filename_format->get(),
					confirm_overwrite
					);
				break;
			case 1:
				written = write_directories (
					collect (kdms),
					directory(),
					_container_name_format->get(),
					_filename_format->get(),
					confirm_overwrite
					);
				break;
			case 2:
				written = write_zip_files (
					collect (kdms),
					directory(),
					_container_name_format->get(),
					_filename_format->get(),
					confirm_overwrite
					);
				break;
			}
		}

		if (_email->GetValue ()) {
			job = make_shared<SendKDMEmailJob>(
				cinema_kdms,
				_container_name_format->get(),
				_filename_format->get(),
				name,
				_extra_addresses
				);
		}

	} catch (dcp::NotEncryptedError& e) {
		error_dialog (this, _("CPL's content is not encrypted."));
	} catch (exception& e) {
		error_dialog (this, std_to_wx(e.what()));
	} catch (...) {
		error_dialog (this, _("An unknown exception occurred."));
	}

	return make_pair (job, written);
}


dcp::Formulation
KDMOutputPanel::formulation () const
{
	return _type->get();
}


boost::filesystem::path
KDMOutputPanel::directory () const
{
	return wx_to_std (_folder->GetPath ());
}


void
KDMOutputPanel::add_email_addresses_clicked ()
{
	ExtraKDMEmailDialog dialog(this, _extra_addresses);
	if (dialog.ShowModal() == wxID_OK) {
		_extra_addresses = dialog.get();
	}
}


bool
KDMOutputPanel::method_selected() const
{
	return _write_to->GetValue() || _email->GetValue();
}


void
KDMOutputPanel::set_annotation_text(string text)
{
	checked_set(_annotation_text, std::move(text));
}


string
KDMOutputPanel::annotation_text() const
{
	return wx_to_std(_annotation_text->GetValue());
}

