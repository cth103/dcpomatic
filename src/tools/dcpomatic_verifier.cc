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
#include "wx/editable_list.h"
#include "wx/verify_dcp_progress_panel.h"
#include "wx/verify_dcp_result_panel.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/job_manager.h"
#include "lib/verify_dcp_job.h"
#include "lib/util.h"
#include <dcp/search.h>
#include <dcp/verify_report.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/evtloop.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif


using std::dynamic_pointer_cast;
using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::vector;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


class DirDialogWrapper : public wxDirDialog
{
public:
	DirDialogWrapper(wxWindow* parent)
		: wxDirDialog(parent, _("Choose a folder"), {}, wxDD_DIR_MUST_EXIST)
	{

	}

	vector<boost::filesystem::path> get() const
	{
		return dcp::find_potential_dcps(boost::filesystem::path(wx_to_std(GetPath())));
	}

	void set(boost::filesystem::path)
	{
		/* Not used */
	}

private:
	void search(vector<boost::filesystem::path>& dcp, boost::filesystem::path path) const
	{
		if (dcp::filesystem::exists(path / "ASSETMAP") || dcp::filesystem::exists(path / "ASSETMAP.xml")) {
			dcp.push_back(path);
		} else if (boost::filesystem::is_directory(path)) {
			for (auto i: boost::filesystem::directory_iterator(path)) {
				search(dcp, i.path());
			}
		}
	};
};




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
		add_label_to_sizer(dcp_sizer, this, _("DCPs"), true, 0, wxALIGN_CENTER_VERTICAL);

		auto dcps = new EditableList<boost::filesystem::path, DirDialogWrapper>(
			this,
			{ EditableListColumn(_("DCP"), 300, true) },
			boost::bind(&DOMFrame::dcp_paths, this),
			boost::bind(&DOMFrame::set_dcp_paths, this, _1),
			[](boost::filesystem::path p, int) { return p.filename().string(); },
			EditableListTitle::INVISIBLE,
			EditableListButton::NEW | EditableListButton::REMOVE
			);

		dcp_sizer->Add(dcps, 1, wxLEFT | wxEXPAND, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(dcp_sizer, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		auto options_sizer = new wxBoxSizer(wxVERTICAL);
		_check_picture_details = new CheckBox(this, _("Verify picture asset details"));
		_check_picture_details->set(true);
		_check_picture_details->SetToolTip(
			_("Tick to check details of the picture asset, such as frame sizes and JPEG2000 bitstream validity.  "
			  "These checks are quite time-consuming.")
		);
		options_sizer->Add(_check_picture_details, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);
		_write_log = new CheckBox(this, _("Write logs to DCP folders"));
		options_sizer->Add(_write_log, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(options_sizer, 0, wxLEFT, DCPOMATIC_DIALOG_BORDER);

		auto actions_sizer = new wxBoxSizer(wxHORIZONTAL);
		_cancel = new Button(this, _("Cancel"));
		actions_sizer->Add(_cancel, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
		_verify = new Button(this, _("Verify"));
		actions_sizer->Add(_verify, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(actions_sizer, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER, DCPOMATIC_DIALOG_BORDER);

		_progress_panel = new VerifyDCPProgressPanel(this);
		overall_sizer->Add(_progress_panel, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		_result_panel = new VerifyDCPResultPanel(this);
		overall_sizer->Add(_result_panel, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		SetSizerAndFit(overall_sizer);

		_cancel->bind(&DOMFrame::cancel_clicked, this);
		_verify->bind(&DOMFrame::verify_clicked, this);

		setup_sensitivity();
	}

private:
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
				std::vector<boost::filesystem::path>{dcp},
				std::vector<boost::filesystem::path>(),
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
				dcp::TextFormatter formatter(_dcp_paths[i] / "REPORT.txt");
				dcp::verify_report({ jobs[i]->result() }, formatter);
			}
		}

		_progress_panel->clear();
		setup_sensitivity();
	}

private:
	void set_dcp_paths (vector<boost::filesystem::path> dcps)
	{
		_dcp_paths = dcps;
		setup_sensitivity();
	}

	vector<boost::filesystem::path> dcp_paths() const
	{
		return _dcp_paths;
	}

	std::vector<boost::filesystem::path> _dcp_paths;
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
					wx::report_problem()
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
					wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s.\n\n%s"),
					std_to_wx(e.what()),
					wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), wx::report_problem()));
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
