/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
#include "static_text.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/variant.h"
#include "lib/version.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/hyperlink.h>
#include <wx/notebook.h>
LIBDCP_ENABLE_WARNINGS

using std::vector;


AboutDialog::AboutDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, variant::wx::insert_dcpomatic(_("About %s")))
{
	auto overall_sizer = new wxBoxSizer(wxVERTICAL);
	auto sizer = new wxBoxSizer(wxVERTICAL);

	wxFont title_font(*wxNORMAL_FONT);
	title_font.SetPointSize(title_font.GetPointSize() + 12);
	title_font.SetWeight(wxFONTWEIGHT_BOLD);

	wxFont subtitle_font(*wxNORMAL_FONT);
	subtitle_font.SetPointSize(subtitle_font.GetPointSize() + 2);

	wxFont version_font(*wxNORMAL_FONT);
	version_font.SetWeight(wxFONTWEIGHT_BOLD);

	auto t = new StaticText(this, variant::wx::dcpomatic());
	t->SetFont(title_font);
	sizer->Add(t, wxSizerFlags().Centre().Border(wxALL, 16));

	if (strcmp(dcpomatic_git_commit, "release") == 0) {
		t = new StaticText(this, std_to_wx(fmt::format("Version {}", dcpomatic_version)));
	} else {
		t = new StaticText(this, std_to_wx(fmt::format("Version {} git {}", dcpomatic_version, dcpomatic_git_commit)));
	}
	t->SetFont(version_font);
	sizer->Add(t, wxSizerFlags().Centre().Border(wxALL, 2));
	sizer->AddSpacer(12);

	if (variant::show_tagline())
	{
		t = new StaticText(
			this,
			_("Free, open-source DCP creation from almost anything."),
			wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER
			);
		t->SetFont(subtitle_font);
		sizer->Add(t, wxSizerFlags().Centre().Border(wxALL, 8));
	}

	if (variant::show_dcpomatic_website())
	{
		auto h = new wxHyperlinkCtrl(
			this, wxID_ANY,
			char_to_wx("dcpomatic.com"),
			char_to_wx("https://dcpomatic.com")
			);
		sizer->Add(h, wxSizerFlags().Centre().Border(wxALL, 8));
	}

	if (variant::show_credits())
	{
		t = new StaticText(
			this,
			_("(C) 2012-2026 Carl Hetherington, Terrence Meiczinger\nAaron Boxer"),
			wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER
			);

		sizer->Add(t, wxSizerFlags().Centre().Border(wxLEFT | wxRIGHT, 16));

		_notebook = new wxNotebook(this, wxID_ANY);

		wxArrayString written_by;
		written_by.Add(char_to_wx("Carl Hetherington"));
		written_by.Add(char_to_wx("Terrence Meiczinger"));
		written_by.Add(char_to_wx("Mart Jansink"));
		written_by.Add(char_to_wx("Aaron Boxer"));
		written_by.Add(char_to_wx("Benjamin Radel"));
		add_section(_("Written by"), written_by);

		wxArrayString with_help_from;
		with_help_from.Add(char_to_wx("David Vignoni"));
		with_help_from.Add(char_to_wx("Dennis Couzin"));
		with_help_from.Add(char_to_wx("Carsten Kurz"));
		with_help_from.Add(char_to_wx("Gérald Maruccia"));
		with_help_from.Add(char_to_wx("Julian van Mil"));
		with_help_from.Add(char_to_wx("Lilian Lefranc"));
		add_section(_("With help from"), with_help_from);

		wxArrayString translated_by;
		translated_by.Add(char_to_wx("Manuel AC"));
		translated_by.Add(char_to_wx("Max Aeschlimann"));
		translated_by.Add(char_to_wx("Gökhan Aksoy"));
		translated_by.Add(char_to_wx("Thiago Andre"));
		translated_by.Add(char_to_wx("Felice D'Andrea"));
		translated_by.Add(char_to_wx("Németh Áron"));
		translated_by.Add(char_to_wx("Grégoire Ausina"));
		translated_by.Add(char_to_wx("Tomáš Begeni"));
		translated_by.Add(char_to_wx("Fabio \"Zak\" Belli"));
		translated_by.Add(char_to_wx("Cherif Ben Brahim"));
		translated_by.Add(char_to_wx("Massimiliano Broggi"));
		translated_by.Add(char_to_wx("Dan Cohen"));
		translated_by.Add(char_to_wx("Akivili Collindort"));
		translated_by.Add(char_to_wx("Davide Dall'AraCiao"));
		translated_by.Add(char_to_wx("Евгений Дац"));
		translated_by.Add(char_to_wx("Uwe Dittes"));
		translated_by.Add(char_to_wx("Михаил Эпштейн"));
		translated_by.Add(char_to_wx("William Fanelli"));
		translated_by.Add(char_to_wx("Max M. Fuhlendorf"));
		translated_by.Add(char_to_wx("Tomáš Hlaváč"));
		translated_by.Add(char_to_wx("Thierry Journet"));
		translated_by.Add(char_to_wx("Adam Klotblixt"));
		translated_by.Add(char_to_wx("Theo Kooijmans"));
		translated_by.Add(char_to_wx("Carsten Kurz"));
		translated_by.Add(char_to_wx("Roman Kuznetsov"));
		translated_by.Add(char_to_wx("David Lankes"));
		translated_by.Add(char_to_wx("Lilian Lefranc"));
		// Doesn't want to be credited
		// translated_by.Add(char_to_wx("Kahn Li"));
		translated_by.Add(char_to_wx("Gérald Maruccia"));
		translated_by.Add(char_to_wx("Mattias Mattsson"));
		translated_by.Add(char_to_wx("Mike Mazur"));
		translated_by.Add(char_to_wx("Rob van Nieuwkerk"));
		translated_by.Add(char_to_wx("Anders Uhl Pedersen"));
		translated_by.Add(char_to_wx("David Perrenoud"));
		translated_by.Add(char_to_wx("Olivier Perriere"));
		translated_by.Add(char_to_wx("Markus Raab"));
		translated_by.Add(char_to_wx("Soleyman Rahmani"));
		translated_by.Add(char_to_wx("Tiago Casal Ribeiro"));
		translated_by.Add(char_to_wx("Davide Sanvito"));
		translated_by.Add(char_to_wx("Marek Skrzelowski"));
		translated_by.Add(char_to_wx("Danbo Song"));
		translated_by.Add(char_to_wx("Martin Srebotnjak"));
		translated_by.Add(char_to_wx("Laurent Tenzer"));
		translated_by.Add(char_to_wx("Michał Tomaszewski"));
		translated_by.Add(char_to_wx("Igor Voytovich"));
		translated_by.Add(char_to_wx("Rov (若文)"));
		translated_by.Add(char_to_wx("刘汉源"));
		translated_by.Add(char_to_wx("poppinzhang"));
		translated_by.Add(char_to_wx("林雅成"));
		add_section(_("Translated by"), translated_by);

		wxArrayString patrons;
		patrons.Add(char_to_wx("LightBenders Color Grading Boutique"));
		add_section(_("Patrons"), patrons);

		wxArrayString subscribers;
		#include "subscribers.cc"
		add_section(_("Subscribers"), subscribers);

		wxArrayString supported_by;
		#include "supporters.cc"
		add_section(_("Also supported by"), supported_by);

		wxArrayString tested_by;
		tested_by.Add(char_to_wx("Manuel AC"));
		tested_by.Add(char_to_wx("Trever Anderson"));
		tested_by.Add(char_to_wx("Mohamad W. Ali"));
		tested_by.Add(char_to_wx("JP Beauviala"));
		tested_by.Add(char_to_wx("Mike Blakesley"));
		tested_by.Add(char_to_wx("David Booty"));
		tested_by.Add(char_to_wx("Antonio Casado"));
		tested_by.Add(char_to_wx("Roop Chand"));
		tested_by.Add(char_to_wx("Daniel Chauvet"));
		tested_by.Add(char_to_wx("Adam Colt"));
		tested_by.Add(char_to_wx("John Convertino"));
		tested_by.Add(char_to_wx("Daniel Courville"));
		tested_by.Add(char_to_wx("Marek Dudzik"));
		tested_by.Add(char_to_wx("Andreas Eli"));
		tested_by.Add(char_to_wx("Leo Enticknap"));
		tested_by.Add(char_to_wx("Jose Angel Velasco Fernandez"));
		tested_by.Add(char_to_wx("Maurizio Giampà"));
		tested_by.Add(char_to_wx("Luke Granger-Brown"));
		tested_by.Add(char_to_wx("Sumit Guha"));
		tested_by.Add(char_to_wx("Steve Guttag"));
		tested_by.Add(char_to_wx("Patrick Haderer"));
		tested_by.Add(char_to_wx("Bill Hamell"));
		tested_by.Add(char_to_wx("Groet Han"));
		tested_by.Add(char_to_wx("Jonathan Jensen"));
		tested_by.Add(char_to_wx("Thierry Journet"));
		tested_by.Add(char_to_wx("Markus Kalb"));
		tested_by.Add(char_to_wx("Ada de Kamper"));
		tested_by.Add(char_to_wx("Stefan Karner"));
		tested_by.Add(char_to_wx("Adam Keay"));
		tested_by.Add(char_to_wx("Simon Kesselman"));
		tested_by.Add(char_to_wx("Pepijn Klijs"));
		tested_by.Add(char_to_wx("Denzil Kriekenbeek"));
		tested_by.Add(char_to_wx("Carsten Kurz"));
		tested_by.Add(char_to_wx("Bill Lam"));
		tested_by.Add(char_to_wx("David Lankes"));
		tested_by.Add(char_to_wx("Lilian Lefranc"));
		tested_by.Add(char_to_wx("Sebastian Leitner"));
		tested_by.Add(char_to_wx("Olivier Lemaire"));
		tested_by.Add(char_to_wx("Gavin Lewarne"));
		tested_by.Add(char_to_wx("Gérald Maruccia"));
		tested_by.Add(char_to_wx("George Mazarakis"));
		tested_by.Add(char_to_wx("Mattias Mattsson"));
		tested_by.Add(char_to_wx("Will Meadows"));
		tested_by.Add(char_to_wx("Brad Miller"));
		tested_by.Add(char_to_wx("Ash Mitchell"));
		tested_by.Add(char_to_wx("Rob van Nieuwkerk"));
		tested_by.Add(char_to_wx("Anders Nordentoft-Madsen"));
		tested_by.Add(char_to_wx("Mauro Ottonello"));
		tested_by.Add(char_to_wx("Peter Puchner"));
		tested_by.Add(char_to_wx("Markus Raab"));
		tested_by.Add(char_to_wx("Michael Reckert"));
		tested_by.Add(char_to_wx("Greg Rooke"));
		tested_by.Add(char_to_wx("Elad Saad"));
		tested_by.Add(char_to_wx("Karim Senoucci"));
		tested_by.Add(char_to_wx("Hordur Valgardsson"));
		tested_by.Add(char_to_wx("Xenophon the Vampire"));
		tested_by.Add(char_to_wx("Simon Vannarath"));
		tested_by.Add(char_to_wx("Igor Voytovich"));
		tested_by.Add(char_to_wx("Andrew Walls"));
		tested_by.Add(char_to_wx("Andreas Weiss"));
		tested_by.Add(char_to_wx("Paul Willmott"));
		tested_by.Add(char_to_wx("Wolfgang Woehl"));
		tested_by.Add(char_to_wx("Benno Zwanenburg"));
		tested_by.Add(char_to_wx("Дима Агатов"));
		add_section(_("Tested by"), tested_by);

		sizer->Add(_notebook, wxSizerFlags().Centre().Border(wxALL, 16));
		overall_sizer->Add(sizer);
	} else {
		overall_sizer->Add(sizer, 0, wxALL, DCPOMATIC_DIALOG_BORDER);
	}

	auto buttons = CreateButtonSizer(wxOK);
	if (buttons) {
		overall_sizer->Add(buttons, 1, wxEXPAND | wxALL, 4);
	}

	SetSizerAndFit(overall_sizer);
}


/** Add a section of credits.
 *  @param name Name of section.
 *  @param credits List of names.
 */
void
AboutDialog::add_section(wxString name, wxArrayString credits)
{
	static auto first = true;
	int const N = 3;

	auto panel = new wxScrolledWindow(_notebook);
	panel->SetMaxSize(wxSize(-1, 380));
	panel->EnableScrolling(false, true);
	panel->SetScrollRate(0, 32);
	auto overall_sizer = new wxBoxSizer(wxHORIZONTAL);

	vector<wxString> strings(N);
	int c = 0;
	for (size_t i = 0; i < credits.Count(); ++i) {
		strings[c] += credits[i] + char_to_wx("\n");
		++c;
		if (c == N) {
			c = 0;
		}
	}

	for (int i = 0; i < N; ++i) {
		auto label = new wxStaticText(panel, wxID_ANY, strings[i]);
		auto sizer = new wxBoxSizer(wxVERTICAL);
		sizer->Add(label);
		overall_sizer->Add(sizer, 1, wxEXPAND | wxALL, 6);
	}

	panel->SetSizerAndFit(overall_sizer);
	_notebook->AddPage(panel, name, first);
	first = false;
}
