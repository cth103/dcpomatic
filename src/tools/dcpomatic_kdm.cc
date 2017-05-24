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
#include "wx/new_dkdm_folder_dialog.h"
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
#include "lib/dkdm_wrapper.h"
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
using std::pair;
using std::map;
using boost::shared_ptr;
using boost::bind;
using boost::optional;
using boost::ref;
using boost::dynamic_pointer_cast;

enum {
	ID_help_report_a_problem = 1,
};

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
		wxBoxSizer* dkdm_sizer = new wxBoxSizer (wxHORIZONTAL);
		_dkdm = new wxTreeCtrl (
			overall_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT
		);
		dkdm_sizer->Add (_dkdm, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_Y_GAP);
		wxBoxSizer* dkdm_buttons = new wxBoxSizer(wxVERTICAL);
		_add_dkdm = new wxButton (overall_panel, wxID_ANY, _("Add..."));
		dkdm_buttons->Add (_add_dkdm, 0, wxALL | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_add_dkdm_folder = new wxButton (overall_panel, wxID_ANY, _("Add folder..."));
		dkdm_buttons->Add (_add_dkdm_folder, 0, wxALL | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		_remove_dkdm = new wxButton (overall_panel, wxID_ANY, _("Remove"));
		dkdm_buttons->Add (_remove_dkdm, 0, wxALL | wxEXPAND, DCPOMATIC_BUTTON_STACK_GAP);
		dkdm_sizer->Add (dkdm_buttons, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);
		right->Add (dkdm_sizer, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_Y_GAP);

		add_dkdm_view (Config::instance()->dkdms());

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
		_dkdm->Bind (wxEVT_TREE_SEL_CHANGED, boost::bind (&DOMFrame::setup_sensitivity, this));
		_dkdm->Bind (wxEVT_TREE_BEGIN_DRAG, boost::bind (&DOMFrame::dkdm_begin_drag, this, _1));
		_dkdm->Bind (wxEVT_TREE_END_DRAG, boost::bind (&DOMFrame::dkdm_end_drag, this, _1));
		_add_dkdm->Bind (wxEVT_BUTTON, bind (&DOMFrame::add_dkdm_clicked, this));
		_add_dkdm_folder->Bind (wxEVT_BUTTON, bind (&DOMFrame::add_dkdm_folder_clicked, this));
		_remove_dkdm->Bind (wxEVT_BUTTON, bind (&DOMFrame::remove_dkdm_clicked, this));

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

	bool confirm_overwrite (boost::filesystem::path path)
	{
		return confirm_dialog (
			this,
			wxString::Format (_("File %s already exists.  Do you want to overwrite it?"), std_to_wx(path.string()).data())
			);
	}

	/** @id if not 0 this is filled in with the wxTreeItemId of the selection */
	shared_ptr<DKDMBase> selected_dkdm (wxTreeItemId* id = 0) const
	{
		wxArrayTreeItemIds selections;
		_dkdm->GetSelections (selections);
		if (selections.GetCount() != 1) {
			if (id) {
				*id = 0;
			}
			return shared_ptr<DKDMBase> ();
		}

		if (id) {
			*id = selections[0];
		}

		DKDMMap::const_iterator i = _dkdm_id.find (selections[0]);
		if (i == _dkdm_id.end()) {
			return shared_ptr<DKDMBase> ();
		}

		return i->second;
	}

	void create_kdms ()
	{
		try {
			shared_ptr<DKDMBase> dkdm_base = selected_dkdm ();
			if (!dkdm_base) {
				return;
			}
			shared_ptr<DKDM> dkdm = boost::dynamic_pointer_cast<DKDM> (dkdm_base);
			if (!dkdm) {
				return;
			}

			/* Decrypt the DKDM */
			dcp::DecryptedKDM decrypted (dkdm->dkdm(), Config::instance()->decryption_chain()->key().get());

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

			pair<shared_ptr<Job>, int> result = _output->make (
				screen_kdms, decrypted.content_title_text(), _timing, bind (&DOMFrame::confirm_overwrite, this, _1), shared_ptr<Log> ()
				);

			if (result.first) {
				JobManager::instance()->add (result.first);
				if (_job_view) {
					_job_view->Destroy ();
					_job_view = 0;
				}
				_job_view = new JobViewDialog (this, _("Send KDM emails"), result.first);
				_job_view->ShowModal ();
			}

			if (result.second > 0) {
				/* XXX: proper plural form support in wxWidgets? */
				wxString s = result.second == 1 ? _("%d KDM written to %s") : _("%d KDMs written to %s");
				message_dialog (
					this,
					wxString::Format (s, result.second, std_to_wx(_output->directory().string()).data())
					);
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
		wxArrayTreeItemIds sel;
		_dkdm->GetSelections (sel);
		_create->Enable (!_screens->screens().empty() && sel.GetCount() > 0);
	}

	void dkdm_begin_drag (wxTreeEvent& ev)
	{
		ev.Allow ();
	}

	void dkdm_end_drag (wxTreeEvent& ev)
	{
		DKDMMap::iterator from = _dkdm_id.find (_dkdm->GetSelection ());
		DKDMMap::iterator to = _dkdm_id.find (ev.GetItem ());
		if (from == _dkdm_id.end() || to == _dkdm_id.end()) {
			return;
		}

		shared_ptr<DKDMGroup> group = dynamic_pointer_cast<DKDMGroup> (to->second);
		if (!group) {
			group = to->second->parent();
		}

		DCPOMATIC_ASSERT (group);
		DCPOMATIC_ASSERT (from->second->parent ());

		from->second->parent()->remove (from->second);
		add_dkdm_model (from->second, group, dynamic_pointer_cast<DKDM>(to->second));

		_dkdm->Delete (from->first);
		_dkdm_id.erase (from->first);
		add_dkdm_view (from->second, dynamic_pointer_cast<DKDM>(to->second) ? to->first : optional<wxTreeItemId>());
	}

	void add_dkdm_clicked ()
	{
		wxFileDialog* d = new wxFileDialog (this, _("Select DKDM file"));
		if (d->ShowModal() == wxID_OK) {
			shared_ptr<DKDMBase> new_dkdm;
			try {
				new_dkdm.reset (
					new DKDM (dcp::EncryptedKDM (dcp::file_to_string (wx_to_std (d->GetPath ()), MAX_KDM_SIZE)))
					);
			} catch (dcp::KDMFormatError& e) {
				error_dialog (
					this,
					wxString::Format (
						_("Could not read file as a KDM.  Perhaps it is badly formatted, or not a KDM at all.\n\n%s"),
						std_to_wx(e.what()).data()
						)
					);
				return;
			}

			shared_ptr<DKDMGroup> group = dynamic_pointer_cast<DKDMGroup> (selected_dkdm ());
			if (!group) {
				group = Config::instance()->dkdms ();
			}
			add_dkdm_model (new_dkdm, group);
			add_dkdm_view (new_dkdm);
		}
		d->Destroy ();
	}

	void add_dkdm_folder_clicked ()
	{
		NewDKDMFolderDialog* d = new NewDKDMFolderDialog (this);
		if (d->ShowModal() == wxID_OK) {
			shared_ptr<DKDMBase> new_dkdm (new DKDMGroup (wx_to_std (d->get ())));
			shared_ptr<DKDMGroup> parent = dynamic_pointer_cast<DKDMGroup> (selected_dkdm ());
			if (!parent) {
				parent = Config::instance()->dkdms ();
			}
			add_dkdm_model (new_dkdm, parent);
			add_dkdm_view (new_dkdm);
		}
		d->Destroy ();
	}

	/** @param dkdm Thing to add.
	 *  @param parent Parent group, or 0.
	 */
	void add_dkdm_view (shared_ptr<DKDMBase> base, optional<wxTreeItemId> previous = optional<wxTreeItemId>())
	{
		if (!base->parent()) {
			/* This is the root group */
			_dkdm_id[_dkdm->AddRoot("root")] = base;
		} else {
			/* Add base to the view */
			wxTreeItemId added;
			if (previous) {
				added = _dkdm->InsertItem(dkdm_to_id(base->parent()), *previous, std_to_wx(base->name()));
			} else {
				added = _dkdm->AppendItem(dkdm_to_id(base->parent()), std_to_wx(base->name()));
			}
			_dkdm_id[added] = base;
		}

		/* Add children */
		shared_ptr<DKDMGroup> g = dynamic_pointer_cast<DKDMGroup> (base);
		if (g) {
			BOOST_FOREACH (shared_ptr<DKDMBase> i, g->children()) {
				add_dkdm_view (i);
			}
		}
	}

	/** @param group Group to add dkdm to */
	void add_dkdm_model (shared_ptr<DKDMBase> dkdm, shared_ptr<DKDMGroup> group, shared_ptr<DKDM> previous = shared_ptr<DKDM> ())
	{
		group->add (dkdm, previous);
		/* We're messing with a Config-owned object here, so tell it that something has changed.
		   This isn't nice.
		*/
		Config::instance()->changed ();
	}

	wxTreeItemId dkdm_to_id (shared_ptr<DKDMBase> dkdm)
	{
		for (DKDMMap::iterator i = _dkdm_id.begin(); i != _dkdm_id.end(); ++i) {
			if (i->second == dkdm) {
				return i->first;
			}
		}
		DCPOMATIC_ASSERT (false);
	}

	void remove_dkdm_clicked ()
	{
		shared_ptr<DKDMBase> removed = selected_dkdm ();
		if (!removed) {
			return;
		}

		_dkdm->Delete (dkdm_to_id (removed));
		shared_ptr<DKDMGroup> dkdms = Config::instance()->dkdms ();
		dkdms->remove (removed);
		Config::instance()->changed ();
	}

	wxPreferencesEditor* _config_dialog;
	ScreensPanel* _screens;
	KDMTimingPanel* _timing;
	wxTreeCtrl* _dkdm;
	typedef std::map<wxTreeItemId, boost::shared_ptr<DKDMBase> > DKDMMap;
	DKDMMap _dkdm_id;
	wxButton* _add_dkdm;
	wxButton* _add_dkdm_folder;
	wxButton* _remove_dkdm;
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
