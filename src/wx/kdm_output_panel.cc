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
	auto table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, 0);
	table->AddGrowableCol (1);

	add_label_to_sizer (table, this, _("KDM type"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);

	auto type = new wxBoxSizer (wxHORIZONTAL);
	_type = new KDMChoice (this);
	type->Add (_type, 1, wxTOP, DCPOMATIC_CHOICE_TOP_PAD);
	_type->set(Config::instance()->default_kdm_type());
	auto advanced = new Button (this, _("Advanced..."));
	type->Add (advanced, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	table->Add (type, 1, wxTOP, DCPOMATIC_CHOICE_TOP_PAD);

	add_label_to_sizer (table, this, _("Folder / ZIP name format"), true, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT);
	_container_name_format = new NameFormatEditor (this, Config::instance()->kdm_container_name_format(), dcp::NameFormat::Map(), dcp::NameFormat::Map(), "");
	table->Add (_container_name_format->panel(), 1, wxEXPAND);

	auto format = create_label (this, _("Filename format"), true);
	auto align = new wxBoxSizer (wxHORIZONTAL);
#ifdef DCPOMATIC_OSX
	align->Add (format, 0, wxTOP, 2);
	table->Add (align, 0, wxALIGN_RIGHT | wxRIGHT, DCPOMATIC_SIZER_GAP - 2);
#else
	align->Add (format, 0, wxLEFT, DCPOMATIC_SIZER_GAP - 2);
	table->Add (align, 0, wxTOP | wxRIGHT | wxALIGN_TOP, DCPOMATIC_SIZER_GAP);
#endif
	dcp::NameFormat::Map titles;
	titles['f'] = wx_to_std (_("film name"));
	titles['c'] = wx_to_std (_("cinema"));
	titles['s'] = wx_to_std (_("screen"));
	titles['b'] = wx_to_std (_("from date/time"));
	titles['e'] = wx_to_std (_("to date/time"));
	dcp::NameFormat::Map ex;
	ex['f'] = "Bambi";
	ex['c'] = "Lumière";
	ex['s'] = "Screen 1";
	ex['b'] = "2012/03/15 12:30";
	ex['e'] = "2012/03/22 02:30";
	_filename_format = new NameFormatEditor (this, Config::instance()->kdm_filename_format(), titles, ex, ".xml");
	table->Add (_filename_format->panel(), 1, wxEXPAND);

	_write_to = new CheckBox (this, _("Write to"));
	table->Add (_write_to, 1, wxEXPAND);

#ifdef DCPOMATIC_USE_OWN_PICKER
	_folder = new DirPickerCtrl (this);
#else
	_folder = new wxDirPickerCtrl (this, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
#endif

	auto path = Config::instance()->default_kdm_directory();
	if (path) {
		_folder->SetPath (std_to_wx (path->string ()));
	} else {
		_folder->SetPath (wxStandardPaths::Get().GetDocumentsDir());
	}

	table->Add (_folder, 1, wxEXPAND);

	auto write_options = new wxBoxSizer(wxVERTICAL);
	_write_flat = new wxRadioButton (this, wxID_ANY, _("Write all KDMs to the same folder"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	write_options->Add (_write_flat, 1, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_write_folder = new wxRadioButton (this, wxID_ANY, _("Write a folder for each cinema's KDMs"));
	write_options->Add (_write_folder, 1, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_write_zip = new wxRadioButton (this, wxID_ANY, _("Write a ZIP file for each cinema's KDMs"));
	write_options->Add (_write_zip, 1, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	table->AddSpacer (0);
	table->Add (write_options);

	_email = new CheckBox (this, _("Send by email"));
	table->Add (_email, 1, wxEXPAND);
	table->AddSpacer (0);

	switch (Config::instance()->last_kdm_write_type().get_value_or(Config::KDM_WRITE_FLAT)) {
	case Config::KDM_WRITE_FLAT:
		_write_flat->SetValue (true);
		break;
	case Config::KDM_WRITE_FOLDER:
		_write_folder->SetValue (true);
		break;
	case Config::KDM_WRITE_ZIP:
		_write_zip->SetValue (true);
		break;
	}

	_write_to->SetValue (Config::instance()->write_kdms_to_disk());
	_email->SetValue (Config::instance()->email_kdms());

	_write_to->Bind     (wxEVT_CHECKBOX, boost::bind (&KDMOutputPanel::write_to_changed, this));
	_email->Bind        (wxEVT_CHECKBOX, boost::bind (&KDMOutputPanel::email_changed, this));
	_write_flat->Bind   (wxEVT_RADIOBUTTON, boost::bind (&KDMOutputPanel::kdm_write_type_changed, this));
	_write_folder->Bind (wxEVT_RADIOBUTTON, boost::bind (&KDMOutputPanel::kdm_write_type_changed, this));
	_write_zip->Bind    (wxEVT_RADIOBUTTON, boost::bind (&KDMOutputPanel::kdm_write_type_changed, this));
	advanced->Bind      (wxEVT_BUTTON, boost::bind (&KDMOutputPanel::advanced_clicked, this));

	SetSizer (table);
}


void
KDMOutputPanel::write_to_changed ()
{
	Config::instance()->set_write_kdms_to_disk(_write_to->GetValue());
	setup_sensitivity ();
}


void
KDMOutputPanel::email_changed ()
{
	Config::instance()->set_email_kdms(_email->GetValue());
	setup_sensitivity ();
}


void
KDMOutputPanel::setup_sensitivity ()
{
	bool const write = _write_to->GetValue ();
	_folder->Enable (write);
	_write_flat->Enable (write);
	_write_folder->Enable (write);
	_write_zip->Enable (write);
}


void
KDMOutputPanel::advanced_clicked ()
{
	auto d = new KDMAdvancedDialog (this, _forensic_mark_video, _forensic_mark_audio, _forensic_mark_audio_up_to);
	d->ShowModal ();
	_forensic_mark_video = d->forensic_mark_video ();
	_forensic_mark_audio = d->forensic_mark_audio ();
	_forensic_mark_audio_up_to = d->forensic_mark_audio_up_to ();
	d->Destroy ();
}


void
KDMOutputPanel::kdm_write_type_changed ()
{
	if (_write_flat->GetValue()) {
		Config::instance()->set_last_kdm_write_type (Config::KDM_WRITE_FLAT);
	} else if (_write_folder->GetValue()) {
		Config::instance()->set_last_kdm_write_type (Config::KDM_WRITE_FOLDER);
	} else if (_write_zip->GetValue()) {
		Config::instance()->set_last_kdm_write_type (Config::KDM_WRITE_ZIP);
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
			if (_write_flat->GetValue()) {
				written = write_files (
					kdms,
					directory(),
					_filename_format->get(),
					confirm_overwrite
					);
			} else if (_write_folder->GetValue()) {
				written = write_directories (
					collect (kdms),
					directory(),
					_container_name_format->get(),
					_filename_format->get(),
					confirm_overwrite
					);
			} else if (_write_zip->GetValue()) {
				written = write_zip_files (
					collect (kdms),
					directory(),
					_container_name_format->get(),
					_filename_format->get(),
					confirm_overwrite
					);
			}
		}

		if (_email->GetValue ()) {
			job.reset (
				new SendKDMEmailJob (
					cinema_kdms,
					_container_name_format->get(),
					_filename_format->get(),
					name
					)
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
