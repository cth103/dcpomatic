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


#include "wx_util.h"
#include "wx_variant.h"
#include "lib/variant.h"



wxString
variant::wx::dcpomatic()
{
	return std_to_wx(variant::dcpomatic());
}

wxString
variant::wx::dcpomatic_batch_converter()
{
	return std_to_wx(variant::dcpomatic_batch_converter());
}

wxString
variant::wx::dcpomatic_combiner()
{
	return std_to_wx(variant::dcpomatic_combiner());
}

wxString
variant::wx::dcpomatic_disk_writer()
{
	return std_to_wx(variant::dcpomatic_disk_writer());
}

wxString
variant::wx::dcpomatic_editor()
{
	return std_to_wx(variant::dcpomatic_editor());
}

wxString
variant::wx::dcpomatic_encode_server()
{
	return std_to_wx(variant::dcpomatic_encode_server());
}

wxString
variant::wx::dcpomatic_kdm_creator()
{
	return std_to_wx(variant::dcpomatic_kdm_creator());
}

wxString
variant::wx::dcpomatic_player()
{
	return std_to_wx(variant::dcpomatic_player());
}

wxString
variant::wx::dcpomatic_playlist_editor()
{
	return std_to_wx(variant::dcpomatic_playlist_editor());
}

wxString
variant::wx::dcpomatic_verifier()
{
	return std_to_wx(variant::dcpomatic_verifier());
}

wxString
variant::wx::insert_dcpomatic(wxString const& s)
{
	return wxString::Format(s, dcpomatic());
}

wxString
variant::wx::insert_dcpomatic_batch_converter(wxString const& s)
{
	return wxString::Format(s, dcpomatic_batch_converter());
}

wxString
variant::wx::insert_dcpomatic_disk_writer(wxString const& s)
{
	return wxString::Format(s, dcpomatic_disk_writer());
}

wxString
variant::wx::insert_dcpomatic_editor(wxString const& s)
{
	return wxString::Format(s, dcpomatic_editor());
}

wxString
variant::wx::insert_dcpomatic_encode_server(wxString const& s)
{
	return wxString::Format(s, dcpomatic_encode_server());
}

wxString
variant::wx::insert_dcpomatic_kdm_creator(wxString const& s)
{
	return wxString::Format(s, dcpomatic_kdm_creator());
}

wxString
variant::wx::insert_dcpomatic_player(wxString const& s)
{
	return wxString::Format(s, dcpomatic_player());
}

wxString
variant::wx::insert_dcpomatic_playlist_editor(wxString const& s)
{
	return wxString::Format(s, dcpomatic_playlist_editor());
}

wxString
variant::wx::insert_dcpomatic_verifier(wxString const& s)
{
	return wxString::Format(s, dcpomatic_verifier());
}
