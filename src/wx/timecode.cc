/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <boost/lexical_cast.hpp>
#include "timecode.h"
#include "wx_util.h"

using std::string;
using boost::lexical_cast;

Timecode::Timecode (wxWindow* parent)
	: wxPanel (parent)
{
	wxClientDC dc (parent);
	wxSize size = dc.GetTextExtent (wxT ("9999"));
	size.SetHeight (-1);

	wxTextValidator validator (wxFILTER_INCLUDE_CHAR_LIST);
	wxArrayString list;

	string n = "0123456789";
	for (size_t i = 0; i < n.length(); ++i) {
		list.Add (n[i]);
	}

	validator.SetIncludes (list);
	
	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	_hours = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size, 0, validator);
	_hours->SetMaxLength (2);
	sizer->Add (_hours);
	add_label_to_sizer (sizer, this, ":");
	_minutes = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	_minutes->SetMaxLength (2);
	sizer->Add (_minutes);
	add_label_to_sizer (sizer, this, ":");
	_seconds = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	_seconds->SetMaxLength (2);
	sizer->Add (_seconds);
	add_label_to_sizer (sizer, this, ".");
	_frames = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	_frames->SetMaxLength (2);
	sizer->Add (_frames);

	SetSizerAndFit (sizer);
}

void
Timecode::set (Time t, int fps)
{
	int const h = t / (3600 * TIME_HZ);
	t -= h * 3600 * TIME_HZ;
	int const m = t / (60 * TIME_HZ);
	t -= m * 60 * TIME_HZ;
	int const s = t / TIME_HZ;
	t -= s * TIME_HZ;
	int const f = t * fps / TIME_HZ;

	_hours->SetValue (wxString::Format ("%02d", h));
	_minutes->SetValue (wxString::Format ("%02d", m));
	_seconds->SetValue (wxString::Format ("%02d", s));
	_frames->SetValue (wxString::Format ("%02d", f));
}

Time
Timecode::get (int fps) const
{
	Time t = 0;
	t += lexical_cast<int> (wx_to_std (_hours->GetValue())) * 3600 * TIME_HZ;
	t += lexical_cast<int> (wx_to_std (_minutes->GetValue())) * 60 * TIME_HZ;
	t += lexical_cast<int> (wx_to_std (_seconds->GetValue())) * TIME_HZ;
	t += lexical_cast<int> (wx_to_std (_frames->GetValue())) * TIME_HZ / fps;
	return t;
}
