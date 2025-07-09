/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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


#include "wx/i18n_setup.h"
#include "wx/id.h"
#include "wx/static_text.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/config.h"
#ifdef DCPOMATIC_GROK
#include "lib/grok/context.h"
#endif
#include "lib/cross.h"
#include "lib/dcpomatic_log.h"
#include "lib/encode_server.h"
#include "lib/encoded_log_entry.h"
#include "lib/log.h"
#include "lib/log.h"
#include "lib/signaller.h"
#include "lib/signaller.h"
#include "lib/util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/icon.h>
#include <wx/splash.h>
#include <wx/taskbar.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/optional.hpp>
#include <boost/thread.hpp>
#include <iostream>


using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::fixed;
using std::list;
using std::setprecision;
using std::shared_ptr;
using std::string;
using boost::bind;
using boost::optional;
using boost::thread;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


enum {
	ID_status = DCPOMATIC_MAIN_MENU,
	ID_quit,
	ID_timer
};


/* Make this longer than the tallest we might want the window to be.
 * In an ideal world we'd scale it with the window size.
 */
static unsigned int const log_lines = 128;


class ServerLog : public Log, public Signaller
{
public:
	ServerLog ()
		: _fps (0)
	{}

	string get () const {
		string a;
		for (auto const& i: _log) {
			a += i + "\n";
		}
		return a;
	}

	float fps () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _fps;
	}

	boost::signals2::signal<void(string)> Appended;
	boost::signals2::signal<void(int)> Removed;

private:
	void do_log (shared_ptr<const LogEntry> entry) override
	{
		time_t const s = entry->seconds ();
		struct tm* local = localtime (&s);
		if (
			!_last_time ||
			local->tm_yday != _last_time->tm_yday ||
			local->tm_year != _last_time->tm_year ||
			local->tm_hour != _last_time->tm_hour ||
			local->tm_min != _last_time->tm_min
			) {
			char buffer[64];
			strftime (buffer, 64, "%c", local);
			append (buffer);
		}

		if (_log.size() > log_lines) {
			emit (boost::bind (boost::ref (Removed), _log.front().length()));
			_log.pop_front ();
		}

		append (entry->message ());
		_last_time = *local;

		auto encoded = dynamic_pointer_cast<const EncodedLogEntry> (entry);
		if (encoded) {
			_history.push_back (encoded->seconds ());
			if (_history.size() > 48) {
				_history.pop_front ();
			}
			if (_history.size() > 2) {
				boost::mutex::scoped_lock lm (_state_mutex);
				_fps = _history.size() / (_history.back() - _history.front());
			}
		}
	}

	void append (string s)
	{
		_log.push_back (s);
		emit (boost::bind(boost::ref(Appended), s));
	}

	list<string> _log;
	optional<struct tm> _last_time;
	std::list<double> _history;

	mutable boost::mutex _state_mutex;
	float _fps;
};


static shared_ptr<ServerLog> server_log;


class StatusDialog : public wxDialog
{
public:
	StatusDialog ()
		: wxDialog (
			nullptr, wxID_ANY, variant::wx::dcpomatic_encode_server(),
			wxDefaultPosition, wxDefaultSize,
#ifdef DCPOMATIC_OSX
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP
#else
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
#endif
			)
	{
		auto state_sizer = new wxBoxSizer(wxHORIZONTAL);

		add_label_to_sizer(state_sizer, this, _("Frames per second"), true, 0, 0);
		_fps = new StaticText(this, {});
		state_sizer->Add(_fps, 1, wxLEFT, DCPOMATIC_DIALOG_BORDER);

		_text = new wxTextCtrl (
			this, wxID_ANY, std_to_wx (server_log->get()), wxDefaultPosition, wxDefaultSize,
			wxTE_READONLY | wxTE_MULTILINE
			);

		auto overall_sizer = new wxBoxSizer(wxVERTICAL);
		overall_sizer->Add(state_sizer, 0, wxLEFT | wxTOP | wxRIGHT, DCPOMATIC_DIALOG_BORDER);
		overall_sizer->Add(_text, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
		SetSizer(overall_sizer);
		overall_sizer->Layout();

		Bind (wxEVT_TIMER, boost::bind (&StatusDialog::update_state, this));
		_timer.reset (new wxTimer (this));
		_timer->Start (1000);

		server_log->Appended.connect (bind (&StatusDialog::appended, this, _1));
		server_log->Removed.connect (bind (&StatusDialog::removed, this, _1));

		SetSize(800, 600);
	}

private:
	void appended (string s)
	{
		(*_text) << std_to_wx(s) << char_to_wx("\n");
	}

	void removed (int n)
	{
#ifdef DCPOMATIC_WINDOWS
		_text->Remove(0, n + 2);
#else
		_text->Remove(0, n + 1);
#endif
	}

	void update_state ()
	{
		_fps->SetLabel(wxString::Format(char_to_wx("%.1f"), server_log->fps()));
	}

	wxTextCtrl* _text;
	wxStaticText* _fps;
	std::shared_ptr<wxTimer> _timer;
};


static StatusDialog* status_dialog = nullptr;


class TaskBarIcon : public wxTaskBarIcon
{
public:
	TaskBarIcon ()
	{
		set_icon ();

		Bind (wxEVT_MENU, boost::bind (&TaskBarIcon::status, this), ID_status);
		Bind (wxEVT_MENU, boost::bind (&TaskBarIcon::quit, this), ID_quit);
	}

	wxMenu* CreatePopupMenu () override
	{
		auto menu = new wxMenu;
		menu->Append(ID_status, _("Status..."));
		menu->Append(ID_quit, _("Quit"));
		return menu;
	}

	void set_icon ()
	{
#ifdef DCPOMATIC_WINDOWS
		wxIcon icon (std_to_wx("id"));
#else
		string const colour = gui_is_dark() ? "white" : "black";
		wxBitmap bitmap (
			bitmap_path(fmt::format("dcpomatic_small_{}.png", colour)),
			wxBITMAP_TYPE_PNG
			);
		wxIcon icon;
		icon.CopyFromBitmap (bitmap);
#endif

		SetIcon(icon, _("DCP-o-matic Encode Server"));
	}

private:
	void status ()
	{
		status_dialog->Show ();
	}

	void quit ()
	{
		wxTheApp->ExitMainLoop ();
	}
};


class App : public wxApp, public ExceptionStore
{
public:
	App ()
		: wxApp ()
	{}

private:

	bool OnInit () override
	{
		if (!wxApp::OnInit()) {
			return false;
		}

		wxInitAllImageHandlers ();

		server_log.reset (new ServerLog);
		server_log->set_types (LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR);
		dcpomatic_log = server_log;

		Config::FailedToLoad.connect(boost::bind(&App::config_failed_to_load, this, _1));
		Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

		auto splash = maybe_show_splash ();

		dcpomatic_setup_path_encoding ();
		dcpomatic::wx::setup_i18n();
		dcpomatic_setup ();
		Config::drop ();

		signal_manager = new wxSignalManager (this);
		Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		/* Bad things happen (on Linux at least) if the config is reloaded by main_thread;
		   it seems like there's a race which results in the locked_sstream mutex being
		   locked before it is initialised.  Calling Config::instance() here loads the config
		   again in this thread, which seems to work around the problem.
		*/
		Config::instance();

		status_dialog = new StatusDialog ();
#ifdef DCPOMATIC_LINUX
		status_dialog->Show ();
#else
		_icon = new TaskBarIcon;
		status_dialog->Bind (wxEVT_SYS_COLOUR_CHANGED, boost::bind(&TaskBarIcon::set_icon, _icon));
#endif
		_thread = thread (bind (&App::main_thread, this));

		Bind (wxEVT_TIMER, boost::bind (&App::check, this));
		_timer.reset (new wxTimer (this));
		_timer->Start (1000);

		if (splash) {
			splash->Destroy ();
		}

		SetExitOnFrameDelete (false);

#ifdef DCPOMATIC_GROK
		grk_plugin::setMessengerLogger(new grk_plugin::GrokLogger("[GROK] "));
		setup_grok_library_path();
#endif

		return true;
	}

	int OnExit () override
	{
		delete _icon;
		return wxApp::OnExit ();
	}

	void main_thread ()
	try {
		EncodeServer server (false, Config::instance()->server_encoding_threads());
		server.run ();
	} catch (...) {
		store_current ();
	}

	void check ()
	{
		try {
			rethrow ();
		} catch (exception& e) {
			error_dialog (nullptr, std_to_wx(e.what()));
			wxTheApp->ExitMainLoop ();
		} catch (...) {
			error_dialog(nullptr, variant::wx::insert_dcpomatic_encode_server(_("An unknown error has occurred with the %s.")));
			wxTheApp->ExitMainLoop ();
		}
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	void config_failed_to_load(Config::LoadFailure what)
	{
		report_config_load_failure(nullptr, what);
	}

	void config_warning (string m)
	{
		message_dialog (nullptr, std_to_wx(m));
	}

	boost::thread _thread;
	TaskBarIcon* _icon = nullptr;
	shared_ptr<wxTimer> _timer;
};

IMPLEMENT_APP (App)
