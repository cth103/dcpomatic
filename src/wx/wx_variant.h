/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include <wx/wx.h>


namespace variant {
namespace wx {


wxString dcpomatic();
wxString dcpomatic_batch_converter();
wxString dcpomatic_combiner();
wxString dcpomatic_disk_writer();
wxString dcpomatic_editor();
wxString dcpomatic_encode_server();
wxString dcpomatic_kdm_creator();
wxString dcpomatic_player();
wxString dcpomatic_playlist_editor();
wxString dcpomatic_verifier();

wxString insert_dcpomatic(wxString const& s);
wxString insert_dcpomatic_batch_converter(wxString const& s);
wxString insert_dcpomatic_disk_writer(wxString const& s);
wxString insert_dcpomatic_editor(wxString const& s);
wxString insert_dcpomatic_encode_server(wxString const& s);
wxString insert_dcpomatic_kdm_creator(wxString const& s);
wxString insert_dcpomatic_player(wxString const& s);
wxString insert_dcpomatic_playlist_editor(wxString const& s);
wxString insert_dcpomatic_verifier(wxString const& s);


}
}
