/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "lib/font.h"
#include "lib/subtitle_content.h"
#include "fonts_dialog.h"
#include "wx_util.h"
#include <wx/wx.h>

using std::list;
using std::string;
using boost::shared_ptr;

FontsDialog::FontsDialog (wxWindow* parent, shared_ptr<SubtitleContent> content)
	: wxDialog (parent, wxID_ANY, _("Fonts"))
	, _content (content)
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
		_set_file = new wxButton (this, wxID_ANY, _("Set file..."));
		s->Add (_set_file, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
		sizer->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
	}

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	_set_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&FontsDialog::set_file_clicked, this));

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
		_fonts->SetItem (n, 0, std_to_wx ((*i)->id));
		if ((*i)->file) {
			_fonts->SetItem (n, 1, (*i)->file.get().leaf().string ());
		}
		++n;
	}
}

void
FontsDialog::set_file_clicked ()
{
	shared_ptr<SubtitleContent> content = _content.lock ();
	if (!content) {
		return;
	}

	int item = _fonts->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item == -1) {
		return;
	}
	
	/* The wxFD_CHANGE_DIR here prevents a `could not set working directory' error 123 on Windows when using
	   non-Latin filenames or paths.
	*/
	wxFileDialog* d = new wxFileDialog (this, _("Choose a font file"), wxT (""), wxT (""), wxT ("*.ttf"), wxFD_CHANGE_DIR);
	int const r = d->ShowModal ();

	if (r != wxID_OK) {
		d->Destroy ();
		return;
	}

	string id = wx_to_std (_fonts->GetItemText (item, 0));

	list<shared_ptr<Font> > fonts = content->fonts ();
	for (list<shared_ptr<Font> >::iterator i = fonts.begin(); i != fonts.end(); ++i) {
		if ((*i)->id == id) {
			(*i)->file = wx_to_std (d->GetPath ());
		}
	}

	d->Destroy ();

	setup ();
}
