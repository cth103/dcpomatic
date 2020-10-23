/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "custom_scale_dialog.h"
#include "wx_util.h"
#include "lib/util.h"
#include <dcp/raw_convert.h>
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
#include <wx/propgrid/property.h>
#include <wx/propgrid/props.h>
DCPOMATIC_ENABLE_WARNINGS


using boost::optional;
using namespace dcp;


CustomScaleDialog::CustomScaleDialog (wxWindow* parent, dcp::Size initial, dcp::Size film_container, optional<float> custom_ratio, optional<dcp::Size> custom_size)
	: TableDialog (parent, _("Custom scale"), 3, 1, true)
	, _film_container (film_container)
{
	_ratio_to_fit = new wxRadioButton (this, wxID_ANY, _("Set ratio and fit to DCP container"));
	add (_ratio_to_fit);
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_ratio = new wxTextCtrl (this, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0, wxNumericPropertyValidator(wxNumericPropertyValidator::Float));
	s->Add (_ratio, 1, wxRIGHT, 4);
	add_label_to_sizer (s, this, wxT(":1"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	add (s);
	_size_from_ratio = new wxStaticText (this, wxID_ANY, wxT(""));
	add (_size_from_ratio, 1, wxALIGN_CENTER_VERTICAL);

#ifdef __WXGTK3__
	int const spin_width = 118;
#else
	int const spin_width = 64;
#endif

	_size = new wxRadioButton (this, wxID_ANY, _("Set size"));
	add (_size);
	s = new wxBoxSizer (wxHORIZONTAL);
	_width = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(spin_width, -1), wxSP_ARROW_KEYS, 1, film_container.width);
	s->Add (_width, 1, wxRIGHT, 4);
	add_label_to_sizer (s, this, wxT("x"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_height = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(spin_width, -1), wxSP_ARROW_KEYS, 1, film_container.height);
	s->Add (_height, 1, wxRIGHT, 4);
	add (s);
	_ratio_from_size = new wxStaticText (this, wxID_ANY, wxT(""));
	add (_ratio_from_size, 1, wxALIGN_CENTER_VERTICAL);

	if (custom_ratio) {
		_ratio_to_fit->SetValue (true);
		_size->SetValue (false);
		_ratio->SetValue (wxString::Format("%.2f", *custom_ratio));
		_width->SetValue (initial.width);
		_height->SetValue (initial.height);
	} else if (custom_size) {
		_ratio_to_fit->SetValue (false);
		_size->SetValue (true);
		_ratio->SetValue (wxString::Format("%.2f", initial.ratio()));
		_width->SetValue (custom_size->width);
		_height->SetValue (custom_size->height);
	} else {
		_ratio_to_fit->SetValue (true);
		_size->SetValue (false);
		_ratio->SetValue (wxString::Format("%.2f", initial.ratio()));
		_width->SetValue (initial.width);
		_height->SetValue (initial.height);
	}

	setup_sensitivity ();
	update_size_from_ratio ();
	update_ratio_from_size ();

	layout ();

	_ratio_to_fit->Bind (wxEVT_RADIOBUTTON, boost::bind(&CustomScaleDialog::setup_sensitivity, this));
	_ratio->Bind (wxEVT_TEXT, boost::bind(&CustomScaleDialog::update_size_from_ratio, this));
	_size->Bind (wxEVT_RADIOBUTTON, boost::bind(&CustomScaleDialog::setup_sensitivity, this));
	_width->Bind (wxEVT_TEXT, boost::bind(&CustomScaleDialog::update_ratio_from_size, this));
	_height->Bind (wxEVT_TEXT, boost::bind(&CustomScaleDialog::update_ratio_from_size, this));
}


void
CustomScaleDialog::update_size_from_ratio ()
{
	dcp::Size const s = fit_ratio_within (raw_convert<float>(wx_to_std(_ratio->GetValue())), _film_container);
	_size_from_ratio->SetLabelMarkup (wxString::Format("<i>%dx%d</i>", s.width, s.height));
}


void
CustomScaleDialog::update_ratio_from_size ()
{
	float const ratio = _height->GetValue() > 0 ? (float(_width->GetValue()) / _height->GetValue()) : 2;
	_ratio_from_size->SetLabelMarkup (wxString::Format("<i>%.2f:1</i>", ratio));
}


void
CustomScaleDialog::setup_sensitivity ()
{
	_ratio->Enable (_ratio_to_fit->GetValue());
	_size_from_ratio->Enable (_ratio_to_fit->GetValue());
	_width->Enable (_size->GetValue());
	_height->Enable (_size->GetValue());
	_ratio_from_size->Enable (_size->GetValue());
}


optional<float>
CustomScaleDialog::custom_ratio () const
{
	if (!_ratio_to_fit->GetValue()) {
		return optional<float>();
	}

	return raw_convert<float>(wx_to_std(_ratio->GetValue()));
}


optional<dcp::Size>
CustomScaleDialog::custom_size () const
{
	if (!_size->GetValue()) {
		return optional<dcp::Size>();
	}

	return dcp::Size (_width->GetValue(), _height->GetValue());
}

