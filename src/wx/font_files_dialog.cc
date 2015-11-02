/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "font_files_dialog.h"
#include "system_font_dialog.h"

using boost::bind;

FontFilesDialog::FontFilesDialog (wxWindow* parent, FontFiles files)
#ifdef DCPOMATIC_WINDOWS
	: TableDialog (parent, _("Fonts"), 4, 1, true)
#else
	: TableDialog (parent, _("Fonts"), 3, 1, true)
#endif
	, _files (files)
{
	wxString labels[] = {
		_("Normal font"),
		_("Italic font"),
		_("Bold font")
	};

	DCPOMATIC_ASSERT (FontFiles::VARIANTS == 3);

	for (int i = 0; i < FontFiles::VARIANTS; ++i) {
		add (labels[i], true);
		_name[i] = new wxStaticText (
			this, wxID_ANY,
			std_to_wx(_files.get(static_cast<FontFiles::Variant>(i)).get_value_or("").string()),
			wxDefaultPosition,
			wxSize (200, -1)
			);
		_table->Add (_name[i], 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
		add (_set_file[i] = new wxButton (this, wxID_ANY, _("Set from file...")));
		_set_file[i]->Bind (wxEVT_COMMAND_BUTTON_CLICKED, bind (&FontFilesDialog::set_from_file_clicked, this, static_cast<FontFiles::Variant>(i)));
#ifdef DCPOMATIC_WINDOWS
		add (_set_system[i] = new wxButton (this, wxID_ANY, _("Set from system font...")));
		_set_system[i]->Bind (wxEVT_COMMAND_BUTTON_CLICKED, bind (&FontFilesDialog::set_from_system_clicked, this, static_cast<FontFiles::Variant>(i)));
#endif
	}

	layout ();
}

void
FontFilesDialog::set_from_file_clicked (FontFiles::Variant variant)
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

	set (variant, wx_to_std (d->GetPath ()));
	d->Destroy ();
}

#ifdef DCPOMATIC_WINDOWS
void
FontFilesDialog::set_from_system_clicked (FontFiles::Variant variant)
{
	SystemFontDialog* d = new SystemFontDialog (this);
	int const r = d->ShowModal ();

	if (r != wxID_OK) {
		d->Destroy ();
		return;
	}

	set (variant, d->get_font().get());
	d->Destroy ();
}
#endif

void
FontFilesDialog::set (FontFiles::Variant variant, boost::filesystem::path path)
{
	_files.set (variant, path);
	_name[variant]->SetLabel (std_to_wx (path.leaf().string()));
}
