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

/** @file src/wx/wx_util.cc
 *  @brief Some utility functions and classes.
 */

#include <boost/thread.hpp>
#include "wx_util.h"

using namespace std;
using namespace boost;

wxStaticText *
add_label_to_sizer (wxSizer* s, wxWindow* p, string t, int prop)
{
	wxStaticText* m = new wxStaticText (p, wxID_ANY, std_to_wx (t));
	s->Add (m, prop, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	return m;
}

void
error_dialog (wxWindow* parent, string m)
{
	wxMessageDialog* d = new wxMessageDialog (parent, std_to_wx (m), wxT ("DVD-o-matic"), wxOK);
	d->ShowModal ();
	d->Destroy ();
}

string
wx_to_std (wxString s)
{
	return string (s.mb_str ());
}

wxString
std_to_wx (string s)
{
	return wxString (s.c_str(), wxConvUTF8);
}

int const ThreadedStaticText::_update_event_id = 10000;

/** @param parent Parent for the wxStaticText.
 *  @param initial Initial text for the wxStaticText while the computation is being run.
 *  @param fn Function which works out what the wxStaticText content should be and returns it.
 */
ThreadedStaticText::ThreadedStaticText (wxWindow* parent, string initial, function<string ()> fn)
	: wxStaticText (parent, wxID_ANY, std_to_wx (initial))
{
	Connect (_update_event_id, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (ThreadedStaticText::thread_finished), 0, this);
	_thread = new thread (bind (&ThreadedStaticText::run, this, fn));
}

ThreadedStaticText::~ThreadedStaticText ()
{
	/* XXX: this is a bit unfortunate */
	_thread->join ();
	delete _thread;
}

void
ThreadedStaticText::run (function<string ()> fn)
{
	/* Run the thread and post the result to the GUI thread via AddPendingEvent */
	wxCommandEvent ev (wxEVT_COMMAND_TEXT_UPDATED, _update_event_id);
	ev.SetString (std_to_wx (fn ()));
	GetEventHandler()->AddPendingEvent (ev);
}

void
ThreadedStaticText::thread_finished (wxCommandEvent& ev)
{
	SetLabel (ev.GetString ());
}
