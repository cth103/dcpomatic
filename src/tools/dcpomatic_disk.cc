/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "wx/disk_warning_dialog.h"
#include "wx/drive_wipe_warning_dialog.h"
#include "wx/editable_list.h"
#include "wx/job_manager_view.h"
#include "wx/message_dialog.h"
#include "wx/try_unmount_dialog.h"
#include "wx/wx_util.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/copy_to_drive_job.h"
#include "lib/cross.h"
#include "lib/dcpomatic_log.h"
#include "lib/disk_writer_messages.h"
#include "lib/file_log.h"
#include "lib/job_manager.h"
#include "lib/signal_manager.h"
#include "lib/util.h"
#include "lib/version.h"
#include <dcp/warnings.h>
#include <wx/cmdline.h>
#include <wx/wx.h>
LIBDCP_DISABLE_WARNINGS
#include <boost/process.hpp>
LIBDCP_ENABLE_WARNINGS
#ifdef DCPOMATIC_WINDOWS
#include <boost/process/windows.hpp>
#endif
#ifdef DCPOMATIC_OSX
#include <notify.h>
#endif


using std::cerr;
using std::cout;
using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


#ifdef DCPOMATIC_OSX
enum {
	ID_tools_uninstall = 1,
};
#endif


class DirDialogWrapper : public wxDirDialog
{
public:
	DirDialogWrapper (wxWindow* parent)
		: wxDirDialog (parent, _("Choose a DCP folder"), wxT(""), wxDD_DIR_MUST_EXIST)
	{

	}

	boost::optional<boost::filesystem::path> get () const
	{
		auto const dcp = boost::filesystem::path(wx_to_std(GetPath()));
		if (!boost::filesystem::exists(dcp / "ASSETMAP") && !boost::filesystem::exists(dcp / "ASSETMAP.xml")) {
			error_dialog (nullptr, _("No ASSETMAP or ASSETMAP.xml found in this folder.  Please choose a DCP folder."));
			return {};
		}

		return dcp;
	}

	void set (boost::filesystem::path)
	{
		/* Not used */
	}
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const & title)
		: wxFrame (nullptr, wxID_ANY, title)
		, _nanomsg (true)
		, _sizer (new wxBoxSizer(wxVERTICAL))
	{
#ifdef DCPOMATIC_OSX
		auto bar = new wxMenuBar;
		auto tools = new wxMenu;
		tools->Append(ID_tools_uninstall, _("Uninstall..."));
		bar->Append(tools, _("Tools"));
		SetMenuBar (bar);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::uninstall, this), ID_tools_uninstall);
#endif

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		auto overall_panel = new wxPanel (this);
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (overall_panel, 1, wxEXPAND);
		SetSizer (s);

		auto grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

		int r = 0;
		add_label_to_sizer (grid, overall_panel, _("DCP"), true, wxGBPosition(r, 0));
		auto dcp_sizer = new wxBoxSizer (wxHORIZONTAL);
		auto dcps = new EditableList<boost::filesystem::path, DirDialogWrapper>(
			overall_panel,
			{ EditableListColumn(_("DCP"), 300, true) },
			boost::bind(&DOMFrame::dcp_paths, this),
			boost::bind(&DOMFrame::set_dcp_paths, this, _1),
			[](boost::filesystem::path p, int) { return p.filename().string(); },
			EditableListTitle::INVISIBLE,
			EditableListButton::NEW | EditableListButton::REMOVE
			);

		dcp_sizer->Add(dcps, 1, wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
		grid->Add(dcp_sizer, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
		++r;

		add_label_to_sizer (grid, overall_panel, _("Drive"), true, wxGBPosition(r, 0));
		auto drive_sizer = new wxBoxSizer (wxHORIZONTAL);
		_drive = new wxChoice (overall_panel, wxID_ANY);
		drive_sizer->Add (_drive, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		_drive_refresh = new wxButton (overall_panel, wxID_ANY, _("Refresh"));
		drive_sizer->Add (_drive_refresh, 0);
		grid->Add (drive_sizer, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
		++r;

		_jobs = new JobManagerView (overall_panel, false);
		grid->Add (_jobs, wxGBPosition(r, 0), wxGBSpan(6, 2), wxEXPAND);
		r += 6;

		_copy = new wxButton (overall_panel, wxID_ANY, _("Copy DCP"));
		grid->Add (_copy, wxGBPosition(r, 0), wxGBSpan(1, 2), wxEXPAND);
		++r;

		grid->AddGrowableCol (1);

		_copy->Bind (wxEVT_BUTTON, boost::bind(&DOMFrame::copy, this));
		_drive->Bind (wxEVT_CHOICE, boost::bind(&DOMFrame::setup_sensitivity, this));
		_drive_refresh->Bind (wxEVT_BUTTON, boost::bind(&DOMFrame::drive_refresh, this));

		_sizer->Add (grid, 1, wxALL | wxEXPAND, DCPOMATIC_DIALOG_BORDER);
		overall_panel->SetSizer (_sizer);
		Fit ();
		SetSize (1024, GetSize().GetHeight() + 32);

		/* XXX: this is a hack, but I expect we'll need logs and I'm not sure if there's
		 * a better place to put them.
		 */
		dcpomatic_log = make_shared<FileLog>(State::write_path("disk.log"));
		dcpomatic_log->set_types (dcpomatic_log->types() | LogEntry::TYPE_DISK);
		LOG_DISK("dcpomatic_disk %1 started", dcpomatic_git_commit);

		drive_refresh ();

		Bind (wxEVT_SIZE, boost::bind(&DOMFrame::sized, this, _1));
		Bind (wxEVT_CLOSE_WINDOW, boost::bind(&DOMFrame::close, this, _1));

		JobManager::instance()->ActiveJobsChanged.connect(boost::bind(&DOMFrame::setup_sensitivity, this));

#ifdef DCPOMATIC_WINDOWS
		/* We must use ::shell here, it seems, to avoid error code 740 (related to privilege escalation) */
		LOG_DISK("Starting writer process %1", disk_writer_path().string());
		_writer = new boost::process::child (disk_writer_path(), boost::process::shell, boost::process::windows::hide);
#endif

#ifdef DCPOMATIC_LINUX
		if (getenv("DCPOMATIC_NO_START_WRITER")) {
			LOG_DISK_NC("Not starting writer process as DCPOMATIC_NO_START_WRITER is set");
		} else {
			LOG_DISK("Starting writer process %1", disk_writer_path().string());
			_writer = new boost::process::child (disk_writer_path());
		}
#endif

#ifdef DCPOMATIC_OSX
		LOG_DISK_NC("Sending notification to writer daemon");
		notify_post ("com.dcpomatic.disk.writer.start");
#endif
	}

	~DOMFrame ()
	{
		_nanomsg.send(DISK_WRITER_QUIT "\n", 2000);
		/* This seems really horrible but it's suggested by the examples on nanomsg.org, so...
		 * Without this the quit is not received (at least sometimes) causing #2018.
		 */
		dcpomatic_sleep_seconds (1);
	}

	void set_dcp_paths (vector<boost::filesystem::path> dcps)
	{
		_dcp_paths = dcps;
		setup_sensitivity();
	}

private:
	vector<boost::filesystem::path> dcp_paths() const
	{
		return _dcp_paths;
	}

	void sized (wxSizeEvent& ev)
	{
		_sizer->Layout ();
		ev.Skip ();
	}


#ifdef DCPOMATIC_OSX
	void uninstall()
	{
		system(String::compose("osascript \"%1/uninstall_disk.applescript\"", resources_path().string()).c_str());
	}
#endif


	bool should_close ()
	{
		if (!JobManager::instance()->work_to_do()) {
			return true;
		}

		auto d = new wxMessageDialog (
			0,
			_("There are unfinished jobs; are you sure you want to quit?"),
			_("Unfinished jobs"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);

		bool const r = d->ShowModal() == wxID_YES;
		d->Destroy ();
		return r;
	}


	void close (wxCloseEvent& ev)
	{
		if (!should_close()) {
			ev.Veto ();
			return;
		}

		ev.Skip ();
	}

	void copy ()
	{
		/* Check that the selected drive still exists and update its properties if so */
		drive_refresh ();
		if (_drive->GetSelection() == wxNOT_FOUND) {
			error_dialog (this, _("The disk you selected is no longer available.  Please choose another."));
			return;
		}

		DCPOMATIC_ASSERT (_drive->GetSelection() != wxNOT_FOUND);
		DCPOMATIC_ASSERT (!_dcp_paths.empty());

		auto ping = [this](int attempt) {
			if (_nanomsg.send(DISK_WRITER_PING "\n", 1000)) {
				auto reply = _nanomsg.receive (1000);
				if (reply && *reply == DISK_WRITER_PONG) {
					return true;
				} else if (reply) {
					LOG_DISK("Unexpected response %1 to ping received (attempt %2)", *reply, attempt);
				} else {
					LOG_DISK("No reply received from ping (attempt %1)", attempt);
				}
			} else {
				LOG_DISK("Could not send ping to writer (attempt %1)", attempt);
			}
			dcpomatic_sleep_seconds (1);
			return false;
		};

		bool have_writer = false;
		for (int i = 0; i < 8; ++i) {
			if (ping(i + 1)) {
				have_writer = true;
				break;
			}
		}

		if (!have_writer) {
#if defined(DCPOMATIC_WINDOWS)
			auto m = new MessageDialog (
				this,
				_("DCP-o-matic Disk Writer"),
				_("Do you see a 'User Account Control' dialogue asking about dcpomatic2_disk_writer.exe?  If so, click 'Yes', then try again.")
				);
			m->ShowModal ();
			m->Destroy ();
			return;
#elif defined(DCPOMATIC_OSX)
			auto m = new MessageDialog (
				this,
				_("DCP-o-matic Disk Writer"),
				_("Did you install the DCP-o-matic Disk Writer.pkg from the .dmg?  Please check and try again.")
				);
			m->ShowModal ();
			m->Destroy ();
			return;
#else
			LOG_DISK_NC ("Failed to ping writer");
			throw CommunicationFailedError ();
#endif
		}

		auto const& drive = _drives[_drive->GetSelection()];
		if (drive.mounted()) {
			auto d = new TryUnmountDialog(this, drive.description());
			int const r = d->ShowModal ();
			d->Destroy ();
			if (r != wxID_OK) {
				return;
			}

			LOG_DISK("Sending unmount request to disk writer for %1", drive.as_xml());
			if (!_nanomsg.send(DISK_WRITER_UNMOUNT "\n", 2000)) {
				LOG_DISK_NC("Failed to send unmount request.");
				throw CommunicationFailedError ();
			}
			if (!_nanomsg.send(drive.as_xml(), 2000)) {
				LOG_DISK_NC("Failed to send drive for unmount request.");
				throw CommunicationFailedError ();
			}
			/* The reply may have to wait for the user to authenticate, so let's wait a while */
			auto reply = _nanomsg.receive (30000);
			if (!reply || *reply != DISK_WRITER_OK) {
				auto * m = new MessageDialog (
						this,
						_("DCP-o-matic Disk Writer"),
						wxString::Format(_("The drive %s could not be unmounted.\nClose any application that is using it, then try again."), std_to_wx(drive.description()))
						);
				m->ShowModal ();
				m->Destroy ();
				return;
			}
		}


		auto * d = new DriveWipeWarningDialog (this, _drive->GetString(_drive->GetSelection()));
		int const r = d->ShowModal ();
		bool ok = r == wxID_OK && d->confirmed();
		d->Destroy ();

		if (!ok) {
			return;
		}

		JobManager::instance()->add(make_shared<CopyToDriveJob>(_dcp_paths, _drives[_drive->GetSelection()], _nanomsg));
		setup_sensitivity ();
	}

	void drive_refresh ()
	{
		int const sel = _drive->GetSelection ();
		wxString current;
		if (sel != wxNOT_FOUND) {
			current = _drive->GetString (sel);
		}
		_drive->Clear ();
		int re_select = wxNOT_FOUND;
		int j = 0;
		_drives = Drive::get ();
		for (auto i: _drives) {
			auto const s = std_to_wx(i.description());
			if (s == current) {
				re_select = j;
			}
			_drive->Append(s);
			++j;
		}
		_drive->SetSelection (re_select);
		setup_sensitivity ();
	}

	void setup_sensitivity ()
	{
		_copy->Enable (!_dcp_paths.empty() && _drive->GetSelection() != wxNOT_FOUND && !JobManager::instance()->work_to_do());
	}

	wxChoice* _drive;
	wxButton* _drive_refresh;
	wxButton* _copy;
	JobManagerView* _jobs;
	std::vector<boost::filesystem::path> _dcp_paths;
	std::vector<Drive> _drives;
#ifndef DCPOMATIC_OSX
	boost::process::child* _writer;
#endif
	Nanomsg _nanomsg;
	wxSizer* _sizer;
};


static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_OPTION, "d", "dcp", "DCP to write", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, "s", "sure", "skip alpha test warnings", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};


class App : public wxApp
{
public:
	App ()
		: _frame (nullptr)
	{}

	bool OnInit () override
	{
		try {
			Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
			Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

			SetAppName (_("DCP-o-matic Disk Writer"));

			if (!wxApp::OnInit()) {
				return false;
			}

#ifdef DCPOMATIC_LINUX
			unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
			dcpomatic_sleep_seconds (1);
			make_foreground_application ();
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

			if (!_skip_alpha_check) {
				auto warning = new DiskWarningDialog ();
				warning->ShowModal ();
				if (!warning->confirmed()) {
					return false;
				}
				warning->Destroy ();
			}

			_frame = new DOMFrame (_("DCP-o-matic Disk Writer"));
			SetTopWindow (_frame);

			_frame->Show ();

			if (_dcp_to_write) {
				_frame->set_dcp_paths({*_dcp_to_write});
			}

			signal_manager = new wxSignalManager (this);
			Bind (wxEVT_IDLE, boost::bind (&App::idle, this, _1));
		}
		catch (exception& e)
		{
			error_dialog (0, wxString::Format ("DCP-o-matic could not start."), std_to_wx(e.what()));
			return false;
		}

		return true;
	}

	void OnInitCmdLine (wxCmdLineParser& parser) override
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser) override
	{
		_skip_alpha_check = parser.Found(wxT("sure"));

		wxString dcp;
		if (parser.Found(wxT("dcp"), &dcp)) {
			_dcp_to_write = wx_to_std (dcp);
		}

		return true;
	}

	void config_failed_to_load ()
	{
		message_dialog (_frame, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
	}

	void config_warning (string m)
	{
		message_dialog (_frame, std_to_wx(m));
	}

	void idle (wxIdleEvent& ev)
	{
		signal_manager->ui_idle ();
		ev.Skip ();
	}

	void report_exception ()
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
					_("An exception occurred: %s.\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what ())
					)
				);
		} catch (...) {
			error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}
	}

	bool OnExceptionInMainLoop () override
	{
		report_exception ();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException () override
	{
		report_exception ();
	}

	DOMFrame* _frame;
	bool _skip_alpha_check = false;
	boost::optional<boost::filesystem::path> _dcp_to_write;
};

IMPLEMENT_APP (App)
