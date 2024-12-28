/*
    Copyright (C) 2015-2022 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "kdm_choice.h"
#include "name_format_editor.h"
#include "short_kdm_output_panel.h"
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#else
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
LIBDCP_ENABLE_WARNINGS
#endif
#include <wx/notebook.h>
#include <wx/sizer.h>


ShortKDMOutputPanel::ShortKDMOutputPanel(wxWindow* parent)
	: KDMOutputPanel(parent)
{
	auto notebook = new wxNotebook(this, wxID_ANY);

	auto destination = new wxPanel(notebook, wxID_ANY);
	create_destination_widgets(destination);
	fill_destination_panel(destination);
	/// TRANSLATORS: Destination here refers to where some KDMs will be written or sent
	/// (a place on disk or some email addresses).
	notebook->AddPage(destination, _("Destination"), true);

	auto details = new wxPanel(notebook, wxID_ANY);
	create_details_widgets(details);
	fill_details_panel(details);
	notebook->AddPage(details, _("Details"));

	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(notebook, 1, wxEXPAND);
	SetSizer(sizer);
}


void
ShortKDMOutputPanel::fill_destination_panel(wxPanel* panel)
{
	auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, 0);
	table->AddGrowableCol(1);

	table->Add(_write_to, 1, wxEXPAND);
	table->Add(_folder, 1, wxEXPAND);

	auto write_options = new wxBoxSizer(wxVERTICAL);
	write_options->Add(_write_flat, 1, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	write_options->Add(_write_folder, 1, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	write_options->Add(_write_zip, 1, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	table->AddSpacer(0);
	table->Add(write_options);

	table->Add(_email, 1, wxEXPAND);
	table->Add(_add_email_addresses);

	auto padding = new wxBoxSizer(wxVERTICAL);
	padding->Add(table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	panel->SetSizer(padding);
}


void
ShortKDMOutputPanel::fill_details_panel(wxPanel* panel)
{
	auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, 0);
	table->AddGrowableCol(1);

	add_label_to_sizer(table, panel, _("KDM type"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);

	auto type = new wxBoxSizer(wxHORIZONTAL);
	type->Add(_type, 1, wxTOP, DCPOMATIC_CHOICE_TOP_PAD);
	type->Add(_advanced, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	table->Add(type, 1, wxTOP, DCPOMATIC_CHOICE_TOP_PAD);

	add_label_to_sizer(table, panel, _("Annotation text"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	table->Add(_annotation_text, 1, wxEXPAND);

	add_label_to_sizer(table, panel, _("Folder / ZIP name format"), true, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT);
	table->Add(_container_name_format->panel(), 1, wxEXPAND);

	auto format = create_label(panel, _("Filename format"), true);
	auto align = new wxBoxSizer(wxHORIZONTAL);
#ifdef DCPOMATIC_OSX
	align->Add(format, 0, wxTOP, 2);
	table->Add(align, 0, wxALIGN_RIGHT | wxRIGHT, DCPOMATIC_SIZER_GAP - 2);
#else
	align->Add(format, 0, wxLEFT, DCPOMATIC_SIZER_GAP);
	table->Add(align, 0, wxTOP | wxRIGHT | wxALIGN_TOP, DCPOMATIC_SIZER_GAP);
#endif
	table->Add(_filename_format->panel(), 1, wxEXPAND);

	auto padding = new wxBoxSizer(wxVERTICAL);
	padding->Add(table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	panel->SetSizer(padding);
}
