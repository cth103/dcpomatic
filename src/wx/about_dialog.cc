/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/wx/about_dialog.cc
 *  @brief The "about DCP-o-matic" dialogue box.
 */

#include <wx/notebook.h>
#include <wx/hyperlink.h>
#include "lib/version.h"
#include "lib/compose.hpp"
#include "about_dialog.h"
#include "wx_util.h"

using std::vector;

AboutDialog::AboutDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("About DCP-o-matic"))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	
	wxFont title_font (*wxNORMAL_FONT);
	title_font.SetPointSize (title_font.GetPointSize() + 12);
	title_font.SetWeight (wxFONTWEIGHT_BOLD);

	wxFont subtitle_font (*wxNORMAL_FONT);
	subtitle_font.SetPointSize (subtitle_font.GetPointSize() + 2);

	wxFont version_font (*wxNORMAL_FONT);
	version_font.SetWeight (wxFONTWEIGHT_BOLD);
	
	wxStaticText* t = new wxStaticText (this, wxID_ANY, _("DCP-o-matic"));
	t->SetFont (title_font);
	sizer->Add (t, wxSizerFlags().Centre().Border(wxALL, 16));

	wxString s;
	if (strcmp (dcpomatic_git_commit, "release") == 0) {
		t = new wxStaticText (this, wxID_ANY, std_to_wx (String::compose ("Version %1", dcpomatic_version)));
	} else {
		t = new wxStaticText (this, wxID_ANY, std_to_wx (String::compose ("Version %1 git %2", dcpomatic_version, dcpomatic_git_commit)));
	}
	t->SetFont (version_font);
	sizer->Add (t, wxSizerFlags().Centre().Border(wxALL, 2));
	sizer->AddSpacer (12);

	t = new wxStaticText (
		this, wxID_ANY,
		_("Free, open-source DCP generation from almost anything."),
		wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER
		);
	t->SetFont (subtitle_font);
	
	sizer->Add (t, wxSizerFlags().Centre().Border(wxALL, 8));

	wxHyperlinkCtrl* h = new wxHyperlinkCtrl (
		this, wxID_ANY,
		wxT ("dcpomatic.com"),
		wxT ("http://dcpomatic.com")
		);

	sizer->Add (h, wxSizerFlags().Centre().Border(wxALL, 8));

	t = new wxStaticText (
		this, wxID_ANY,
		_("(C) 2012-2015 Carl Hetherington, Terrence Meiczinger\n Ole Laursen, Brecht Sanders"),
		wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER
		);
	
	sizer->Add (t, wxSizerFlags().Centre().Border(wxLEFT | wxRIGHT, 16));

	_notebook = new wxNotebook (this, wxID_ANY);

	wxArrayString written_by;
	written_by.Add (wxT ("Carl Hetherington"));
	written_by.Add (wxT ("Terrence Meiczinger"));
	written_by.Add (wxT ("Ole Laursen"));
	written_by.Add (wxT ("Brecht Sanders"));
	add_section (_("Written by"), written_by);

	wxArrayString translated_by;
	translated_by.Add (wxT ("Manuel AC"));
	translated_by.Add (wxT ("Massimiliano Broggi"));
	translated_by.Add (wxT ("William Fanelli"));
	translated_by.Add (wxT ("Thierry Journet"));
	translated_by.Add (wxT ("Adam Klotblixt"));
	translated_by.Add (wxT ("Lilian Lefranc"));
	translated_by.Add (wxT ("Olivier Perriere"));
	translated_by.Add (wxT ("Markus Raab"));
	translated_by.Add (wxT ("Theo Kooijmans"));
	translated_by.Add (wxT ("Max Aeschlimann"));
	translated_by.Add (wxT ("Carsten Kurz"));
	translated_by.Add (wxT ("Grégoire Ausina"));
	translated_by.Add (wxT ("Cherif Ben Brahim"));
	add_section (_("Translated by"), translated_by);

	wxArrayString artwork_by;
	artwork_by.Add (wxT ("David Vignoni"));
	add_section (_("Artwork by"), artwork_by);

	wxArrayString supported_by;
	supported_by.Add (wxT ("Manual AC"));
	supported_by.Add (wxT ("Kambiz Afshar"));
	supported_by.Add (wxT ("Alex Asp"));
	supported_by.Add (wxT ("Eric Audurier"));
	supported_by.Add (wxT ("Louis Belloisy"));
	supported_by.Add (wxT ("Mike Blakesley"));
	supported_by.Add (wxT ("Jeff Boot"));
	supported_by.Add (wxT ("Kieran Carroll"));
	supported_by.Add (wxT ("Matt Carter"));
	supported_by.Add (wxT ("Frank Cianciolo"));
	supported_by.Add (wxT ("Cinema Clarici"));
	supported_by.Add (wxT ("Adam Colt"));
	supported_by.Add (wxT ("Matthias Damm"));
	supported_by.Add (wxT ("Alexey Derevyanko"));
	supported_by.Add (wxT ("Unwana Essien"));
	supported_by.Add (wxT ("Maxime Estoppey"));
	supported_by.Add (wxT ("Andres Fink"));
	supported_by.Add (wxT ("Evan Freeze"));
	supported_by.Add (wxT ("Silvio Giuliano"));
	supported_by.Add (wxT ("Alan Gouger"));
	supported_by.Add (wxT ("Flor Guillaume"));
	supported_by.Add (wxT ("Patrick Haderer"));
	supported_by.Add (wxT ("Antonio Ruiz Hernandez"));
	supported_by.Add (wxT ("Jonathan Jensen"));
	supported_by.Add (wxT ("Chris Kay"));
	supported_by.Add (wxT ("Adam Klotblixt"));
	supported_by.Add (wxT ("Filip Kovcin"));
	supported_by.Add (wxT ("Cihan Kulaber"));
	supported_by.Add (wxT ("Carsten Kurz"));
	supported_by.Add (wxT ("Lilian Lefranc"));
	supported_by.Add (wxT ("Sean Leigh"));
	supported_by.Add (wxT ("Olivier Lemaire"));
	supported_by.Add (wxT ("Gavin Lewarne"));
	supported_by.Add (wxT ("Theo Lipfert"));
	supported_by.Add (wxT ("Mattias Mattsson"));
	supported_by.Add (wxT ("Kjarten Michaelsen"));
	supported_by.Add (wxT ("Aldo Midali"));
	supported_by.Add (wxT ("Sylvain Mielle"));
	supported_by.Add (wxT ("Lindsay Morris"));
	supported_by.Add (wxT ("Гуляев Михаил"));
	supported_by.Add (wxT ("David Nedrow"));
	supported_by.Add (wxT ("Tim O'Brien"));
	supported_by.Add (wxT ("Jerome Cohen Olivar"));
	supported_by.Add (wxT ("Ivan Pullman"));
	supported_by.Add (wxT ("Mark Rolfe"));
	supported_by.Add (wxT ("David Rozenthal"));
	supported_by.Add (wxT ("Andrä Steiner"));
	supported_by.Add (wxT ("Jussi Siponen"));
	supported_by.Add (wxT ("Lasse Salling"));
	supported_by.Add (wxT ("Mike Stiebing"));
	supported_by.Add (wxT ("Randy Stankey"));
	supported_by.Add (wxT ("Bruce Taylor"));
	supported_by.Add (wxT ("Richard Turner"));
	supported_by.Add (wxT ("Wolfgang Woehl"));
	supported_by.Add (wxT ("Wolfram Weber"));
	supported_by.Add (wxT ("Johannes Wilbrand"));
	supported_by.Add (wxT ("Frank de Wulf"));
	supported_by.Add (wxT ("Pavel Zhdanko"));
	supported_by.Add (wxT ("Daniel Židek"));
	add_section (_("Supported by"), supported_by);

	wxArrayString tested_by;
	tested_by.Add (wxT ("Trever Anderson"));
	tested_by.Add (wxT ("Mike Blakesley"));
	tested_by.Add (wxT ("David Booty"));
	tested_by.Add (wxT ("Roop Chand"));
	tested_by.Add (wxT ("Daniel Chauvet"));
	tested_by.Add (wxT ("Adam Colt"));
	tested_by.Add (wxT ("John Convertino"));
	tested_by.Add (wxT ("Andreas Eli"));
	tested_by.Add (wxT ("Maurizio Giampà"));
	tested_by.Add (wxT ("Luke Granger-Brown"));
	tested_by.Add (wxT ("Sumit Guha"));
	tested_by.Add (wxT ("Steve Guttag"));
	tested_by.Add (wxT ("Patrick Haderer"));
	tested_by.Add (wxT ("Bill Hamell"));
	tested_by.Add (wxT ("Jonathan Jensen"));
	tested_by.Add (wxT ("Thierry Journet"));
	tested_by.Add (wxT ("Ada de Kamper"));
	tested_by.Add (wxT ("Stefan Karner"));
	tested_by.Add (wxT ("Adam Keay"));
	tested_by.Add (wxT ("Simon Kesselman"));
	tested_by.Add (wxT ("Pepijn Klijs"));
	tested_by.Add (wxT ("Denzil Kriekenbeek"));
	tested_by.Add (wxT ("Carsten Kurz"));
	tested_by.Add (wxT ("Bill Lam"));
	tested_by.Add (wxT ("Lilian Lefranc"));
	tested_by.Add (wxT ("Olivier Lemaire"));
	tested_by.Add (wxT ("Gavin Lewarne"));
	tested_by.Add (wxT ("Mattias Mattsson"));
	tested_by.Add (wxT ("Gérald Maruccia"));
	tested_by.Add (wxT ("Will Meadows"));
	tested_by.Add (wxT ("Brad Miller"));
	tested_by.Add (wxT ("Anders Nordentoft-Madsen"));
	tested_by.Add (wxT ("Mauro Ottonello"));
	tested_by.Add (wxT ("Peter Puchner"));
	tested_by.Add (wxT ("Markus Raab"));
	tested_by.Add (wxT ("Greg Rooke"));
	tested_by.Add (wxT ("Elad Saad"));
	tested_by.Add (wxT ("Karim Senoucci"));
	tested_by.Add (wxT ("Simon Vannarath"));
	tested_by.Add (wxT ("Andrew Walls"));
	tested_by.Add (wxT ("Andreas Weiss"));
	tested_by.Add (wxT ("Paul Willmott"));
	tested_by.Add (wxT ("Wolfgang Woehl"));
	add_section (_("Tested by"), tested_by);
	
	sizer->Add (_notebook, wxSizerFlags().Centre().Border(wxALL, 16).Expand());

	overall_sizer->Add (sizer);

	wxSizer* buttons = CreateButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, 1, wxEXPAND | wxALL, 4);
	}
	
	SetSizerAndFit (overall_sizer);
}

/** Add a section of credits.
 *  @param name Name of section.
 *  @param credits List of names.
 */
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
		add_label_to_sizer (sizers[c], panel, credits[i], false);
		++c;
		if (c == N) {
			c = 0;
		}
	}

	panel->SetSizerAndFit (overall_sizer);
	_notebook->AddPage (panel, name, first);
	first = false;
}
