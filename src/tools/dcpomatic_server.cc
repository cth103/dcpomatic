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

#include "wx/wx_util.h"
#include "wx/wx_signal_manager.h"
#include "wx/static_text.h"
#include "lib/util.h"
#include "lib/encoded_log_entry.h"
#include "lib/encode_server.h"
#include "lib/config.h"
#include "lib/log.h"
#include "lib/signaller.h"
#include "lib/cross.h"
#include "lib/dcpomatic_log.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/taskbar.h>
#include <wx/splash.h>
#include <wx/icon.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/thread.hpp>
#include <boost/optional.hpp>
#include <iostream>

using std::cout;
using std::string;
using std::exception;
using std::list;
using std::fixed;
using std::setprecision;
using std::shared_ptr;
using boost::thread;
using boost::bind;
using boost::optional;
using std::dynamic_pointer_cast;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

enum {
	ID_status = 1,
	ID_quit,
	ID_timer
};

static unsigned int const log_lines = 32;

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
	void do_log (shared_ptr<const LogEntry> entry)
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

		append (entry->message ());
		if (_log.size() > log_lines) {
			emit (boost::bind (boost::ref (Removed), _log.front().length()));
			_log.pop_front ();
		}
		_last_time = *local;

		shared_ptr<const EncodedLogEntry> encoded = dynamic_pointer_cast<const EncodedLogEntry> (entry);
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
		emit (boost::bind (boost::ref (Appended), s));
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
			0, wxID_ANY, _("DCP-o-matic Encode Server"),
			wxDefaultPosition, wxDefaultSize,
#ifdef DCPOMATIC_OSX
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP
#else
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
#endif
			)
	{
		wxFlexGridSizer* state_sizer = new wxFlexGridSizer (2, DCPOMATIC_SIZER_GAP, DCPOMATIC_SIZER_GAP);

		add_label_to_sizer (state_sizer, this, _("Frames per second"), true);
		_fps = new StaticText (this, wxT(""));
		state_sizer->Add (_fps);

		wxFlexGridSizer* log_sizer = new wxFlexGridSizer (1, DCPOMATIC_SIZER_GAP, DCPOMATIC_SIZER_GAP);
		log_sizer->AddGrowableCol (0, 1);

		wxClientDC dc (this);
		wxSize size = dc.GetTextExtent (wxT ("This is the length of the file label it should be quite long"));
		int const height = (size.GetHeight() + 2) * log_lines;
		SetSize (700, height + DCPOMATIC_SIZER_GAP * 2);

		_text = new wxTextCtrl (
			this, wxID_ANY, std_to_wx (server_log->get()), wxDefaultPosition, wxSize (-1, height),
			wxTE_READONLY | wxTE_MULTILINE
			);

		log_sizer->Add (_text, 1, wxEXPAND);

		wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
		overall_sizer->Add (state_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add (log_sizer, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);
		SetSizer (overall_sizer);
		overall_sizer->Layout ();

		Bind (wxEVT_TIMER, boost::bind (&StatusDialog::update_state, this));
		_timer.reset (new wxTimer (this));
		_timer->Start (1000);

		server_log->Appended.connect (bind (&StatusDialog::appended, this, _1));
		server_log->Removed.connect (bind (&StatusDialog::removed, this, _1));
	}

private:
	void appended (string s)
	{
		(*_text) << s << "\n";
	}

	void removed (int n)
	{
		_text->Remove (0, n + 1);
	}

	void update_state ()
	{
		_fps->SetLabel (wxString::Format ("%.1f", server_log->fps()));
	}

	wxTextCtrl* _text;
	wxStaticText* _fps;
	std::shared_ptr<wxTimer> _timer;
};

class TaskBarIcon : public wxTaskBarIcon
{
public:
	TaskBarIcon ()
		: _status (0)
	{
#ifdef DCPOMATIC_WINDOWS
		wxIcon icon (std_to_wx ("id"));
#else
		wxBitmap bitmap (wxString::Format(wxT("%s/dcpomatic_small.png"), std_to_wx(resources_path().string())), wxBITMAP_TYPE_PNG);
		wxIcon icon;
		icon.CopyFromBitmap (bitmap);
#endif

		SetIcon (icon, std_to_wx ("DCP-o-matic Encode Server"));

		Bind (wxEVT_MENU, boost::bind (&TaskBarIcon::status, this), ID_status);
		Bind (wxEVT_MENU, boost::bind (&TaskBarIcon::quit, this), ID_quit);
	}

	wxMenu* CreatePopupMenu ()
	{
		wxMenu* menu = new wxMenu;
		menu->Append (ID_status, std_to_wx ("Status..."));
		menu->Append (ID_quit, std_to_wx ("Quit"));
		return menu;
	}

private:
	void status ()
	{
		if (!_status) {
			_status = new StatusDialog ();
		}
		_status->Show ();
	}

	void quit ()
	{
		wxTheApp->ExitMainLoop ();
	}

	StatusDialog* _status;
};

class App : public wxApp, public ExceptionStore
{
public:
	App ()
		: wxApp ()
		, _icon (0)
	{}

private:

	bool OnInit ()
	{
		if (!wxApp::OnInit ()) {
			return false;
		}

		wxInitAllImageHandlers ();

		server_log.reset (new ServerLog);
		server_log->set_types (LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR);
		dcpomatic_log = server_log;

		Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
		Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

		wxSplashScreen* splash = maybe_show_splash ();

		dcpomatic_setup_path_encoding ();
		dcpomatic_setup_i18n ();
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

#ifdef DCPOMATIC_LINUX
		StatusDialog* d = new StatusDialog ();
		d->Show ();
#else
		_icon = new TaskBarIcon;
#endif
		_thread = thread (bind (&App::main_thread, this));

		Bind (wxEVT_TIMER, boost::bind (&App::check, this));
		_timer.reset (new wxTimer (this));
		_timer->Start (1000);

		if (splash) {
			splash->Destroy ();
		}

		SetExitOnFrameDelete (false);

		return true;
	}

	int OnExit ()
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
			error_dialog (0, std_to_wx (e.what ()));
			wxTheApp->ExitMainLoop ();
		} catch (...) {
			error_dialog (0, _("An unknown error has occurred with the DCP-o-matic server."));
			wxTheApp->ExitMainLoop ();
		}
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	void config_failed_to_load ()
	{
		message_dialog (0, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
	}

	void config_warning (string m)
	{
		message_dialog (0, std_to_wx (m));
	}

	boost::thread _thread;
	TaskBarIcon* _icon;
	shared_ptr<wxTimer> _timer;
};

IMPLEMENT_APP (App)
