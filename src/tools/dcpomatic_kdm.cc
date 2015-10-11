/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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
#include "lib/config.h"
#include "lib/util.h"
#include "lib/screen.h"
#include "lib/job_manager.h"
#include "lib/screen_kdm.h"
#include "lib/exceptions.h"
#include "lib/cinema_kdms.h"
#include "lib/send_kdm_email_job.h"
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
using boost::shared_ptr;
using boost::bind;

enum {
	ID_help_report_a_problem = 1,
};

class DOMFrame : public wxFrame
{
public:
	DOMFrame (wxString const & title)
		: wxFrame (NULL, -1, title)
		, _config_dialog (0)
		, _job_view (0)
	{
		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&DOMFrame::file_exit, this),             wxID_EXIT);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&DOMFrame::edit_preferences, this),      wxID_PREFERENCES);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&DOMFrame::help_about, this),            wxID_ABOUT);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&DOMFrame::help_report_a_problem, this), ID_help_report_a_problem);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);
		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);

		wxBoxSizer* vertical = new wxBoxSizer (wxVERTICAL);

		wxFont subheading_font (*wxNORMAL_FONT);
		subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

		wxStaticText* h = new wxStaticText (overall_panel, wxID_ANY, _("Screens"));
		h->SetFont (subheading_font);
		vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL);
		_screens = new ScreensPanel (overall_panel);
		vertical->Add (_screens, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_Y_GAP);

		h = new wxStaticText (overall_panel, wxID_ANY, S_("KDM|Timing"));
		h->SetFont (subheading_font);
		vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);
		_timing = new KDMTimingPanel (overall_panel);
		vertical->Add (_timing, 0, wxALL, DCPOMATIC_SIZER_Y_GAP);

		h = new wxStaticText (overall_panel, wxID_ANY, _("DKDM"));
		h->SetFont (subheading_font);
		vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);
		wxSizer* dkdm = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		add_label_to_sizer (dkdm, overall_panel, _("DKDM file"), true);
		_dkdm = new FilePickerCtrl (overall_panel, _("Select a DKDM XML file..."), "*.xml");
		dkdm->Add (_dkdm, 1, wxEXPAND);
		add_label_to_sizer (dkdm, overall_panel, _("Annotation"), true);
		_annotation_text = new wxStaticText (overall_panel, wxID_ANY, wxT(""));
		dkdm->Add (_annotation_text, 1, wxEXPAND);
		add_label_to_sizer (dkdm, overall_panel, _("Content title"), true);
		_content_title_text = new wxStaticText (overall_panel, wxID_ANY, wxT(""));
		dkdm->Add (_content_title_text, 1, wxEXPAND);
		add_label_to_sizer (dkdm, overall_panel, _("Issue date"), true);
		_issue_date = new wxStaticText (overall_panel, wxID_ANY, wxT(""));
		dkdm->Add (_issue_date, 1, wxEXPAND);
		vertical->Add (dkdm, 0, wxALL, DCPOMATIC_SIZER_X_GAP);

		h = new wxStaticText (overall_panel, wxID_ANY, _("Output"));
		h->SetFont (subheading_font);
		vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);
		/* XXX: hard-coded non-interop here */
		_output = new KDMOutputPanel (overall_panel, false);
		vertical->Add (_output, 0, wxALL, DCPOMATIC_SIZER_Y_GAP);

		_create = new wxButton (overall_panel, wxID_ANY, _("Create KDMs"));
		vertical->Add (_create, 0, wxALL, DCPOMATIC_SIZER_GAP);

		main_sizer->Add (vertical, 1, wxALL, DCPOMATIC_DIALOG_BORDER);
		overall_panel->SetSizer (main_sizer);

		/* Instantly save any config changes when using a DCP-o-matic GUI */
		Config::instance()->Changed.connect (boost::bind (&Config::write, Config::instance ()));

		_screens->ScreensChanged.connect (boost::bind (&DOMFrame::setup_sensitivity, this));
		_dkdm->Bind (wxEVT_COMMAND_FILEPICKER_CHANGED, bind (&DOMFrame::dkdm_changed, this));
		_create->Bind (wxEVT_COMMAND_BUTTON_CLICKED, bind (&DOMFrame::create_kdms, this));

		setup_sensitivity ();
	}

private:
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

	void dkdm_changed ()
	{
		if (_dkdm->GetPath().IsEmpty()) {
			return;
		}

		try {
			dcp::EncryptedKDM encrypted (dcp::file_to_string (wx_to_std (_dkdm->GetPath())));
			dcp::DecryptedKDM decrypted (encrypted, Config::instance()->decryption_chain()->key().get());
			_annotation_text->Enable (true);
			_annotation_text->SetLabel (std_to_wx (decrypted.annotation_text ()));
			_content_title_text->Enable (true);
			_content_title_text->SetLabel (std_to_wx (decrypted.content_title_text ()));
			_issue_date->Enable (true);
			_issue_date->SetLabel (std_to_wx (decrypted.issue_date ()));
		} catch (exception& e) {
			error_dialog (this, wxString::Format (_("Could not load DKDM (%s)"), std_to_wx (e.what()).data()));
			_dkdm->SetPath (wxT(""));
			_annotation_text->SetLabel (wxT(""));
			_annotation_text->Enable (false);
			_content_title_text->SetLabel (wxT(""));
			_content_title_text->Enable (false);
			_issue_date->SetLabel (wxT(""));
			_issue_date->Enable (false);
		}

		setup_sensitivity ();
	}

	void create_kdms ()
	{
		try {
			/* Decrypt the DKDM */
			dcp::EncryptedKDM encrypted (dcp::file_to_string (wx_to_std (_dkdm->GetPath())));
			dcp::DecryptedKDM decrypted (encrypted, Config::instance()->decryption_chain()->key().get());

			/* This is the signer for our new KDMs */
			shared_ptr<const dcp::CertificateChain> signer = Config::instance()->signer_chain ();
			if (!signer->valid ()) {
				throw InvalidSignerError ();
			}

			list<ScreenKDM> screen_kdms;
			BOOST_FOREACH (shared_ptr<Screen> i, _screens->screens()) {

				if (!i->certificate) {
					continue;
				}

				/* Make an empty KDM */
				dcp::DecryptedKDM kdm (
					_timing->from(), _timing->until(), decrypted.annotation_text(), decrypted.content_title_text(), dcp::LocalTime().as_string()
					);

				/* Add keys from the DKDM */
				BOOST_FOREACH (dcp::DecryptedKDMKey const & j, decrypted.keys()) {
					kdm.add_key (j);
				}

				/* Encrypt */
				screen_kdms.push_back (ScreenKDM (i, kdm.encrypt (signer, i->certificate.get(), _output->formulation())));
			}

			if (_output->write_to()) {
				ScreenKDM::write_files (decrypted.content_title_text(), screen_kdms, _output->directory());
				/* XXX: proper plural form support in wxWidgets? */
				wxString s = screen_kdms.size() == 1 ? _("%d KDM written to %s") : _("%d KDMs written to %s");
				message_dialog (
					this,
					wxString::Format (s, int(screen_kdms.size()), std_to_wx(_output->directory().string()).data())
					);
			} else {
				string film_name = decrypted.annotation_text ();
				if (film_name.empty ()) {
					film_name = decrypted.content_title_text ();
				}
				shared_ptr<Job> job (new SendKDMEmailJob (
							     film_name,
							     decrypted.content_title_text(),
							     _timing->from(), _timing->until(),
							     CinemaKDMs::collect (screen_kdms),
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
		_create->Enable (!_screens->screens().empty() && !_dkdm->GetPath().IsEmpty());
	}

	wxPreferencesEditor* _config_dialog;
	ScreensPanel* _screens;
	KDMTimingPanel* _timing;
	/* I can't seem to clear the value in a wxFilePickerCtrl, so use our own */
	FilePickerCtrl* _dkdm;
	wxStaticText* _annotation_text;
	wxStaticText* _content_title_text;
	wxStaticText* _issue_date;
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

		SetAppName (_("DCP-o-matic KDM creator"));

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

		_frame = new DOMFrame (_("DCP-o-matic KDM creator"));
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
