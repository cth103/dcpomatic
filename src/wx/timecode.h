/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_WX_TIMECODE_H
#define DCPOMATIC_WX_TIMECODE_H

#include "wx_util.h"
#include "lib/types.h"
#include <dcp/raw_convert.h>
#include <wx/wx.h>
#include <boost/signals2.hpp>

class TimecodeBase : public wxPanel
{
public:
	TimecodeBase (wxWindow *, bool set_button);

	void clear ();

	void set_editable (bool);
	void set_focus ();

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
	Timecode (wxWindow* parent, bool set_button = true)
		: TimecodeBase (parent, set_button)
	{

	}

	void set (T t, float fps)
	{
		int h;
		int m;
		int s;
		int f;
		t.split (fps, h, m, s, f);

		checked_set (_hours, dcp::raw_convert<std::string>(h));
		checked_set (_minutes, dcp::raw_convert<std::string>(m));
		checked_set (_seconds, dcp::raw_convert<std::string>(s));
		checked_set (_frames, dcp::raw_convert<std::string>(f));

		checked_set (_fixed, t.timecode (fps));
	}

	void set_hint (T t, float fps)
	{
		int h;
		int m;
		int s;
		int f;
		t.split (fps, h, m, s, f);

		_hours->SetHint (std_to_wx(dcp::raw_convert<std::string>(h)));
		_minutes->SetHint (std_to_wx(dcp::raw_convert<std::string>(m)));
		_seconds->SetHint (std_to_wx(dcp::raw_convert<std::string>(s)));
		_frames->SetHint (std_to_wx(dcp::raw_convert<std::string>(f)));
	}

	T get (float fps) const
	{
		T t;

		auto value_or_hint = [](wxTextCtrl const * t) {
			if (!t->GetValue().IsEmpty()) {
				return wx_to_std (t->GetValue());
			} else {
				return wx_to_std (t->GetHint());
			}
		};

		std::string const h = value_or_hint (_hours);
		t += T::from_seconds (dcp::raw_convert<int>(h.empty() ? "0" : h) * 3600);
		std::string const m = value_or_hint (_minutes);
		t += T::from_seconds (dcp::raw_convert<int>(m.empty() ? "0" : m) * 60);
		std::string const s = value_or_hint (_seconds);
		t += T::from_seconds (dcp::raw_convert<int>(s.empty() ? "0" : s));
		std::string const f = value_or_hint (_frames);
		t += T::from_seconds (dcp::raw_convert<double>(f.empty() ? "0" : f) / fps);

		return t;
	}
};

#endif
