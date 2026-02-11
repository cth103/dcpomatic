/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


/** @file  src/tools/dcpomatic_verify.cc
 *  @brief A DCP verify GUI.
 */


#include "wx/about_dialog.h"
#include "wx/check_box.h"
#include "wx/dcpomatic_button.h"
#include "wx/dir_dialog.h"
#include "wx/dir_picker_ctrl.h"
#include "wx/editable_list.h"
#include "wx/file_dialog.h"
#include "wx/i18n_setup.h"
#include "wx/id.h"
#include "wx/verify_dcp_progress_panel.h"
#include "wx/verify_dcp_result_panel.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/dkdm_wrapper.h"
#include "lib/job_manager.h"
#include "lib/verify_dcp_job.h"
#include "lib/util.h"
#include "lib/variant.h"
#include <dcp/dcp.h>
#include <dcp/search.h>
#include <dcp/text_formatter.h>
#include <dcp/verify_report.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/evtloop.h>
#include <wx/progdlg.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif


using std::dynamic_pointer_cast;
using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


enum {
	ID_help_report_a_problem = DCPOMATIC_MAIN_MENU
};


class DCPPath
{
public:
	explicit DCPPath(boost::filesystem::path path, vector<dcp::DecryptedKDM> const& kdms)
		: _path(std::move(path))
	{
		check(kdms);
	}

	void check(vector<dcp::DecryptedKDM> const& kdms)
	{
		dcp::DCP dcp(_path);
		dcp.read(nullptr, true);
		_encrypted = dcp.any_encrypted();
		if (_encrypted) {
			for (auto const& kdm: kdms) {
				dcp.add(kdm);
			}
		}
		_readable = dcp.can_be_read();
	}

	string description() const
	{
		string d = _path.filename().string();
		if (_encrypted) {
			if (_readable) {
				d += string{" "} + wx_to_std(_("(encrypted, have KDM)"));
			} else {
				d += string{" "} + wx_to_std(_("(encrypted, no KDM)"));
			}
		}
		return d;
	}

	boost::filesystem::path path() const {
		return _path;
	}

private:
	boost::filesystem::path _path;
	bool _encrypted;
	bool _readable;
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame(wxString const& title)
		: wxFrame(nullptr, -1, title)
		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		, _overall_panel(new wxPanel(this, wxID_ANY))
	{
		auto bar = new wxMenuBar;
		setup_menu(bar);
		SetMenuBar(bar);

		Bind(wxEVT_MENU, boost::bind(&DOMFrame::file_exit, this), wxID_EXIT);
		Bind(wxEVT_MENU, boost::bind(&DOMFrame::help_about, this), wxID_ABOUT);

#ifdef DCPOMATIC_WINDOWS
		SetIcon(wxIcon(std_to_wx("id")));
#endif
		auto overall_sizer = new wxBoxSizer(wxVERTICAL);

		auto dcp_sizer = new wxBoxSizer(wxHORIZONTAL);
		add_label_to_sizer(dcp_sizer, _overall_panel, _("DCPs"), true, 0, wxALIGN_CENTER_VERTICAL);

		auto add = [this](wxWindow* parent) {
			DirDialog dialog(parent, _("Select DCP(s)"), wxDD_MULTIPLE, "AddVerifierInputPath");

			if (dialog.show()) {
				wxProgressDialog progress(variant::wx::dcpomatic(), _("Examining DCPs"));
				vector<DCPPath> paths;
				for (auto path: dialog.paths()) {
					for (auto const& dcp: dcp::find_potential_dcps(path)) {
						progress.Pulse();
						paths.push_back(DCPPath(dcp, _kdms));
					}
				}
				return paths;
			} else {
				return std::vector<DCPPath>{};
			}
		};

		_dcps = new EditableList<DCPPath>(
			_overall_panel,
			{ EditableListColumn(_("DCP"), 300, true) },
			boost::bind(&DOMFrame::dcp_paths, this),
			boost::bind(&DOMFrame::set_dcp_paths, this, _1),
			add,
			std::function<void (wxWindow*, DCPPath&)>(),
			[](DCPPath p, int) { return p.description(); },
			EditableListTitle::INVISIBLE,
			EditableListButton::NEW | EditableListButton::REMOVE,
			_("Add KDM...")
			);

		dcp_sizer->Add(_dcps, 1, wxLEFT | wxEXPAND, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(dcp_sizer, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		auto options_sizer = new wxBoxSizer(wxVERTICAL);
		_check_picture_details = new CheckBox(_overall_panel, _("Verify picture asset details"));
		_check_picture_details->set(true);
		_check_picture_details->SetToolTip(
			_("Tick to check details of the picture asset, such as frame sizes and JPEG2000 bitstream validity.  "
			  "These checks are quite time-consuming.")
		);
		options_sizer->Add(_check_picture_details, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);
		_write_log = new CheckBox(_overall_panel, _("Write logs to DCP folders"));
		options_sizer->Add(_write_log, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(options_sizer, 0, wxLEFT, DCPOMATIC_DIALOG_BORDER);

		auto actions_sizer = new wxBoxSizer(wxHORIZONTAL);
		_cancel = new Button(_overall_panel, _("Cancel"));
		actions_sizer->Add(_cancel, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
		_verify = new Button(_overall_panel, _("Verify"));
		actions_sizer->Add(_verify, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(actions_sizer, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER, DCPOMATIC_DIALOG_BORDER);

		_progress_panel = new VerifyDCPProgressPanel(_overall_panel);
		overall_sizer->Add(_progress_panel, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		_result_panel = new VerifyDCPResultPanel(_overall_panel);
		overall_sizer->Add(_result_panel, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		_overall_panel->SetSizerAndFit(overall_sizer);

		_cancel->bind(&DOMFrame::cancel_clicked, this);
		_verify->bind(&DOMFrame::verify_clicked, this);
		_dcps->custom_button()->bind(&DOMFrame::add_kdm_clicked, this);

		setup_sensitivity();

		if (auto private_key = Config::instance()->decryption_chain()->key()) {
			for (auto dkdm: Config::instance()->dkdms()->all_dkdms()) {
				try {
					_kdms.push_back(dcp::DecryptedKDM(dkdm, *private_key));
				} catch (...) {}
			}
		}
	}

private:
	void file_exit()
	{
		Close();
	}

	void help_about()
	{
		AboutDialog dialog(_overall_panel);
		dialog.ShowModal();
	}

	void setup_menu(wxMenuBar* m)
	{
		auto help = new wxMenu;
#ifdef DCPOMATIC_OSX
		/* This just needs to be appended somewhere, it seems - it magically
		 * gets moved to the right place.
		 */
		help->Append(wxID_EXIT, _("&Exit"));
		help->Append(wxID_ABOUT, variant::wx::insert_dcpomatic(_("About %s")));
#else
		auto file = new wxMenu;
		file->Append(wxID_EXIT, _("&Quit"));
		m->Append(file, _("&File"));

		help->Append(wxID_ABOUT, _("About"));
#endif
		m->Append(help, _("&Help"));
	}

	void setup_sensitivity()
	{
		auto const work = JobManager::instance()->work_to_do();
		_cancel->Enable(work);
		_verify->Enable(!_dcp_paths.empty() && !work);
	}

	void cancel_clicked()
	{
		_cancel_pending = true;
	}

	void verify_clicked()
	{
		dcp::VerificationOptions options;
		options.check_picture_details = _check_picture_details->get();
		auto job_manager = JobManager::instance();
		vector<shared_ptr<const VerifyDCPJob>> jobs;
		for (auto const& dcp: _dcp_paths) {
			auto job = make_shared<VerifyDCPJob>(
				std::vector<boost::filesystem::path>{dcp.path()},
				_kdms,
				options
			);
			job_manager->add(job);
			jobs.push_back(job);
		}

		setup_sensitivity();

		while (job_manager->work_to_do() && !_cancel_pending) {
			wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
			dcpomatic_sleep_milliseconds(250);
			auto last = job_manager->last_active_job();
			if (auto locked = last.lock()) {
				if (auto dcp = dynamic_pointer_cast<VerifyDCPJob>(locked)) {
					_progress_panel->update(dcp);
				}
			}
		}

		if (_cancel_pending) {
			_cancel_pending = false;
			JobManager::instance()->cancel_all_jobs();
			_progress_panel->clear();
			setup_sensitivity();
			return;
		}

		DCPOMATIC_ASSERT(_dcp_paths.size() == jobs.size());
		_result_panel->add(jobs);
		if (_write_log->get()) {
			for (size_t i = 0; i < _dcp_paths.size(); ++i) {
				dcp::TextFormatter formatter(_dcp_paths[i].path() / "REPORT.txt");
				dcp::verify_report({ jobs[i]->result() }, formatter);
			}
		}

		_progress_panel->clear();
		setup_sensitivity();
	}

private:
	void set_dcp_paths(vector<DCPPath> dcps)
	{
		_dcp_paths = dcps;
		setup_sensitivity();
	}

	vector<DCPPath> dcp_paths() const
	{
		return _dcp_paths;
	}

	void add_kdm_clicked()
	{
		FileDialog dialog(this, _("Select KDM"), char_to_wx("XML files|*.xml|All files|*.*"), wxFD_MULTIPLE, "AddKDMPath");
		if (!dialog.show()) {
			return;
		}

		for (auto path: dialog.paths()) {
			try {
				auto encrypted = dcp::EncryptedKDM(dcp::file_to_string(path));
				_kdms.push_back(dcp::DecryptedKDM(encrypted, Config::instance()->decryption_chain()->key().get()));
			} catch (dcp::KDMFormatError& e) {
				error_dialog (
					this,
					_("Could not read file as a KDM.  Perhaps it is badly formatted, or not a KDM at all."),
					std_to_wx(e.what())
					);
				return;
			} catch (dcp::KDMDecryptionError &) {
				error_dialog (
					this,
					_("Could not decrypt the DKDM.  Perhaps it was not created with the correct certificate.")
					);
			} catch (exception& e) {
				error_dialog(this, wxString::Format(_("Could not load KDM.")), std_to_wx(e.what()));
				return;
			}
		}

		wxProgressDialog progress(variant::wx::dcpomatic(), _("Checking KDM"));
		for (auto& dcp: _dcp_paths) {
			dcp.check(_kdms);
			progress.Pulse();
		}
		_dcps->refresh();
	}

	wxPanel* _overall_panel = nullptr;
	EditableList<DCPPath>* _dcps;
	std::vector<DCPPath> _dcp_paths;
	std::vector<dcp::DecryptedKDM> _kdms;
	CheckBox* _check_picture_details;
	CheckBox* _write_log;
	Button* _cancel;
	Button* _verify;
	VerifyDCPProgressPanel* _progress_panel;
	VerifyDCPResultPanel* _result_panel;
	bool _cancel_pending = false;
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App()
		: wxApp()
	{
		dcpomatic_setup_path_encoding();
#ifdef DCPOMATIC_LINUX
		XInitThreads();
#endif
	}

private:
	bool OnInit() override
	{
		try {
			SetAppName(variant::wx::dcpomatic_verifier());

			if (!wxApp::OnInit()) {
				return false;
			}

#ifdef DCPOMATIC_LINUX
			unsetenv("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
			dcpomatic_sleep_seconds(1);
			make_foreground_application();
#endif

			/* Enable i18n; this will create a Config object
			   to look for a force-configured language.  This Config
			   object will be wrong, however, because dcpomatic_setup
			   hasn't yet been called and there aren't any filters etc.
			   set up yet.
			*/
			dcpomatic::wx::setup_i18n();

			/* Set things up, including filters etc.
			   which will now be internationalised correctly.
			*/
			dcpomatic_setup();

			/* Force the configuration to be re-loaded correctly next
			   time it is needed.
			*/
			Config::drop();

			_frame = new DOMFrame(variant::wx::dcpomatic_verifier());
			SetTopWindow(_frame);
			_frame->Maximize();
			_frame->Show();
		}
		catch (exception& e)
		{
			error_dialog(nullptr, variant::wx::insert_dcpomatic_verifier(char_to_wx("%s could not start.")), std_to_wx(e.what()));
		}

		return true;
	}

	void report_exception()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (boost::filesystem::filesystem_error& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s) (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.path1().string()),
					std_to_wx(e.path2().string()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s.\n\n%s"),
					std_to_wx(e.what()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), dcpomatic::wx::report_problem()));
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop() override
	{
		report_exception();
		return false;
	}

	void OnUnhandledException() override
	{
		report_exception();
	}

	DOMFrame* _frame = nullptr;
};


IMPLEMENT_APP(App)
