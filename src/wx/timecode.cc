/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "lib/util.h"
#include "timecode.h"
#include "wx_util.h"
#include <boost/lexical_cast.hpp>

using std::string;
using std::cout;
using boost::lexical_cast;

TimecodeBase::TimecodeBase (wxWindow* parent)
	: wxPanel (parent)
{
	wxSize const s = TimecodeBase::size (parent);
	
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
	_hours = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, s, 0, validator);
	_hours->SetMaxLength (2);
	editable_sizer->Add (_hours);
	add_label_to_sizer (editable_sizer, _editable, wxT (":"), false);
	_minutes = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, s, 0, validator);
	_minutes->SetMaxLength (2);
	editable_sizer->Add (_minutes);
	add_label_to_sizer (editable_sizer, _editable, wxT (":"), false);
	_seconds = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, s, 0, validator);
	_seconds->SetMaxLength (2);
	editable_sizer->Add (_seconds);
	add_label_to_sizer (editable_sizer, _editable, wxT (":"), false);
	_frames = new wxTextCtrl (_editable, wxID_ANY, wxT(""), wxDefaultPosition, s, 0, validator);
	_frames->SetMaxLength (2);
	editable_sizer->Add (_frames);
	_set_button = new wxButton (_editable, wxID_ANY, _("Set"));
	editable_sizer->Add (_set_button, 0, wxLEFT | wxRIGHT, 8);
	_editable->SetSizerAndFit (editable_sizer);
	_sizer->Add (_editable);

	_fixed = add_label_to_sizer (_sizer, this, wxT ("42"), false);
	
	_hours->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&TimecodeBase::changed, this));
	_minutes->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&TimecodeBase::changed, this));
	_seconds->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&TimecodeBase::changed, this));
	_frames->Bind	  (wxEVT_COMMAND_TEXT_UPDATED,   boost::bind (&TimecodeBase::changed, this));
	_set_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&TimecodeBase::set_clicked, this));

	_set_button->Enable (false);

	set_editable (true);

	SetSizerAndFit (_sizer);
}

void
TimecodeBase::clear ()
{
	checked_set (_hours, wxT (""));
	checked_set (_minutes, wxT (""));
	checked_set (_seconds, wxT (""));
	checked_set (_frames, wxT (""));
	checked_set (_fixed, wxT (""));
}

void
TimecodeBase::changed ()
{
	_set_button->Enable (true);
}

void
TimecodeBase::set_clicked ()
{
	Changed ();
	_set_button->Enable (false);
}

void
TimecodeBase::set_editable (bool e)
{
	_editable->Show (e);
	_fixed->Show (!e);
	_sizer->Layout ();
}

wxSize
TimecodeBase::size (wxWindow* parent)
{
	wxClientDC dc (parent);
	wxSize size = dc.GetTextExtent (wxT ("9999"));
	size.SetHeight (-1);
	return size;
}


