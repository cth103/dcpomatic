/*
    Copyright (C) 2015-2019 Carl Hetherington <cth@carlh.net>

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

#include "kdm_cpl_panel.h"
#include "wx_util.h"
#include "static_text.h"
#include "dcpomatic_button.h"
#include <libcxml/cxml.h>
#include <boost/foreach.hpp>

using std::vector;

KDMCPLPanel::KDMCPLPanel (wxWindow* parent, vector<CPLSummary> cpls)
	: wxPanel (parent, wxID_ANY)
	, _cpls (cpls)
{
	wxBoxSizer* vertical = new wxBoxSizer (wxVERTICAL);

	/* CPL choice */
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (s, this, _("CPL"), true);
	_cpl = new wxChoice (this, wxID_ANY);
	s->Add (_cpl, 1, wxEXPAND);
	_cpl_browse = new Button (this, _("Browse..."));
	s->Add (_cpl_browse, 0, wxALIGN_CENTER_VERTICAL);
	vertical->Add (s, 0, wxEXPAND | wxTOP, DCPOMATIC_SIZER_GAP + 2);

	/* CPL details */
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	add_label_to_sizer (table, this, _("DCP directory"), true);
	_dcp_directory = new StaticText (this, "");
	table->Add (_dcp_directory);
	add_label_to_sizer (table, this, _("CPL ID"), true);
	_cpl_id = new StaticText (this, "");
	table->Add (_cpl_id);
	add_label_to_sizer (table, this, _("CPL annotation text"), true);
	_cpl_annotation_text = new StaticText (this, "");
	table->Add (_cpl_annotation_text);
	vertical->Add (table, 0, wxEXPAND | wxTOP, DCPOMATIC_SIZER_GAP + 2);

	update_cpl_choice ();

	_cpl->Bind        (wxEVT_CHOICE, boost::bind (&KDMCPLPanel::update_cpl_summary, this));
	_cpl_browse->Bind (wxEVT_BUTTON,  boost::bind (&KDMCPLPanel::cpl_browse_clicked, this));

	SetSizerAndFit (vertical);
}

void
KDMCPLPanel::update_cpl_choice ()
{
	_cpl->Clear ();

	BOOST_FOREACH (CPLSummary const & i, _cpls) {
		_cpl->Append (std_to_wx(i.cpl_id));

		if (_cpls.size() > 0) {
			_cpl->SetSelection (0);
		}
	}

	update_cpl_summary ();
}

void
KDMCPLPanel::update_cpl_summary ()
{
	int const n = _cpl->GetSelection();
	if (n == wxNOT_FOUND) {
		return;
	}

	_dcp_directory->SetLabel (std_to_wx (_cpls[n].dcp_directory));
	_cpl_id->SetLabel (std_to_wx (_cpls[n].cpl_id));
	_cpl_annotation_text->SetLabel (std_to_wx (_cpls[n].cpl_annotation_text));
}

void
KDMCPLPanel::cpl_browse_clicked ()
{
	wxFileDialog* d = new wxFileDialog (this, _("Select CPL XML file"), wxEmptyString, wxEmptyString, "*.xml");
	if (d->ShowModal() == wxID_CANCEL) {
		d->Destroy ();
		return;
	}

	boost::filesystem::path cpl_file (wx_to_std (d->GetPath ()));
	boost::filesystem::path dcp_dir = cpl_file.parent_path ();

	d->Destroy ();

	/* XXX: hack alert */
	cxml::Document cpl_document ("CompositionPlaylist");
	cpl_document.read_file (cpl_file);

	bool encrypted = false;
	BOOST_FOREACH (cxml::ConstNodePtr i, cpl_document.node_children("ReelList")) {
		BOOST_FOREACH (cxml::ConstNodePtr j, i->node_children("Reel")) {
			BOOST_FOREACH (cxml::ConstNodePtr k, j->node_children("AssetList")) {
				BOOST_FOREACH (cxml::ConstNodePtr l, k->node_children()) {
					if (!l->node_children("KeyId").empty()) {
						encrypted = true;
					}
				}
			}
		}
	}

	if (!encrypted) {
		error_dialog (this, _("This CPL contains no encrypted assets."));
		return;
	}

	/* We're ignoring the CPLSummary timestamp stuff here and just putting the new one in at the end
	   of the list, then selecting it.
	*/

	try {
		_cpls.push_back (
			CPLSummary (
				dcp_dir.filename().string(),
				cpl_document.string_child("Id").substr (9),
				cpl_document.string_child("ContentTitleText"),
				cpl_file,
				encrypted,
				0
				)
			);
	} catch (cxml::Error &) {
		error_dialog (this, _("This is not a valid CPL file"));
		return;
	}

	update_cpl_choice ();
	_cpl->SetSelection (_cpls.size() - 1);
	update_cpl_summary ();
}

boost::filesystem::path
KDMCPLPanel::cpl () const
{
	int const item = _cpl->GetSelection ();
	DCPOMATIC_ASSERT (item >= 0);
	return _cpls[item].cpl_file;
}

bool
KDMCPLPanel::has_selected () const
{
	return _cpl->GetSelection() != -1;
}
