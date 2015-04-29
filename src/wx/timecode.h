/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_WX_TIMECODE_H
#define DCPOMATIC_WX_TIMECODE_H

#include "wx_util.h"
#include "lib/types.h"
#include <wx/wx.h>
#include <boost/signals2.hpp>
#include <boost/lexical_cast.hpp>

class TimecodeBase : public wxPanel
{
public:
	TimecodeBase (wxWindow *);

	void clear ();

	void set_editable (bool);

	boost::signals2::signal<void ()> Changed;

	static wxSize size (wxWindow* parent);

protected:
	void changed ();
	void set_clicked ();
	
	wxSizer* _sizer;
	wxPanel* _editable;
	wxTextCtrl* _hours;
	wxTextCtrl* _minutes;
	wxTextCtrl* _seconds;
	wxTextCtrl* _frames;
	wxButton* _set_button;
	wxStaticText* _fixed;
};

template <class T>
class Timecode : public TimecodeBase
{
public:
	Timecode (wxWindow* parent)
		: TimecodeBase (parent)
	{

	}

	void set (T t, float fps)
	{
		int h;
		int m;
		int s;
		int f;
		t.split (fps, h, m, s, f);
		
		checked_set (_hours, boost::lexical_cast<std::string> (h));
		checked_set (_minutes, boost::lexical_cast<std::string> (m));
		checked_set (_seconds, boost::lexical_cast<std::string> (s));
		checked_set (_frames, boost::lexical_cast<std::string> (f));
		
		checked_set (_fixed, t.timecode (fps));
	}

	T get (int fps) const
	{
		T t;
		std::string const h = wx_to_std (_hours->GetValue ());
		t += T::from_seconds (boost::lexical_cast<int> (h.empty() ? "0" : h) * 3600);
		std::string const m = wx_to_std (_minutes->GetValue());
		t += T::from_seconds (boost::lexical_cast<int> (m.empty() ? "0" : m) * 60);
		std::string const s = wx_to_std (_seconds->GetValue());
		t += T::from_seconds (boost::lexical_cast<int> (s.empty() ? "0" : s));
		std::string const f = wx_to_std (_frames->GetValue());
		t += T::from_seconds (boost::lexical_cast<double> (f.empty() ? "0" : f) / fps);
		
		return t;
	}
};

#endif
