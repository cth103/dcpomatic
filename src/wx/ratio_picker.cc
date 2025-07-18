/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_choice.h"
#include "check_box.h"
#include "ratio_picker.h"
#include "wx_util.h"
#include "lib/ratio.h"
#include <dcp/locale_convert.h>
#include <dcp/scope_guard.h>


using boost::optional;


RatioPicker::RatioPicker(wxWindow* parent, optional<float> ratio)
	: wxPanel(parent, wxID_ANY)
{
	_enable = new CheckBox(parent, _("Crop output to"));
	_preset = new Choice(this);
	_custom = new wxTextCtrl(this, wxID_ANY);

	auto sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(_preset, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	sizer->Add(_custom, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	add_label_to_sizer(sizer, this, _(":1"), false, 0, wxALIGN_CENTER_VERTICAL);

	for (auto ratio: Ratio::all()) {
		_preset->add_entry(ratio.image_nickname(), ratio.id());
	}
	_preset->add_entry(_("Custom"), std::string("custom"));

	SetSizer(sizer);
	Layout();

	set(ratio);

	_enable->bind(&RatioPicker::enable_changed, this);
	_preset->bind(&RatioPicker::preset_changed, this);
	_custom->Bind(wxEVT_TEXT, boost::bind(&RatioPicker::custom_changed, this));

	setup_sensitivity();
}


void
RatioPicker::set(optional<float> ratio)
{
	_enable->set(static_cast<bool>(ratio));
	set_preset(ratio);
	set_custom(ratio);
}


void
RatioPicker::enable_changed()
{
	setup_sensitivity();
	if (_enable->get()) {
		Changed(1.85);
	} else {
		Changed({});
	}
}


void
RatioPicker::setup_sensitivity()
{
	_preset->Enable(_enable->get());
	_custom->Enable(_enable->get());
}


void
RatioPicker::preset_changed()
{
	if (!_enable->get() || _ignore_changes) {
		return;
	}

	optional<Ratio> ratio;

	if (auto data = _preset->get_data()) {
		ratio = Ratio::from_id_if_exists(*data);
	}

	optional<float> new_value;
	if (ratio) {
		new_value = ratio->ratio();
	} else {
		new_value = dcp::locale_convert<float>(wx_to_std(_custom->GetValue()));
	}

	set_custom(new_value);
	Changed(new_value);
}


void
RatioPicker::custom_changed()
{
	if (!_enable->get() || _ignore_changes) {
		return;
	}

	auto const new_value = dcp::locale_convert<float>(wx_to_std(_custom->GetValue()));
	set_preset(new_value);
	Changed(new_value);
}


void
RatioPicker::set_preset(optional<float> ratio)
{
	_ignore_changes = true;
	dcp::ScopeGuard sg = [this]() { _ignore_changes = false; };

	if (ratio) {
		auto preset = Ratio::from_ratio(*ratio);
		_preset->set_by_data(preset ? preset->id() : "custom");
	} else {
		_preset->set_by_data(std::string("185"));
	}
}


void
RatioPicker::set_custom(optional<float> ratio)
{
	_ignore_changes = true;
	dcp::ScopeGuard sg = [this]() { _ignore_changes = false; };

	if (ratio) {
		auto preset = Ratio::from_ratio(*ratio);
		_custom->SetValue(wxString::Format(char_to_wx("%.2f"), preset ? preset->ratio() : *ratio));
	} else {
		_custom->SetValue(_("1.85"));
	}
}
