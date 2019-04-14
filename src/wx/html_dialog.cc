/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "html_dialog.h"
#include "wx_util.h"
#include "lib/cross.h"
#include <boost/filesystem.hpp>
#include <wx/wxhtml.h>
#include <wx/fs_mem.h>
#include <iostream>

HTMLDialog::HTMLDialog (wxWindow* parent, wxString title, wxString html)
	: wxDialog (parent, wxID_ANY, title)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	wxFileSystem::AddHandler(new wxMemoryFSHandler);

	/* Add some resources that are used by HTML passed into this dialog */
	wxMemoryFSHandler::AddFile(
		"me.jpg",
		wxBitmap(std_to_wx(boost::filesystem::path(shared_path() / "me.jpg").string()), wxBITMAP_TYPE_JPEG), wxBITMAP_TYPE_JPEG
		);

	wxHtmlWindow* h = new wxHtmlWindow (this);
	h->SetPage (html);
	sizer->Add (h, 1, wxEXPAND | wxALL, 6);

	h->Bind (wxEVT_HTML_LINK_CLICKED, boost::bind(&HTMLDialog::link_clicked, this, _1));

	SetSizer (sizer);
	sizer->Layout ();

	/* Set width */
	SetSize (800, -1);

	/* Set height */
	SetSize (h->GetInternalRepresentation()->GetWidth(), h->GetInternalRepresentation()->GetHeight() + 64);
}

void
HTMLDialog::link_clicked (wxHtmlLinkEvent& ev)
{
	wxLaunchDefaultBrowser(ev.GetLinkInfo().GetHref());
}
