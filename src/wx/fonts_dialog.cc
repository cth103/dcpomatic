/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "fonts_dialog.h"
#include "wx_util.h"
#include "system_font_dialog.h"
#include "font_files_dialog.h"
#include "lib/font.h"
#include "lib/content.h"
#include "lib/subtitle_content.h"
#include <wx/wx.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::string;
using std::cout;
using boost::shared_ptr;

FontsDialog::FontsDialog (wxWindow* parent, shared_ptr<Content> content)
	: wxDialog (parent, wxID_ANY, _("Fonts"))
	, _content (content)
{
	_fonts = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (550, 200), wxLC_REPORT | wxLC_SINGLE_SEL);

	{
		wxListItem ip;
		ip.SetId (0);
		ip.SetText (_("ID"));
		ip.SetWidth (100);
		_fonts->InsertColumn (0, ip);
	}

	{
		wxListItem ip;
		ip.SetId (1);
		ip.SetText (_("Normal file"));
		ip.SetWidth (150);
		_fonts->InsertColumn (1, ip);
	}

	{
		wxListItem ip;
		ip.SetId (2);
		ip.SetText (_("Italic file"));
		ip.SetWidth (150);
		_fonts->InsertColumn (2, ip);
	}

	{
		wxListItem ip;
		ip.SetId (3);
		ip.SetText (_("Bold file"));
		ip.SetWidth (150);
		_fonts->InsertColumn (3, ip);
	}

	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	sizer->Add (_fonts, 1, wxEXPAND | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);

	_edit = new wxButton (this, wxID_ANY, _("Edit..."));
	sizer->Add (_edit, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	_edit->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&FontsDialog::edit_clicked, this));
	_fonts->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&FontsDialog::selection_changed, this));
	_fonts->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&FontsDialog::selection_changed, this));

	setup ();
}

void
FontsDialog::setup ()
{
	shared_ptr<Content> content = _content.lock ();
	if (!content) {
		return;
	}

	_fonts->DeleteAllItems ();
	size_t n = 0;
	BOOST_FOREACH (shared_ptr<Font> i, content->subtitle->fonts ()) {
		wxListItem item;
		item.SetId (n);
		_fonts->InsertItem (item);
		_fonts->SetItem (n, 0, std_to_wx (i->id ()));
		if (i->file(FontFiles::NORMAL)) {
			_fonts->SetItem (n, 1, i->file(FontFiles::NORMAL).get().leaf().string ());
		}
		++n;
	}

	setup_sensitivity ();
}

void
FontsDialog::selection_changed ()
{
	setup_sensitivity ();
}

void
FontsDialog::setup_sensitivity ()
{
	int const item = _fonts->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	_edit->Enable (item != -1);
}

void
FontsDialog::edit_clicked ()
{
	shared_ptr<Content> content = _content.lock ();
	if (!content) {
		return;
	}

	int const item = _fonts->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	string const id = wx_to_std (_fonts->GetItemText (item, 0));
	shared_ptr<Font> font;
	BOOST_FOREACH (shared_ptr<Font> i, content->subtitle->fonts()) {
		if (i->id() == id) {
			font = i;
		}
	}

	if (!font) {
		return;
	}

	FontFilesDialog* d = new FontFilesDialog (this, font->files ());
	if (d->ShowModal () == wxID_OK) {
		font->set_files (d->get ());
	}
	d->Destroy ();

	setup ();
}
