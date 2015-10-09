/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "wx/wx_util.h"
#include "wx/wx_signal_manager.h"
#include "lib/util.h"
#include "lib/server.h"
#include "lib/config.h"
#include "lib/log.h"
#include "lib/signaller.h"
#include <wx/taskbar.h>
#include <wx/icon.h>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using std::string;
using std::exception;
using std::list;
using boost::shared_ptr;
using boost::thread;
using boost::bind;

enum {
	ID_status = 1,
	ID_quit,
	ID_timer
};

static int const log_lines = 32;

class MemoryLog : public Log, public Signaller
{
public:

	string get () const {
		string a;
		BOOST_FOREACH (string const & i, _log) {
			a += i + "\n";
		}
		return a;
	}

	string head_and_tail (int) const {
		/* Not necessary */
		return "";
	}

	boost::signals2::signal<void(string)> Appended;
	boost::signals2::signal<void(int)> Removed;

private:
	void do_log (string m)
	{
		_log.push_back (m);
		emit (boost::bind (boost::ref (Appended), m));
		if (_log.size() > log_lines) {
			emit (boost::bind (boost::ref (Removed), _log.front().length()));
			_log.pop_front ();
		}
	}

	list<string> _log;
};

static shared_ptr<MemoryLog> memory_log (new MemoryLog);

class StatusDialog : public wxDialog
{
public:
	StatusDialog ()
		: wxDialog (
			0, wxID_ANY, _("DCP-o-matic encode server"),
			wxDefaultPosition, wxDefaultSize,
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
			)
	{
		_sizer = new wxFlexGridSizer (1, DCPOMATIC_SIZER_GAP, DCPOMATIC_SIZER_GAP);
		_sizer->AddGrowableCol (0, 1);

		wxClientDC dc (this);
		wxSize size = dc.GetTextExtent (wxT ("This is the length of the file label it should be quite long"));
		int const height = (size.GetHeight() + 2) * log_lines;
		SetSize (700, height + DCPOMATIC_SIZER_GAP * 2);

		_text = new wxTextCtrl (
			this, wxID_ANY, std_to_wx (memory_log->get()), wxDefaultPosition, wxSize (-1, height),
			wxTE_READONLY | wxTE_MULTILINE
			);

		_sizer->Add (_text, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

		SetSizer (_sizer);
		_sizer->Layout ();

		memory_log->Appended.connect (bind (&StatusDialog::appended, this, _1));
		memory_log->Removed.connect (bind (&StatusDialog::removed, this, _1));
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

	wxFlexGridSizer* _sizer;
	wxTextCtrl* _text;
};

class TaskBarIcon : public wxTaskBarIcon
{
public:
	TaskBarIcon ()
		: _status (0)
	{
#ifdef DCPOMATIC_WINDOWS
		wxIcon icon (std_to_wx ("taskbar_icon"));
#else
		wxInitAllImageHandlers();
		wxBitmap bitmap (wxString::Format (wxT ("%s/dcpomatic2_server_small.png"), LINUX_SHARE_PREFIX), wxBITMAP_TYPE_PNG);
		wxIcon icon;
		icon.CopyFromBitmap (bitmap);
#endif

		SetIcon (icon, std_to_wx ("DCP-o-matic encode server"));

		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&TaskBarIcon::status, this), ID_status);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&TaskBarIcon::quit, this), ID_quit);
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
		, _thread (0)
		, _icon (0)
	{}

private:

	bool OnInit ()
	{
		if (!wxApp::OnInit ()) {
			return false;
		}

		dcpomatic_setup_path_encoding ();
		dcpomatic_setup_i18n ();
		dcpomatic_setup ();
		Config::drop ();

		signal_manager = new wxSignalManager (this);
		Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		_icon = new TaskBarIcon;
		_thread = new thread (bind (&App::main_thread, this));

		Bind (wxEVT_TIMER, boost::bind (&App::check, this));
		_timer.reset (new wxTimer (this));
		_timer->Start (1000);

		return true;
	}

	int OnExit ()
	{
		delete _icon;
		return wxApp::OnExit ();
	}

	void main_thread ()
	try {
		Server server (memory_log, false);
		server.run (Config::instance()->num_local_encoding_threads ());
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

	boost::thread* _thread;
	TaskBarIcon* _icon;
	shared_ptr<wxTimer> _timer;
};

IMPLEMENT_APP (App)
