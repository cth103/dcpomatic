/*
    Copyright (C) 2014-2022 Carl Hetherington <cth@carlh.net>

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


#include "make_chain_dialog.h"
#include "static_text.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/util.h"
#include <dcp/certificate_chain.h>
#include <boost/algorithm/string.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;


MakeChainDialog::MakeChainDialog (
	wxWindow* parent,
	shared_ptr<const dcp::CertificateChain> chain
	)
	: TableDialog (parent, _("Make certificate chain"), 2, 1, true)
{
	string subject_organization_name;
	string subject_organizational_unit_name;
	string root_common_name;
	string intermediate_common_name;
	string leaf_common_name;

	auto all = chain->root_to_leaf ();

	if (all.size() >= 1) {
		/* Have a root */
		subject_organization_name = chain->root().subject_organization_name ();
		subject_organizational_unit_name = chain->root().subject_organizational_unit_name ();
		root_common_name = chain->root().subject_common_name ();
	}

	if (all.size() >= 2) {
		/* Have a leaf */
		leaf_common_name = chain->leaf().subject_common_name ();
	}

	if (all.size() >= 3) {
		/* Have an intermediate */
		dcp::CertificateChain::List::iterator i = all.begin ();
		++i;
		intermediate_common_name = i->subject_common_name ();
	}

	wxTextValidator validator (wxFILTER_EXCLUDE_CHAR_LIST);
	validator.SetCharExcludes(char_to_wx("/"));

	if (boost::algorithm::starts_with (root_common_name, ".")) {
		root_common_name = root_common_name.substr (1);
	}

	if (boost::algorithm::starts_with (intermediate_common_name, ".")) {
		intermediate_common_name = intermediate_common_name.substr (1);
	}

	if (boost::algorithm::starts_with (leaf_common_name, "CS.")) {
		leaf_common_name = leaf_common_name.substr (3);
	}

	add (_("Organisation"), true);
	add (_organisation = new wxTextCtrl (this, wxID_ANY, std_to_wx(subject_organization_name), wxDefaultPosition, wxSize (480, -1), 0, validator));
	add (_("Organisational unit"), true);
	add (_organisational_unit = new wxTextCtrl (this, wxID_ANY, std_to_wx(subject_organizational_unit_name), wxDefaultPosition, wxDefaultSize, 0, validator));

	add (_("Root common name"), true);

	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (new StaticText (this, char_to_wx(".")), 0, wxALIGN_CENTER_VERTICAL);
		s->Add (_root_common_name = new wxTextCtrl (
				this, wxID_ANY, std_to_wx (root_common_name), wxDefaultPosition, wxDefaultSize, 0, validator), 1, wxALIGN_CENTER_VERTICAL
			);
		add (s);
	}

	add (_("Intermediate common name"), true);

	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add(new StaticText(this, char_to_wx(".")), 0, wxALIGN_CENTER_VERTICAL);
		s->Add (_intermediate_common_name = new wxTextCtrl (
				this, wxID_ANY, std_to_wx (intermediate_common_name), wxDefaultPosition, wxDefaultSize, 0, validator), 1, wxALIGN_CENTER_VERTICAL
			);
		add (s);
	}

	add (_("Leaf common name"), true);

	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add(new StaticText(this, char_to_wx("CS.")), 0, wxALIGN_CENTER_VERTICAL);
		s->Add (_leaf_common_name = new wxTextCtrl (
				this, wxID_ANY, std_to_wx (leaf_common_name), wxDefaultPosition, wxDefaultSize, 0, validator), 1, wxALIGN_CENTER_VERTICAL
			);
		add (s);
	}

	layout ();

	_organisation->SetFocus ();
}


shared_ptr<dcp::CertificateChain>
MakeChainDialog::get () const
{
	return make_shared<dcp::CertificateChain>(
		openssl_path(),
		CERTIFICATE_VALIDITY_PERIOD,
		wx_to_std(_organisation->GetValue()),
		wx_to_std(_organisational_unit->GetValue()),
		"." + wx_to_std(_root_common_name->GetValue()),
		"." + wx_to_std(_intermediate_common_name->GetValue()),
		"CS." + wx_to_std(_leaf_common_name->GetValue())
		);
}
