/*
    Copyright (C) 2015-2020 Carl Hetherington <cth@carlh.net>

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
#include "dkdm_output_panel.h"
#include "kdm_timing_panel.h"
#include "name_format_editor.h"
#include "wx_util.h"
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


DKDMOutputPanel::DKDMOutputPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, 0);
	table->AddGrowableCol (1);

	add_label_to_sizer (table, this, _("Filename format"), true, 0, wxALIGN_TOP | wxTOP | wxLEFT | wxRIGHT);
	dcp::NameFormat::Map titles;
	titles['f'] = wx_to_std (_("film name"));
	titles['b'] = wx_to_std (_("from date/time"));
	titles['e'] = wx_to_std (_("to date/time"));
	dcp::NameFormat::Map ex;
	ex['f'] = "Bambi";
	ex['b'] = "2012/03/15 12:30";
	ex['e'] = "2012/03/22 02:30";
	_filename_format = new NameFormatEditor (this, Config::instance()->dkdm_filename_format(), titles, ex, ".xml");
	table->Add (_filename_format->panel(), 1, wxEXPAND);

	_write_to = new CheckBox (this, _("Write to"));
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

	_email = new CheckBox (this, _("Send by email"));
	table->Add (_email, 1, wxEXPAND);
	table->AddSpacer (0);

	_write_to->Bind (wxEVT_CHECKBOX, boost::bind(&DKDMOutputPanel::setup_sensitivity, this));
	_email->Bind (wxEVT_CHECKBOX, boost::bind(&DKDMOutputPanel::setup_sensitivity, this));

	SetSizer (table);
}


void
DKDMOutputPanel::setup_sensitivity ()
{
	_folder->Enable (_write_to->GetValue());
}


pair<shared_ptr<Job>, int>
DKDMOutputPanel::make (
	list<KDMWithMetadataPtr> kdms, string name, function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	/* Decide whether to proceed */

	bool proceed = true;

	if (_email->GetValue()) {

		if (Config::instance()->mail_server().empty()) {
			proceed = false;
			error_dialog (this, _("You must set up a mail server in Preferences before you can send emails."));
		}

		bool kdms_with_no_email = false;
		for (auto i: kdms) {
			if (i->emails().empty()) {
				kdms_with_no_email = true;
			}
		}

		if (proceed && kdms_with_no_email && !confirm_dialog (
			    this,
			    _("You have selected some cinemas that have no configured email address.  Do you want to continue?")
			    )) {
			proceed = false;
		}

		if (proceed && Config::instance()->confirm_kdm_email()) {
			list<string> emails;
			for (auto const& i: kdms) {
				for (auto j: i->emails()) {
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

	Config::instance()->set_dkdm_filename_format (_filename_format->get());

	int written = 0;
	shared_ptr<Job> job;

	try {
		written = write_files (
			kdms,
			directory(),
			_filename_format->get(),
			confirm_overwrite
			);

		if (_email->GetValue ()) {
			job.reset (
				new SendKDMEmailJob (
					kdms,
					_filename_format->get(),
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


boost::filesystem::path
DKDMOutputPanel::directory () const
{
	return wx_to_std (_folder->GetPath ());
}
