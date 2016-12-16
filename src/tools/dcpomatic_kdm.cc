/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "wx/config_dialog.h"
#include "wx/about_dialog.h"
#include "wx/report_problem_dialog.h"
#include "wx/file_picker_ctrl.h"
#include "wx/wx_util.h"
#include "wx/wx_signal_manager.h"
#include "wx/screens_panel.h"
#include "wx/kdm_timing_panel.h"
#include "wx/kdm_output_panel.h"
#include "wx/job_view_dialog.h"
#include "wx/file_dialog_wrapper.h"
#include "wx/editable_list.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/screen.h"
#include "lib/job_manager.h"
#include "lib/screen_kdm.h"
#include "lib/exceptions.h"
#include "lib/cinema_kdms.h"
#include "lib/send_kdm_email_job.h"
#include "lib/compose.hpp"
#include "lib/cinema.h"
#include <dcp/encrypted_kdm.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/exceptions.h>
#include <wx/wx.h>
#include <wx/preferences.h>
#include <wx/filepicker.h>
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#ifdef check
#undef check
#endif

using std::exception;
using std::list;
using std::string;
using std::vector;
using boost::shared_ptr;
using boost::bind;
using boost::optional;

enum {
	ID_help_report_a_problem = 1,
};

class KDMFileDialogWrapper : public FileDialogWrapper<dcp::EncryptedKDM>
{
public:
	KDMFileDialogWrapper (wxWindow* parent)
		: FileDialogWrapper<dcp::EncryptedKDM> (parent, _("Select DKDM file"))
	{

	}

	optional<dcp::EncryptedKDM> get ()
	{
		try {
			return dcp::EncryptedKDM (dcp::file_to_string (wx_to_std (_dialog->GetPath ()), MAX_KDM_SIZE));
		} catch (cxml::Error& e) {
			error_dialog (_parent, wxString::Format ("This file does not look like a KDM (%s)", std_to_wx (e.what()).data()));
		}

		return optional<dcp::EncryptedKDM> ();
	}
};

static string
column (dcp::EncryptedKDM k)
{
	return String::compose ("%1 (%2)", k.content_title_text(), k.cpl_id());
}

class DOMFrame : public wxFrame
{
public:
	DOMFrame (wxString const & title)
		: wxFrame (0, -1, title)
		, _config_dialog (0)
		, _job_view (0)
	{
#if defined(DCPOMATIC_WINDOWS)
		if (Config::instance()->win32_console ()) {
			AllocConsole();

			HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
			int hCrt = _open_osfhandle((intptr_t) handle_out, _O_TEXT);
			FILE* hf_out = _fdopen(hCrt, "w");
			setvbuf(hf_out, NULL, _IONBF, 1);
			*stdout = *hf_out;

			HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
			hCrt = _open_osfhandle((intptr_t) handle_in, _O_TEXT);
			FILE* hf_in = _fdopen(hCrt, "r");
			setvbuf(hf_in, NULL, _IONBF, 128);
			*stdin = *hf_in;

			std::cout << "DCP-o-matic KDM creator is starting." << "\n";
		}
#endif

		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this),             wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this),      wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this),            wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this), ID_help_report_a_problem);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);
		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);

		wxBoxSizer* horizontal = new wxBoxSizer (wxHORIZONTAL);
		wxBoxSizer* left = new wxBoxSizer (wxVERTICAL);
		wxBoxSizer* right = new wxBoxSizer (wxVERTICAL);

		horizontal->Add (left, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP * 2);
		horizontal->Add (right, 1, wxEXPAND);

		wxFont subheading_font (*wxNORMAL_FONT);
		subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

		wxStaticText* h = new wxStaticText (overall_panel, wxID_ANY, _("Screens"));
		h->SetFont (subheading_font);
		left->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);
		_screens = new ScreensPanel (overall_panel);
		left->Add (_screens, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);

		/// TRANSLATORS: translate the word "Timing" here; do not include the "KDM|" prefix
		h = new wxStaticText (overall_panel, wxID_ANY, S_("KDM|Timing"));
		h->SetFont (subheading_font);
		right->Add (h, 0, wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_Y_GAP * 2);
		_timing = new KDMTimingPanel (overall_panel);
		right->Add (_timing, 0, wxALL, DCPOMATIC_SIZER_Y_GAP);

		h = new wxStaticText (overall_panel, wxID_ANY, _("DKDM"));
		h->SetFont (subheading_font);
		right->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);

		vector<string> columns;
		columns.push_back (wx_to_std (_("CPL")));
		_dkdm = new EditableList<dcp::EncryptedKDM, KDMFileDialogWrapper> (
			overall_panel, columns, bind (&DOMFrame::dkdms, this), bind (&DOMFrame::set_dkdms, this, _1), bind (&column, _1), false
			);
		right->Add (_dkdm, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_Y_GAP);

		h = new wxStaticText (overall_panel, wxID_ANY, _("Output"));
		h->SetFont (subheading_font);
		right->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);
		/* XXX: hard-coded non-interop here */
		_output = new KDMOutputPanel (overall_panel, false);
		right->Add (_output, 0, wxALL, DCPOMATIC_SIZER_Y_GAP);

		_create = new wxButton (overall_panel, wxID_ANY, _("Create KDMs"));
		right->Add (_create, 0, wxALL, DCPOMATIC_SIZER_GAP);

		main_sizer->Add (horizontal, 1, wxALL | wxEXPAND, DCPOMATIC_DIALOG_BORDER);
		overall_panel->SetSizer (main_sizer);

		/* Instantly save any config changes when using a DCP-o-matic GUI */
		Config::instance()->Changed.connect (boost::bind (&Config::write, Config::instance ()));

		_screens->ScreensChanged.connect (boost::bind (&DOMFrame::setup_sensitivity, this));
		_create->Bind (wxEVT_BUTTON, bind (&DOMFrame::create_kdms, this));
		_dkdm->SelectionChanged.connect (boost::bind (&DOMFrame::setup_sensitivity, this));

		setup_sensitivity ();
	}

private:
	vector<dcp::EncryptedKDM> dkdms () const
	{
		return Config::instance()->dkdms ();
	}

	void set_dkdms (vector<dcp::EncryptedKDM> dkdms)
	{
		Config::instance()->set_dkdms (dkdms);
	}

	void file_exit ()
	{
		/* false here allows the close handler to veto the close request */
		Close (false);
	}

	void edit_preferences ()
	{
		if (!_config_dialog) {
			_config_dialog = create_config_dialog ();
		}
		_config_dialog->Show (this);
	}

	void help_about ()
	{
		AboutDialog* d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	void help_report_a_problem ()
	{
		ReportProblemDialog* d = new ReportProblemDialog (this, shared_ptr<Film> ());
		if (d->ShowModal () == wxID_OK) {
			d->report ();
		}
		d->Destroy ();
	}

	void setup_menu (wxMenuBar* m)
	{
		wxMenu* file = new wxMenu;

#ifdef __WXOSX__
		file->Append (wxID_EXIT, _("&Exit"));
#else
		file->Append (wxID_EXIT, _("&Quit"));
#endif

#ifdef __WXOSX__
		file->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#else
		wxMenu* edit = new wxMenu;
		edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

		wxMenu* help = new wxMenu;
#ifdef __WXOSX__
		help->Append (wxID_ABOUT, _("About DCP-o-matic"));
#else
		help->Append (wxID_ABOUT, _("About"));
#endif
		help->Append (ID_help_report_a_problem, _("Report a problem..."));

		m->Append (file, _("&File"));
#ifndef __WXOSX__
		m->Append (edit, _("&Edit"));
#endif
		m->Append (help, _("&Help"));
	}

	void create_kdms ()
	{
		try {
			if (!_dkdm->selection()) {
				return;
			}

			/* Decrypt the DKDM */
			dcp::DecryptedKDM decrypted (_dkdm->selection().get(), Config::instance()->decryption_chain()->key().get());

			/* This is the signer for our new KDMs */
			shared_ptr<const dcp::CertificateChain> signer = Config::instance()->signer_chain ();
			if (!signer->valid ()) {
				throw InvalidSignerError ();
			}

			list<ScreenKDM> screen_kdms;
			BOOST_FOREACH (shared_ptr<Screen> i, _screens->screens()) {

				if (!i->recipient) {
					continue;
				}

				/* Make an empty KDM */
				dcp::DecryptedKDM kdm (
					dcp::LocalTime (_timing->from(), i->cinema->utc_offset_hour(), i->cinema->utc_offset_minute()),
					dcp::LocalTime (_timing->until(), i->cinema->utc_offset_hour(), i->cinema->utc_offset_minute()),
					decrypted.annotation_text().get_value_or (""),
					decrypted.content_title_text(),
					dcp::LocalTime().as_string()
					);

				/* Add keys from the DKDM */
				BOOST_FOREACH (dcp::DecryptedKDMKey const & j, decrypted.keys()) {
					kdm.add_key (j);
				}

				/* Encrypt */
				screen_kdms.push_back (ScreenKDM (i, kdm.encrypt (signer, i->recipient.get(), i->trusted_devices, _output->formulation())));
			}

			dcp::NameFormat::Map name_values;
			name_values['f'] = decrypted.content_title_text();
			name_values['b'] = dcp::LocalTime(_timing->from()).date() + " " + dcp::LocalTime(_timing->from()).time_of_day();
			name_values['e'] = dcp::LocalTime(_timing->until()).date() + " " + dcp::LocalTime(_timing->until()).time_of_day();

			if (_output->write_to()) {
				ScreenKDM::write_files (screen_kdms, _output->directory(), _output->name_format(), name_values);
				/* XXX: proper plural form support in wxWidgets? */
				wxString s = screen_kdms.size() == 1 ? _("%d KDM written to %s") : _("%d KDMs written to %s");
				message_dialog (
					this,
					wxString::Format (s, int(screen_kdms.size()), std_to_wx(_output->directory().string()).data())
					);
			} else {
				string film_name = decrypted.annotation_text().get_value_or ("");
				if (film_name.empty ()) {
					film_name = decrypted.content_title_text ();
				}
				shared_ptr<Job> job (new SendKDMEmailJob (
							     CinemaKDMs::collect (screen_kdms),
							     _output->name_format(),
							     name_values,
							     decrypted.content_title_text(),
							     shared_ptr<Log> ()
							     ));

				JobManager::instance()->add (job);
				if (_job_view) {
					_job_view->Destroy ();
					_job_view = 0;
				}
				_job_view = new JobViewDialog (this, _("Send KDM emails"), job);
				_job_view->ShowModal ();
			}
		} catch (dcp::NotEncryptedError& e) {
			error_dialog (this, _("CPL's content is not encrypted."));
		} catch (exception& e) {
			error_dialog (this, e.what ());
		} catch (...) {
			error_dialog (this, _("An unknown exception occurred."));
		}
	}

	void setup_sensitivity ()
	{
		_screens->setup_sensitivity ();
		_output->setup_sensitivity ();
		_create->Enable (!_screens->screens().empty() && _dkdm->selection());
	}

	wxPreferencesEditor* _config_dialog;
	ScreensPanel* _screens;
	KDMTimingPanel* _timing;
	EditableList<dcp::EncryptedKDM, KDMFileDialogWrapper>* _dkdm;
	wxButton* _create;
	KDMOutputPanel* _output;
	JobViewDialog* _job_view;
};

/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
		, _frame (0)
	{}

private:

	bool OnInit ()
	try
	{
		wxInitAllImageHandlers ();

		SetAppName (_("DCP-o-matic KDM Creator"));

		if (!wxApp::OnInit()) {
			return false;
		}

#ifdef DCPOMATIC_LINUX
		unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef __WXOSX__
		ProcessSerialNumber serial;
		GetCurrentProcess (&serial);
		TransformProcessType (&serial, kProcessTransformToForegroundApplication);
#endif

		dcpomatic_setup_path_encoding ();

		/* Enable i18n; this will create a Config object
		   to look for a force-configured language.  This Config
		   object will be wrong, however, because dcpomatic_setup
		   hasn't yet been called and there aren't any filters etc.
		   set up yet.
		*/
		dcpomatic_setup_i18n ();

		/* Set things up, including filters etc.
		   which will now be internationalised correctly.
		*/
		dcpomatic_setup ();

		/* Force the configuration to be re-loaded correctly next
		   time it is needed.
		*/
		Config::drop ();

		_frame = new DOMFrame (_("DCP-o-matic KDM Creator"));
		SetTopWindow (_frame);
		_frame->Maximize ();
		_frame->Show ();

		signal_manager = new wxSignalManager (this);
		Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		return true;
	}
	catch (exception& e)
	{
		error_dialog (0, wxString::Format ("DCP-o-matic could not start: %s", e.what ()));
		return true;
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.file().string().c_str ())
					)
				);
		} catch (exception& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s.\n\n") + " " + REPORT_PROBLEM,
					std_to_wx (e.what ())
					)
				);
		} catch (...) {
			error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}

		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException ()
	{
		error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	DOMFrame* _frame;
};

IMPLEMENT_APP (App)
