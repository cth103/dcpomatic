/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_button.h"
#include "timecode.h"
#include "wx_util.h"
#include "lib/util.h"


using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


TimecodeBase::TimecodeBase (wxWindow* parent, bool set_button)
	: wxPanel (parent)
	, _set_button (0)
{
	auto const s = TimecodeBase::size (parent);

	wxTextValidator validator (wxFILTER_INCLUDE_CHAR_LIST);
	wxArrayString list;

	auto n = char_to_wx("0123456789");
	for (size_t i = 0; i < n.Length(); ++i) {
		list.Add (n[i]);
	}

	validator.SetIncludes (list);

	_sizer = new wxBoxSizer (wxHORIZONTAL);

	_editable = new wxPanel (this);
	auto editable_sizer = new wxBoxSizer (wxHORIZONTAL);
	_controls.push_back(_hours = new wxTextCtrl(_editable, wxID_ANY, {}, wxDefaultPosition, s, 0, validator));
	_controls.push_back(_minutes = new wxTextCtrl(_editable, wxID_ANY, {}, wxDefaultPosition, s, 0, validator));
	_controls.push_back(_seconds = new wxTextCtrl(_editable, wxID_ANY, {}, wxDefaultPosition, s, 0, validator));
	_controls.push_back(_frames = new wxTextCtrl(_editable, wxID_ANY, {}, wxDefaultPosition, s, 0, validator));

	if (parent->GetLayoutDirection() == wxLayout_RightToLeft) {
		std::reverse(_controls.begin(), _controls.end());
	}

	for (auto i = _controls.begin(); i != _controls.end(); ++i) {
		(*i)->SetMaxLength(2);
		editable_sizer->Add(*i);
		if (std::next(i) != _controls.end()) {
			add_label_to_sizer(editable_sizer, _editable, char_to_wx(":"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
		}
	}

	if (set_button) {
		_set_button = new Button (_editable, _("Set"), wxDefaultPosition, small_button_size(parent, _("Set")));
		editable_sizer->Add (_set_button, 0, wxLEFT | wxRIGHT, 8);
	}
	_editable->SetSizerAndFit (editable_sizer);
	_sizer->Add (_editable);

	_fixed = add_label_to_sizer(_sizer, this, char_to_wx("42"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);

	for (auto control: _controls) {
		control->Bind(wxEVT_TEXT, boost::bind(&TimecodeBase::changed, this, _1));
	}
	if (_set_button) {
		_set_button->Bind (wxEVT_BUTTON, boost::bind (&TimecodeBase::set_clicked, this));
		_set_button->Enable (false);
	}

	set_editable (true);

	SetSizerAndFit (_sizer);
}

void
TimecodeBase::set_focus ()
{
	_hours->SetFocus ();
}

void
TimecodeBase::clear ()
{
	for (auto control: _controls) {
		checked_set(control, wxString{});
	}
	checked_set(_fixed, wxString{});
}

void
TimecodeBase::changed(wxCommandEvent& ev)
{
	if (_ignore_changed) {
		return;
	}

	if (_set_button) {
		_set_button->Enable(valid());
	}

	auto iter = std::find(_controls.begin(), _controls.end(), ev.GetEventObject());
	DCPOMATIC_ASSERT(iter != _controls.end());

	if ((*iter)->GetValue().Length() == 2) {
		auto next = std::next(iter);
		if (next != _controls.end()) {
			(*next)->SetFocus();
		}
	}
}

void
TimecodeBase::set_clicked ()
{
	Changed ();
	if (_set_button) {
		_set_button->Enable (false);
	}

	_ignore_changed = true;
	for (auto control: _controls) {
		if (control->GetValue().IsEmpty()) {
			control->SetValue(char_to_wx("0"));
		}
	}
	_ignore_changed = false;
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
#ifdef DCPOMATIC_OSX
	auto size = dc.GetTextExtent(char_to_wx("999"));
#else
	auto size = dc.GetTextExtent(char_to_wx("99999"));
#endif
	size.SetHeight (-1);
	return size;
}
