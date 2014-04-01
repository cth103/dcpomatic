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
#include "lib/util.h"
#include "timecode.h"
#include "wx_util.h"

using std::string;
using std::cout;
using boost::lexical_cast;

Timecode::Timecode (wxWindow* parent)
	: wxPanel (parent)
{
	wxClientDC dc (parent);
	wxSize size = dc.GetTextExtent (wxT ("9999"));
	size.SetHeight (-1);

	wxTextValidator validator (wxFILTER_INCLUDE_CHAR_LIST);
	wxArrayString list;

	wxString n (wxT ("0123456789"));
	for (size_t i = 0; i < n.Length(); ++i) {
		list.Add (n[i]);
	}

	validator.SetIncludes (list);

	_sizer = new wxBoxSizer (wxHORIZONTAL);
	
	_editable = new wxPanel (this);
	wxSizer* editable_sizer = new wxBoxSizer (wxHORIZONTAL);
	_hours = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, size, 0, validator);
	_hours->SetMaxLength (2);
	editable_sizer->Add (_hours);
	add_label_to_sizer (editable_sizer, _editable, wxT (":"), false);
	_minutes = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, size, 0, validator);
	_minutes->SetMaxLength (2);
	editable_sizer->Add (_minutes);
	add_label_to_sizer (editable_sizer, _editable, wxT (":"), false);
	_seconds = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, size, 0, validator);
	_seconds->SetMaxLength (2);
	editable_sizer->Add (_seconds);
	add_label_to_sizer (editable_sizer, _editable, wxT (":"), false);
	_frames = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, size, 0, validator);
	_frames->SetMaxLength (2);
	editable_sizer->Add (_frames);
	_set_button = new wxButton (_editable, wxID_ANY, _("Set"));
	editable_sizer->Add (_set_button, 0, wxLEFT | wxRIGHT, 8);
	_editable->SetSizerAndFit (editable_sizer);
	_sizer->Add (_editable);

	_fixed = add_label_to_sizer (_sizer, this, wxT ("42"), false);
	
	_hours->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&Timecode::changed, this));
	_minutes->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&Timecode::changed, this));
	_seconds->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&Timecode::changed, this));
	_frames->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&Timecode::changed, this));
	_set_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&Timecode::set_clicked, this));

	_set_button->Enable (false);

	set_editable (true);

	SetSizerAndFit (_sizer);
}

void
Timecode::set (DCPTime t, int fps)
{
	/* Do this calculation with frames so that we can round
	   to a frame boundary at the start rather than the end.
	*/
	int64_t f = rint (t.seconds() * fps);
	
	int const h = f / (3600 * fps);
	f -= h * 3600 * fps;
	int const m = f / (60 * fps);
	f -= m * 60 * fps;
	int const s = f / fps;
	f -= s * fps;

	checked_set (_hours, lexical_cast<string> (h));
	checked_set (_minutes, lexical_cast<string> (m));
	checked_set (_seconds, lexical_cast<string> (s));
	checked_set (_frames, lexical_cast<string> (f));

	_fixed->SetLabel (wxString::Format ("%02d:%02d:%02d.%02" wxLongLongFmtSpec "d", h, m, s, f));
}

DCPTime
Timecode::get (int fps) const
{
	DCPTime t;
	string const h = wx_to_std (_hours->GetValue ());
	t += DCPTime::from_seconds (lexical_cast<int> (h.empty() ? "0" : h) * 3600);
	string const m = wx_to_std (_minutes->GetValue());
	t += DCPTime::from_seconds (lexical_cast<int> (m.empty() ? "0" : m) * 60);
	string const s = wx_to_std (_seconds->GetValue());
	t += DCPTime::from_seconds (lexical_cast<int> (s.empty() ? "0" : s));
	string const f = wx_to_std (_frames->GetValue());
	t += DCPTime::from_seconds (lexical_cast<double> (f.empty() ? "0" : f) / fps);

	return t;
}

void
Timecode::changed ()
{
	_set_button->Enable (true);
}

void
Timecode::set_clicked ()
{
	Changed ();
	_set_button->Enable (false);
}

void
Timecode::set_editable (bool e)
{
	_editable->Show (e);
	_fixed->Show (!e);
	_sizer->Layout ();
}
