/*
    Copyright (C) 2015-2017 Carl Hetherington <cth@carlh.net>

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

#include "lib/config.h"
#include "lib/cinema.h"
#include "lib/cinema_kdms.h"
#include "lib/send_kdm_email_job.h"
#include "kdm_output_panel.h"
#include "kdm_timing_panel.h"
#include "confirm_kdm_email_dialog.h"
#include "wx_util.h"
#include "name_format_editor.h"
#include <dcp/exceptions.h>
#include <dcp/types.h>
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#else
#include <wx/filepicker.h>
#endif
#include <wx/stdpaths.h>

using std::pair;
using std::string;
using std::list;
using std::exception;
using std::make_pair;
using boost::shared_ptr;
using boost::function;

KDMOutputPanel::KDMOutputPanel (wxWindow* parent, bool interop)
	: wxPanel (parent, wxID_ANY)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, 0);

	add_label_to_sizer (table, this, _("KDM type"), true);
	_type = new wxChoice (this, wxID_ANY);
	_type->Append ("Modified Transitional 1", ((void *) dcp::MODIFIED_TRANSITIONAL_1));
	_type->Append ("Multiple Modified Transitional 1", ((void *) dcp::MULTIPLE_MODIFIED_TRANSITIONAL_1));
	_type->Append ("Modified Transitional 1 (without AuthorizedDeviceInfo)", ((void *) dcp::MODIFIED_TRANSITIONAL_TEST));
	if (!interop) {
		_type->Append ("DCI Any", ((void *) dcp::DCI_ANY));
		_type->Append ("DCI Specific", ((void *) dcp::DCI_SPECIFIC));
	}
	table->Add (_type, 1, wxEXPAND);
	_type->SetSelection (0);

	{
		int flags = wxALIGN_TOP | wxTOP | wxLEFT | wxRIGHT;
		wxString t = _("Folder / ZIP name format");
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		t += wxT (":");
#endif
		wxStaticText* m = new wxStaticText (this, wxID_ANY, t);
		table->Add (m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	_container_name_format = new NameFormatEditor (this, Config::instance()->kdm_container_name_format(), dcp::NameFormat::Map(), dcp::NameFormat::Map(), "");
	table->Add (_container_name_format->panel(), 1, wxEXPAND);

	{
		int flags = wxALIGN_TOP | wxTOP | wxLEFT | wxRIGHT;
		wxString t = _("Filename format");
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		t += wxT (":");
#endif
		wxStaticText* m = new wxStaticText (this, wxID_ANY, t);
		table->Add (m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	dcp::NameFormat::Map titles;
	titles['f'] = "film name";
	titles['c'] = "cinema";
	titles['s'] = "screen";
	titles['b'] = "from date/time";
	titles['e'] = "to date/time";
	dcp::NameFormat::Map ex;
	ex['f'] = "Bambi";
	ex['c'] = "Lumière";
	ex['s'] = "Screen 1";
	ex['b'] = "2012/03/15 12:30";
	ex['e'] = "2012/03/22 02:30";
	_filename_format = new NameFormatEditor (this, Config::instance()->kdm_filename_format(), titles, ex, ".xml");
	table->Add (_filename_format->panel(), 1, wxEXPAND);

	_write_to = new wxCheckBox (this, wxID_ANY, _("Write to"));
	table->Add (_write_to, 1, wxEXPAND);

#ifdef DCPOMATIC_USE_OWN_PICKER
	_folder = new DirPickerCtrl (this);
#else
	_folder = new wxDirPickerCtrl (this, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
#endif

	boost::optional<boost::filesystem::path> path = Config::instance()->default_kdm_directory ();
	if (path) {
		_folder->SetPath (std_to_wx (path->string ()));
	} else {
		_folder->SetPath (wxStandardPaths::Get().GetDocumentsDir());
	}

	table->Add (_folder, 1, wxEXPAND);

	wxSizer* write_options = new wxBoxSizer(wxVERTICAL);
	_write_flat = new wxRadioButton (this, wxID_ANY, _("Write all KDMs to the same folder"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	write_options->Add (_write_flat);
	_write_folder = new wxRadioButton (this, wxID_ANY, _("Write a folder for each cinema's KDMs"));
	write_options->Add (_write_folder);
	_write_zip = new wxRadioButton (this, wxID_ANY, _("Write a ZIP file for each cinema's KDMs"));
	write_options->Add (_write_zip);
	table->AddSpacer (0);
	table->Add (write_options);

	_email = new wxCheckBox (this, wxID_ANY, _("Send by email"));
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

	_write_to->SetValue (true);

	_write_to->Bind     (wxEVT_CHECKBOX, boost::bind (&KDMOutputPanel::setup_sensitivity, this));
	_email->Bind        (wxEVT_CHECKBOX, boost::bind (&KDMOutputPanel::setup_sensitivity, this));
	_write_flat->Bind   (wxEVT_RADIOBUTTON, boost::bind (&KDMOutputPanel::kdm_write_type_changed, this));
	_write_folder->Bind (wxEVT_RADIOBUTTON, boost::bind (&KDMOutputPanel::kdm_write_type_changed, this));
	_write_zip->Bind    (wxEVT_RADIOBUTTON, boost::bind (&KDMOutputPanel::kdm_write_type_changed, this));

	SetSizer (table);
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
	list<ScreenKDM> screen_kdms, string name, KDMTimingPanel* timing, function<bool (boost::filesystem::path)> confirm_overwrite, shared_ptr<Log> log
	)
{
	list<CinemaKDMs> const cinema_kdms = CinemaKDMs::collect (screen_kdms);

	/* Decide whether to proceed */

	bool proceed = true;

	if (_email->GetValue ()) {

		if (Config::instance()->mail_server().empty ()) {
			proceed = false;
			error_dialog (this, _("You must set up a mail server in Preferences before you can send emails."));
		}

		bool cinemas_with_no_email = false;
		BOOST_FOREACH (CinemaKDMs i, cinema_kdms) {
			if (i.cinema->emails.empty ()) {
				cinemas_with_no_email = true;
			}
		}

		if (proceed && cinemas_with_no_email && !confirm_dialog (
			    this,
			    _("You have selected some cinemas that have no configured email address.  Do you want to continue?")
			    )) {
			proceed = false;
		}

		if (proceed && Config::instance()->confirm_kdm_email ()) {
			list<string> emails;
			BOOST_FOREACH (CinemaKDMs i, cinema_kdms) {
				BOOST_FOREACH (string j, i.cinema->emails) {
					emails.push_back (j);
				}
			}

			if (!emails.empty ()) {
				ConfirmKDMEmailDialog* d = new ConfirmKDMEmailDialog (this, emails);
				if (d->ShowModal() == wxID_CANCEL) {
					proceed = false;
				}
			}
		}
	}

	if (!proceed) {
		return make_pair (shared_ptr<Job>(), 0);
	}

	Config::instance()->set_kdm_filename_format (_filename_format->get ());

	int written = 0;
	shared_ptr<Job> job;

	try {
		dcp::NameFormat::Map name_values;
		name_values['f'] = name;
		name_values['b'] = dcp::LocalTime(timing->from()).date() + " " + dcp::LocalTime(timing->from()).time_of_day(false, false);
		name_values['e'] = dcp::LocalTime(timing->until()).date() + " " + dcp::LocalTime(timing->until()).time_of_day(false, false);

		if (_write_to->GetValue()) {
			if (_write_flat->GetValue()) {
				written = ScreenKDM::write_files (
					screen_kdms,
					directory(),
					_filename_format->get(),
					name_values,
					confirm_overwrite
					);
			} else if (_write_folder->GetValue()) {
				written = CinemaKDMs::write_directories (
					CinemaKDMs::collect (screen_kdms),
					directory(),
					_container_name_format->get(),
					_filename_format->get(),
					name_values,
					confirm_overwrite
					);
			} else if (_write_zip->GetValue()) {
				written = CinemaKDMs::write_zip_files (
					CinemaKDMs::collect (screen_kdms),
					directory(),
					_container_name_format->get(),
					_filename_format->get(),
					name_values,
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
					name_values,
					name,
					log
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
	return (dcp::Formulation) reinterpret_cast<intptr_t> (_type->GetClientData (_type->GetSelection()));
}

boost::filesystem::path
KDMOutputPanel::directory () const
{
	return wx_to_std (_folder->GetPath ());
}
