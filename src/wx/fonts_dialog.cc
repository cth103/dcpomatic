/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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
#include "fonts_dialog.h"
#include "system_font_dialog.h"
#include "wx_util.h"
#include "lib/content.h"
#include "lib/font.h"
#include "lib/text_content.h"
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <memory>


using std::list;
using std::shared_ptr;
using std::string;
using namespace dcpomatic;


FontsDialog::FontsDialog (wxWindow* parent, shared_ptr<Content> content, shared_ptr<TextContent> caption)
	: wxDialog (parent, wxID_ANY, _("Fonts"))
	, _content (content)
	, _caption (caption)
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
		ip.SetText (_("File"));
		ip.SetWidth (450);
		_fonts->InsertColumn (1, ip);
	}

	auto sizer = new wxBoxSizer (wxHORIZONTAL);
	sizer->Add (_fonts, 1, wxEXPAND | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);

	auto buttons_panel = new wxPanel(this);
	auto buttons_sizer = new wxBoxSizer(wxVERTICAL);

	_set_from_file = new Button(buttons_panel, _("Set from file..."));
	buttons_sizer->Add (_set_from_file, 0, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

#ifdef DCPOMATIC_WINDOWS
	_set_from_system_font = new Button(buttons_panel, _("Set from system font..."));
	buttons_sizer->Add (_set_from_system_font, 0, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
#endif

	buttons_panel->SetSizer(buttons_sizer);
	sizer->Add(buttons_panel);

	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);

	auto buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	_set_from_file->Bind(wxEVT_BUTTON, boost::bind(&FontsDialog::set_from_file_clicked, this));
	if (_set_from_system_font) {
		_set_from_system_font->Bind(wxEVT_BUTTON, boost::bind(&FontsDialog::set_from_system_font_clicked, this));
	}
	_fonts->Bind (wxEVT_LIST_ITEM_SELECTED, boost::bind (&FontsDialog::selection_changed, this));
	_fonts->Bind (wxEVT_LIST_ITEM_DESELECTED, boost::bind (&FontsDialog::selection_changed, this));

	setup ();
}


void
FontsDialog::setup ()
{
	auto content = _content.lock ();
	auto caption = _caption.lock ();
	if (!content || !caption) {
		return;
	}

	_fonts->DeleteAllItems ();
	size_t n = 0;
	for (auto i: caption->fonts ()) {
		wxListItem item;
		item.SetId (n);
		_fonts->InsertItem (item);
		auto const id = i->id().empty() ? _("Unspecified") : std_to_wx(i->id());
		_fonts->SetItem(n, 0, id);
		_fonts->SetItemData(n, i->id().empty());
		if (i->file()) {
			_fonts->SetItem(n, 1, i->file()->filename().string());
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
	_set_from_file->Enable (item != -1);
	if (_set_from_system_font) {
		_set_from_system_font->Enable (item != -1);
	}
}


shared_ptr<Font>
FontsDialog::get_selection ()
{
	auto caption = _caption.lock();
	if (!caption) {
		return {};
	}

	int const item = _fonts->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	auto const id = _fonts->GetItemData(item) ? "" : wx_to_std(_fonts->GetItemText(item, 0));
	return caption->get_font(id);
}


void
FontsDialog::set_from_file_clicked ()
{
	auto font = get_selection();
	if (!font) {
		return;
	}

        /* The wxFD_CHANGE_DIR here prevents a `could not set working directory' error 123 on Windows when using
           non-Latin filenames or paths.
        */
        wxString default_dir = "";
#ifdef DCPOMATIC_LINUX
        if (dcp::filesystem::exists("/usr/share/fonts/truetype")) {
                default_dir = "/usr/share/fonts/truetype";
        } else {
                default_dir = "/usr/share/fonts";
        }
#endif
#ifdef DCPOMATIC_OSX
        default_dir = "/System/Library/Fonts";
#endif

	auto d = make_wx<wxFileDialog>(this, _("Choose a font file"), default_dir, wxT(""), wxT("*.ttf;*.otf;*.ttc"), wxFD_CHANGE_DIR);

	if (d->ShowModal() != wxID_OK) {
		return;
	}

	font->set_file (wx_to_std(d->GetPath()));
	setup ();
}


void
FontsDialog::set_from_system_font_clicked()
{
	auto font = get_selection();
	if (!font) {
		return;
	}

	auto dialog = make_wx<SystemFontDialog>(this);
	if (dialog->ShowModal() == wxID_OK) {
		auto font_file = dialog->get_font();
		if (font_file) {
			font->set_file(*font_file);
		}
	}

	setup ();
}
