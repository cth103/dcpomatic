/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <wx/notebook.h>
#include <wx/hyperlink.h>
#include "lib/version.h"
#include "lib/compose.hpp"
#include "about_dialog.h"
#include "wx_util.h"

using std::vector;

AboutDialog::AboutDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("About DVD-o-matic"))
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	
	wxFont title_font (*wxNORMAL_FONT);
	title_font.SetPointSize (title_font.GetPointSize() + 4);
	title_font.SetWeight (wxFONTWEIGHT_BOLD);

	wxFont version_font (*wxNORMAL_FONT);
	version_font.SetWeight (wxFONTWEIGHT_BOLD);
	
	wxStaticText* t = new wxStaticText (this, wxID_ANY, _("DVD-o-matic"));
	t->SetFont (title_font);
	sizer->Add (t, wxSizerFlags().Centre().Border());

	wxString s;
	if (strcmp (dvdomatic_git_commit, "release") == 0) {
		t = new wxStaticText (this, wxID_ANY, std_to_wx (String::compose ("Version %1", dvdomatic_version)));
	} else {
		t = new wxStaticText (this, wxID_ANY, std_to_wx (String::compose ("Version %1 git %2", dvdomatic_version, dvdomatic_git_commit)));
	}
	t->SetFont (version_font);
	sizer->Add (t, wxSizerFlags().Centre().Border());
	sizer->AddSpacer (12);

	t = new wxStaticText (
		this, wxID_ANY,
		_("Free, open-source DCP generation from almost anything."),
		wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER
		);
	
	sizer->Add (t, wxSizerFlags().Centre().Border());

	wxHyperlinkCtrl* h = new wxHyperlinkCtrl (
		this, wxID_ANY,
		wxT ("www.carlh.net/software/dvdomatic"),
		wxT ("http://www.carlh.net/software/dvdomatic")
		);

	sizer->Add (h, wxSizerFlags().Centre().Border());

	t = new wxStaticText (
		this, wxID_ANY,
		_("(C) 2012-2013 Carl Hetherington, Terrence Meiczinger, Paul Davis, Ole Laursen"),
		wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER
		);
	
	sizer->Add (t, wxSizerFlags().Centre().Border());

	_notebook = new wxNotebook (this, wxID_ANY);

	wxArrayString written_by;
	written_by.Add (wxT ("Carl Hetherington"));
	written_by.Add (wxT ("Terrence Meiczinger"));
	written_by.Add (wxT ("Paul Davis"));
	written_by.Add (wxT ("Ole Laursen"));
	add_section (_("Written by"), written_by);

	wxArrayString translated_by;
	translated_by.Add (wxT ("Olivier Perriere"));
	translated_by.Add (wxT ("Lilian Lefranc"));
	translated_by.Add (wxT ("Thierry Journet"));
	translated_by.Add (wxT ("Massimiliano Broggi"));
	translated_by.Add (wxT ("Manuel AC"));
	translated_by.Add (wxT ("Adam Klotblixt"));
	add_section (_("Translated by"), translated_by);

	wxArrayString supported_by;
	supported_by.Add (wxT ("Carsten Kurz"));
	supported_by.Add (wxT ("Wolfgang Woehl"));
	supported_by.Add (wxT ("Manual AC"));
	supported_by.Add (wxT ("Theo Lipfert"));
	supported_by.Add (wxT ("Olivier Lemaire"));
	supported_by.Add (wxT ("Andrä Steiner"));
	supported_by.Add (wxT ("Jonathan Jensen"));
	supported_by.Add (wxT ("Kjarten Michaelsen"));
	supported_by.Add (wxT ("Jussi Siponen"));
	supported_by.Add (wxT ("Cinema Clarici"));
	supported_by.Add (wxT ("Evan Freeze"));
	supported_by.Add (wxT ("Flor Guillaume"));
	supported_by.Add (wxT ("Adam Klotblixt "));
	supported_by.Add (wxT ("Lilian Lefranc"));
	supported_by.Add (wxT ("Gavin Lewarne"));
	supported_by.Add (wxT ("Lasse Salling"));
	supported_by.Add (wxT ("Andres Fink"));
	supported_by.Add (wxT ("Kieran Carroll"));
	add_section (_("Supported by"), supported_by);

	sizer->Add (_notebook, wxSizerFlags().Centre().Border().Expand());
	
#if 0	
	info.SetWebSite (wxT ("http://carlh.net/software/dvdomatic"));
#endif

	SetSizerAndFit (sizer);
}

void
AboutDialog::add_section (wxString name, wxArrayString credits)
{
	static bool first = true;
	int const N = 3;

	wxPanel* panel = new wxPanel (_notebook, wxID_ANY);
	wxSizer* overall_sizer = new wxBoxSizer (wxHORIZONTAL);

	vector<wxSizer*> sizers;
	
	for (int i = 0; i < N; ++i) {
		sizers.push_back (new wxBoxSizer (wxVERTICAL));
		overall_sizer->Add (sizers.back (), 1, wxEXPAND | wxALL, 6);
	}

	int c = 0;
	for (size_t i = 0; i < credits.Count(); ++i) {
		add_label_to_sizer (sizers[c], panel, credits[i]);
		++c;
		if (c == N) {
			c = 0;
		}
	}

	panel->SetSizerAndFit (overall_sizer);
	_notebook->AddPage (panel, name, first);
	first = false;
}
