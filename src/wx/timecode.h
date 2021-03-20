/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/dcpomatic_time.h"
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
		auto const hmsf = t.split (fps);

		checked_set (_hours, dcp::raw_convert<std::string>(hmsf.h));
		checked_set (_minutes, dcp::raw_convert<std::string>(hmsf.m));
		checked_set (_seconds, dcp::raw_convert<std::string>(hmsf.s));
		checked_set (_frames, dcp::raw_convert<std::string>(hmsf.f));

		checked_set (_fixed, t.timecode (fps));
	}

	void set_hint (T t, float fps)
	{
		auto hmsf = t.split (fps);

		_hours->SetHint (std_to_wx(dcp::raw_convert<std::string>(hmsf.h)));
		_minutes->SetHint (std_to_wx(dcp::raw_convert<std::string>(hmsf.m)));
		_seconds->SetHint (std_to_wx(dcp::raw_convert<std::string>(hmsf.s)));
		_frames->SetHint (std_to_wx(dcp::raw_convert<std::string>(hmsf.f)));
	}

	dcpomatic::HMSF get () const
	{
		auto value_or_hint = [](wxTextCtrl const * t) {
			auto s = wx_to_std (t->GetValue().IsEmpty() ? t->GetHint() : t->GetValue());
			if (s.empty()) {
				return 0;
			}
			return dcp::raw_convert<int>(s);
		};

		return { value_or_hint(_hours),
			value_or_hint(_minutes),
			value_or_hint(_seconds),
			value_or_hint(_frames) };
	}

	T get (float fps) const
	{
		return T(get(), fps);
	}
};

#endif
