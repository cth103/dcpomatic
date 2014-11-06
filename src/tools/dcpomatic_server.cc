/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <boost/thread.hpp>
#include <wx/taskbar.h>
#include <wx/icon.h>
#include "wx/wx_util.h"
#include "lib/util.h"
#include "lib/server.h"
#include "lib/config.h"

using std::cout;
using std::string;
using std::exception;
using boost::shared_ptr;
using boost::thread;
using boost::bind;

enum {
	ID_status = 1,
	ID_quit,
	ID_timer
};

class MemoryLog : public Log
{
public:

	string get () const {
		boost::mutex::scoped_lock (_mutex);
		return _log;
	}

	string head_and_tail () const {
		if (_log.size () < 2048) {
			return _log;
		}

		return _log.substr (0, 1024) + _log.substr (_log.size() - 1025, 1024);
	}

private:
	void do_log (string m)
	{
		_log = m;
	}

	string _log;	
};

static shared_ptr<MemoryLog> memory_log (new MemoryLog);

class StatusDialog : public wxDialog
{
public:
	StatusDialog ()
		: wxDialog (0, wxID_ANY, _("DCP-o-matic encode server"), wxDefaultPosition, wxSize (600, 80), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
		, _timer (this, ID_timer)
	{
		_sizer = new wxFlexGridSizer (1, 6, 6);
		_sizer->AddGrowableCol (0, 1);

		_text = new wxTextCtrl (this, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
		_sizer->Add (_text, 1, wxEXPAND);

		SetSizer (_sizer);
		_sizer->Layout ();

		Bind (wxEVT_TIMER, boost::bind (&StatusDialog::update, this), ID_timer);
		_timer.Start (1000);
	}

private:
	void update ()
	{
		_text->ChangeValue (std_to_wx (memory_log->get ()));
		_sizer->Layout ();
	}

	wxFlexGridSizer* _sizer;
	wxTextCtrl* _text;
	wxTimer _timer;
};

class TaskBarIcon : public wxTaskBarIcon
{
public:
	TaskBarIcon ()
	{
#ifdef __WXMSW__		
		wxIcon icon (std_to_wx ("taskbar_icon"));
#endif
#ifdef __WXGTK__
		wxInitAllImageHandlers();
		wxBitmap bitmap (wxString::Format (wxT ("%s/taskbar_icon.png"), POSIX_ICON_PREFIX), wxBITMAP_TYPE_PNG);
		wxIcon icon;
		icon.CopyFromBitmap (bitmap);
#endif
#ifndef __WXOSX__
		/* XXX: fix this for OS X */
		SetIcon (icon, std_to_wx ("DCP-o-matic encode server"));
#endif		

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
		StatusDialog* d = new StatusDialog;
		d->Show ();
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
		, _thread (0)
		, _icon (0)
	{}

private:	
	
	bool OnInit ()
	{
		if (!wxApp::OnInit ()) {
			return false;
		}
		
		dcpomatic_setup ();

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

	boost::thread* _thread;
	TaskBarIcon* _icon;
	shared_ptr<wxTimer> _timer;
};

IMPLEMENT_APP (App)
