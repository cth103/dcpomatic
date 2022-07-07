/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/fs_mem.h>
#include <wx/wxhtml.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


HTMLDialog::HTMLDialog (wxWindow* parent, wxString title, wxString html)
	: wxDialog (parent, wxID_ANY, title)
{
	auto sizer = new wxBoxSizer (wxVERTICAL);

	wxFileSystem::AddHandler(new wxMemoryFSHandler);

	/* Add some resources that are used by HTML passed into this dialog */
	wxMemoryFSHandler::AddFile(
		"me.jpg",
		wxBitmap(bitmap_path("me.jpg"), wxBITMAP_TYPE_JPEG), wxBITMAP_TYPE_JPEG
		);

	auto h = new wxHtmlWindow (this);
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
