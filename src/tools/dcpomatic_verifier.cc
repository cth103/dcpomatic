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


#include "wx/check_box.h"
#include "wx/dcpomatic_button.h"
#include "wx/dir_picker_ctrl.h"
#include "wx/verify_dcp_progress_panel.h"
#include "wx/verify_dcp_result_panel.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/job_manager.h"
#include "lib/verify_dcp_job.h"
#include "lib/util.h"
#include <dcp/verify_report.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/evtloop.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif


using std::exception;
using std::make_shared;


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame(wxString const& title)
		: wxFrame(nullptr, -1, title)
	{
#ifdef DCPOMATIC_WINDOWS
		SetIcon(wxIcon(std_to_wx("id")));
#endif
		auto overall_sizer = new wxBoxSizer(wxVERTICAL);

		auto dcp_sizer = new wxBoxSizer(wxHORIZONTAL);
		add_label_to_sizer(dcp_sizer, this, _("DCP"), true, 0, wxALIGN_CENTER_VERTICAL);
		_dcp = new DirPickerCtrl(this, true);
		dcp_sizer->Add(_dcp, 1, wxEXPAND);
		overall_sizer->Add(dcp_sizer, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		auto options_sizer = new wxBoxSizer(wxVERTICAL);
		_write_log = new CheckBox(this, _("Write log to DCP folder"));
		options_sizer->Add(_write_log, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(options_sizer, 0, wxLEFT, DCPOMATIC_DIALOG_BORDER);

		_verify = new Button(this, _("Verify"));
		overall_sizer->Add(_verify, 0, wxEXPAND | wxLEFT | wxRIGHT, DCPOMATIC_DIALOG_BORDER);

		_progress_panel = new VerifyDCPProgressPanel(this);
		overall_sizer->Add(_progress_panel, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		_result_panel = new VerifyDCPResultPanel(this);
		overall_sizer->Add(_result_panel, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		SetSizerAndFit(overall_sizer);

		_dcp->Changed.connect(boost::bind(&DOMFrame::setup_sensitivity, this));
		_verify->bind(&DOMFrame::verify_clicked, this);

		setup_sensitivity();
	}

private:
	void setup_sensitivity()
	{
		_verify->Enable(!_dcp->GetPath().IsEmpty());
	}

	void verify_clicked()
	{
		auto dcp = boost::filesystem::path(wx_to_std(_dcp->GetPath()));
		if (dcp.empty()) {
			return;
		}

		auto job_manager = JobManager::instance();
		auto job = make_shared<VerifyDCPJob>(std::vector<boost::filesystem::path>{dcp}, std::vector<boost::filesystem::path>());
		job_manager->add(job);

		while (job_manager->work_to_do()) {
			wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
			dcpomatic_sleep_seconds(1);

			_progress_panel->update(job);
		}

		_result_panel->fill(job);
		if (_write_log->get()) {
			dcp::TextFormatter formatter(dcp / "REPORT.txt");
			dcp::verify_report(job->result(), formatter);
		}
	}

	DirPickerCtrl* _dcp;
	CheckBox* _write_log;
	Button* _verify;
	VerifyDCPProgressPanel* _progress_panel;
	VerifyDCPResultPanel* _result_panel;
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
			dcpomatic_setup_i18n();

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
			_frame->SetSize({480, 640});
			_frame->Show();
		}
		catch (exception& e)
		{
			error_dialog(nullptr, variant::wx::insert_dcpomatic_verifier("%s could not start."), std_to_wx(e.what()));
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
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str())
					)
				);
		} catch (boost::filesystem::filesystem_error& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s) (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx(e.what()),
					std_to_wx(e.path1().string()),
					std_to_wx(e.path2().string())
					)
				);
		} catch (exception& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s.\n\n") + REPORT_PROBLEM,
					std_to_wx(e.what())
					)
				);
		} catch (...) {
			error_dialog(nullptr, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
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
