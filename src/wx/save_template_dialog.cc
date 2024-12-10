/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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
#include "save_template_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/constants.h"


using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


using boost::optional;


SaveTemplateDialog::SaveTemplateDialog (wxWindow* parent)
	: TableDialog (parent, _("Save template"), 2, 1, true)
{
	_default = add(new wxRadioButton(this, wxID_ANY, _("Save as default")));
	add_spacer();
	_existing = add(new wxRadioButton(this, wxID_ANY, _("Save over existing template")));
	_existing_name = add(new Choice(this));
	_new = add(new wxRadioButton(this, wxID_ANY, _("Save as new with name")));
	_new_name = add(new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1)));

	_default->SetFocus ();
	layout ();

	auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	ok->Bind (wxEVT_BUTTON, boost::bind(&SaveTemplateDialog::check, this, _1));

	_default->Bind(wxEVT_RADIOBUTTON, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));
	_existing->Bind(wxEVT_RADIOBUTTON, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));
	_new->Bind(wxEVT_RADIOBUTTON, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));
	_new_name->Bind(wxEVT_TEXT, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));

	setup_sensitivity ();

	for (auto name: Config::instance()->templates()) {
		_existing_name->add_entry(name);
	}
}


void
SaveTemplateDialog::setup_sensitivity ()
{
	auto const have_templates = !Config::instance()->templates().empty();
	_existing->Enable(have_templates);
	_existing_name->Enable(have_templates && _existing->GetValue());
	_new_name->Enable(_new->GetValue());

	auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	if (ok) {
		ok->Enable(_default->GetValue() || (_existing->GetValue() && have_templates) || (_new->GetValue() && !_new_name->GetValue().IsEmpty()));
	}
}


optional<string>
SaveTemplateDialog::name () const
{
	if (_default->GetValue()) {
		return {};
	} else if (_existing->GetValue()) {
		DCPOMATIC_ASSERT(_existing_name->get());
		auto index = *_existing_name->get();
		auto templates = Config::instance()->templates();
		DCPOMATIC_ASSERT(index < int(templates.size()));
		return templates[index];
	} else {
		return wx_to_std(_new_name->GetValue());
	}
}


void
SaveTemplateDialog::check (wxCommandEvent& ev)
{
	bool ok = true;

	if (_new->GetValue() && Config::instance()->existing_template(wx_to_std(_new_name->GetValue()))) {
		ok = confirm_dialog (this, _("There is already a template with this name.  Do you want to overwrite it?"));
	}

	if (ok) {
		ev.Skip ();
	}
}
