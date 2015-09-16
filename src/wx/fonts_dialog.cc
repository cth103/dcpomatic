/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "fonts_dialog.h"
#include "wx_util.h"
#include "system_font_dialog.h"
#include "lib/font.h"
#include "lib/subtitle_content.h"
#include <wx/wx.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::string;
using std::cout;
using boost::shared_ptr;

FontsDialog::FontsDialog (wxWindow* parent, shared_ptr<SubtitleContent> content)
	: wxDialog (parent, wxID_ANY, _("Fonts"))
	, _content (content)
	, _set_from_system (0)
{
	_fonts = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (400, 200), wxLC_REPORT | wxLC_SINGLE_SEL);

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
		ip.SetText (_("Font file"));
		ip.SetWidth (300);
		_fonts->InsertColumn (1, ip);
	}

	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	sizer->Add (_fonts, 1, wxEXPAND);

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		_set_from_file = new wxButton (this, wxID_ANY, _("Set from .ttf file..."));
		s->Add (_set_from_file, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
#ifdef DCPOMATIC_WINDOWS
		_set_from_system = new wxButton (this, wxID_ANY, _("Set from system font..."));
		s->Add (_set_from_system, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
#endif
		sizer->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
	}

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	_set_from_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&FontsDialog::set_from_file_clicked, this));
	if (_set_from_system) {
		_set_from_system->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&FontsDialog::set_from_system_clicked, this));
	}
	_fonts->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&FontsDialog::selection_changed, this));
	_fonts->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&FontsDialog::selection_changed, this));

	setup ();
}

void
FontsDialog::setup ()
{
	shared_ptr<SubtitleContent> content = _content.lock ();
	if (!content) {
		return;
	}

	_fonts->DeleteAllItems ();
	list<shared_ptr<Font> > fonts = content->fonts ();
	size_t n = 0;
	for (list<shared_ptr<Font> >::const_iterator i = fonts.begin(); i != fonts.end(); ++i) {
		wxListItem item;
		item.SetId (n);
		_fonts->InsertItem (item);
		_fonts->SetItem (n, 0, std_to_wx ((*i)->id ()));
		if ((*i)->file ()) {
			_fonts->SetItem (n, 1, (*i)->file().get().leaf().string ());
		}
		++n;
	}

	setup_sensitivity ();
}

void
FontsDialog::set_from_file_clicked ()
{
	/* The wxFD_CHANGE_DIR here prevents a `could not set working directory' error 123 on Windows when using
	   non-Latin filenames or paths.
	*/
	wxString default_dir = "";
#ifdef DCPOMATIC_LINUX
	if (boost::filesystem::exists ("/usr/share/fonts/truetype")) {
		default_dir = "/usr/share/fonts/truetype";
	} else {
		default_dir = "/usr/share/fonts";
	}
#endif
#ifdef DCPOMATIC_OSX
	default_dir = "/System/Library/Fonts";
#endif

	wxFileDialog* d = new wxFileDialog (this, _("Choose a font file"), default_dir, wxT (""), wxT ("*.ttf"), wxFD_CHANGE_DIR);
	int const r = d->ShowModal ();

	if (r != wxID_OK) {
		d->Destroy ();
		return;
	}

	set_selected_font_file (wx_to_std (d->GetPath ()));
	d->Destroy ();
}

void
FontsDialog::set_from_system_clicked ()
{
	SystemFontDialog* d = new SystemFontDialog (this);
	int const r = d->ShowModal ();

	if (r != wxID_OK) {
		d->Destroy ();
		return;
	}

	set_selected_font_file (d->get_font().get ());
	d->Destroy ();
}

void
FontsDialog::set_selected_font_file (boost::filesystem::path file)
{
	shared_ptr<SubtitleContent> content = _content.lock ();
	if (!content) {
		return;
	}

	int item = _fonts->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item == -1) {
		return;
	}

	string id = wx_to_std (_fonts->GetItemText (item, 0));

	BOOST_FOREACH (shared_ptr<Font> i, content->fonts ()) {
		if (i->id() == id) {
			i->set_file (file);
		}
	}

	setup ();
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
	_set_from_file->Enable (item != -1);
	if (_set_from_system) {
		_set_from_system->Enable (item != -1);
	}
}
