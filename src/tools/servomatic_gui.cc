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
#include "wx_util.h"
#include "lib/util.h"
#include "lib/server.h"

using namespace boost;

enum {
	ID_status = 1,
	ID_quit
};

class StatusDialog : public wxDialog
{
public:
	StatusDialog ()
		: wxDialog (0, wxID_ANY, _("DVD-o-matic encode server"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
	{
		wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);
		table->AddGrowableCol (1, 1);

		add_label_to_sizer (table, this, "Hello");

		SetSizer (table);
		table->Layout ();
		table->SetSizeHints (this);
	}
};

class TaskBarIcon : public wxTaskBarIcon
{
public:
	TaskBarIcon ()
	{
		wxIcon icon (std_to_wx ("taskbar_icon"));
		SetIcon (icon, std_to_wx ("DVD-o-matic encode server"));

		Connect (ID_status, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (TaskBarIcon::status));
		Connect (ID_quit, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (TaskBarIcon::quit));
	}
	
	wxMenu* CreatePopupMenu ()
	{
		wxMenu* menu = new wxMenu;
		menu->Append (ID_status, std_to_wx ("Status..."));
		menu->Append (ID_quit, std_to_wx ("Quit"));
		return menu;
	}

private:
	void status (wxCommandEvent &)
	{
		StatusDialog* d = new StatusDialog;
		d->Show ();
	}

	void quit (wxCommandEvent &)
	{
		wxTheApp->ExitMainLoop ();
	}
};

class App : public wxApp
{
public:
	App ()
		: wxApp ()
		, _thread (0)
	{}

private:	
	
	bool OnInit ()
	{
		dvdomatic_setup ();

		new TaskBarIcon;

		_thread = new thread (bind (&App::main_thread, this));
		return true;
	}

	void main_thread ()
	{
		Server server;
		server.run ();
	}

	boost::thread* _thread;
};

IMPLEMENT_APP (App)
