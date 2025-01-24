/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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


#include "gain_calculator_dialog.h"
#include "wx_util.h"
#include "lib/cinema_sound_processor.h"


using boost::optional;


GainCalculatorDialog::GainCalculatorDialog(wxWindow* parent)
	: TableDialog(parent, _("Gain Calculator"), 2, 1, true)
{
	add(_("Sound processor"), true);
	_processor = add(new wxChoice(this, wxID_ANY));

	add(_("I want to play this back at fader"), true);
	_wanted = add(new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_NUMERIC)));

	add(_("But I have to use fader"), true);
	_actual = add(new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_NUMERIC)));

	for (auto i: CinemaSoundProcessor::all()) {
		_processor->Append(std_to_wx(i->name()));
	}

	_processor->SetSelection(0);

	layout();
}


optional<float>
GainCalculatorDialog::db_change() const
{
	if (_wanted->GetValue().IsEmpty() || _actual->GetValue().IsEmpty()) {
		return {};
	}

	auto relaxed_string_to_float = [](std::string s) {
		try {
			boost::algorithm::replace_all(s, ",", ".");
			return boost::lexical_cast<float>(s);
		} catch (boost::bad_lexical_cast &) {
			boost::algorithm::replace_all(s, ".", ",");
			return boost::lexical_cast<float>(s);
		}
	};

	return CinemaSoundProcessor::from_index(
		_processor->GetSelection())->db_for_fader_change(
			relaxed_string_to_float(wx_to_std(_wanted->GetValue())),
			relaxed_string_to_float(wx_to_std(_actual->GetValue()))
			);
}
