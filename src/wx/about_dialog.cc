/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

/** @file  src/wx/about_dialog.cc
 *  @brief The "about DCP-o-matic" dialogue box.
 */

#include "about_dialog.h"
#include "wx_util.h"
#include "lib/version.h"
#include "lib/compose.hpp"
#include <wx/notebook.h>
#include <wx/hyperlink.h>

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
		_("Free, open-source DCP creation from almost anything."),
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
		_("(C) 2012-2018 Carl Hetherington, Terrence Meiczinger\n Ole Laursen, Brecht Sanders"),
		wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER
		);

	sizer->Add (t, wxSizerFlags().Centre().Border(wxLEFT | wxRIGHT, 16));

	_notebook = new wxNotebook (this, wxID_ANY);

	wxArrayString written_by;
	written_by.Add (wxT ("Carl Hetherington"));
	written_by.Add (wxT ("Terrence Meiczinger"));
	written_by.Add (wxT ("Ole Laursen"));
	written_by.Add (wxT ("Brecht Sanders"));
	written_by.Add (wxT ("Jianguo Huang"));
	add_section (_("Written by"), written_by);

	wxArrayString translated_by;
	translated_by.Add (wxT ("Manuel AC"));
	translated_by.Add (wxT ("Max Aeschlimann"));
	translated_by.Add (wxT ("Thiago Andre"));
	translated_by.Add (wxT ("Grégoire Ausina"));
	translated_by.Add (wxT ("Tomáš Begeni"));
	translated_by.Add (wxT ("Fabio \"Zak\" Belli"));
	translated_by.Add (wxT ("Cherif Ben Brahim"));
	translated_by.Add (wxT ("Massimiliano Broggi"));
	translated_by.Add (wxT ("Davide Dall'AraCiao"));
	translated_by.Add (wxT ("William Fanelli"));
	translated_by.Add (wxT ("Max M. Fuhlendorf"));
	translated_by.Add (wxT ("Tomáš Hlaváč"));
	translated_by.Add (wxT ("Thierry Journet"));
	translated_by.Add (wxT ("Adam Klotblixt"));
	translated_by.Add (wxT ("Theo Kooijmans"));
	translated_by.Add (wxT ("Carsten Kurz"));
	translated_by.Add (wxT ("Lilian Lefranc"));
	translated_by.Add (wxT ("Gérald Maruccia"));
	translated_by.Add (wxT ("Mike Mazur"));
	translated_by.Add (wxT ("Rob van Nieuwkerk"));
	translated_by.Add (wxT ("Anders Uhl Pedersen"));
	translated_by.Add (wxT ("Olivier Perriere"));
	translated_by.Add (wxT ("Markus Raab"));
	translated_by.Add (wxT ("Tiago Casal Ribeiro"));
	translated_by.Add (wxT ("Davide Sanvito"));
	translated_by.Add (wxT ("Marek Skrzelowski"));
	translated_by.Add (wxT ("Igor Voytovich"));
	translated_by.Add (wxT ("Rov (若文)"));
	translated_by.Add (wxT ("刘汉源"));
 	add_section (_("Translated by"), translated_by);

	wxArrayString with_help_from;
	with_help_from.Add (wxT ("David Vignoni"));
	with_help_from.Add (wxT ("Dennis Couzin"));
	with_help_from.Add (wxT ("Gérald Maruccia"));
	with_help_from.Add (wxT ("Julian van Mil"));
	add_section (_("With help from"), with_help_from);

	wxArrayString supported_by;
	#include "../../build/supporters.cc"
	add_section (_("Supported by"), supported_by);

	wxArrayString tested_by;
	tested_by.Add (wxT ("Manuel AC"));
	tested_by.Add (wxT ("Trever Anderson"));
	tested_by.Add (wxT ("JP Beauviala"));
	tested_by.Add (wxT ("Mike Blakesley"));
	tested_by.Add (wxT ("David Booty"));
	tested_by.Add (wxT ("Roop Chand"));
	tested_by.Add (wxT ("Daniel Chauvet"));
	tested_by.Add (wxT ("Adam Colt"));
	tested_by.Add (wxT ("John Convertino"));
	tested_by.Add (wxT ("Andreas Eli"));
	tested_by.Add (wxT ("Leo Enticknap"));
	tested_by.Add (wxT ("Jose Angel Velasco Fernandez"));
	tested_by.Add (wxT ("Maurizio Giampà"));
	tested_by.Add (wxT ("Luke Granger-Brown"));
	tested_by.Add (wxT ("Sumit Guha"));
	tested_by.Add (wxT ("Steve Guttag"));
	tested_by.Add (wxT ("Patrick Haderer"));
	tested_by.Add (wxT ("Bill Hamell"));
	tested_by.Add (wxT ("Jonathan Jensen"));
	tested_by.Add (wxT ("Thierry Journet"));
	tested_by.Add (wxT ("Markus Kalb"));
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
	tested_by.Add (wxT ("Gérald Maruccia"));
	tested_by.Add (wxT ("George Mazarakis"));
	tested_by.Add (wxT ("Mattias Mattsson"));
	tested_by.Add (wxT ("Will Meadows"));
	tested_by.Add (wxT ("Brad Miller"));
	tested_by.Add (wxT ("Ash Mitchell"));
	tested_by.Add (wxT ("Rob van Nieuwkerk"));
	tested_by.Add (wxT ("Anders Nordentoft-Madsen"));
	tested_by.Add (wxT ("Mauro Ottonello"));
	tested_by.Add (wxT ("Peter Puchner"));
	tested_by.Add (wxT ("Markus Raab"));
	tested_by.Add (wxT ("Michael Reckert"));
	tested_by.Add (wxT ("Greg Rooke"));
	tested_by.Add (wxT ("Elad Saad"));
	tested_by.Add (wxT ("Karim Senoucci"));
	tested_by.Add (wxT ("Hordur Valgardsson"));
	tested_by.Add (wxT ("Xenophon the Vampire"));
	tested_by.Add (wxT ("Simon Vannarath"));
	tested_by.Add (wxT ("Igor Voytovich"));
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
	int const N = 4;

	wxScrolledWindow* panel = new wxScrolledWindow (_notebook);
	panel->SetMaxSize (wxSize (-1, 380));
	panel->EnableScrolling (false, true);
	panel->SetScrollRate (0, 32);
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
