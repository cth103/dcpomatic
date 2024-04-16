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


#include "compose.hpp"


namespace variant
{

std::string dcpomatic();
std::string dcpomatic_batch_converter();
std::string dcpomatic_combiner();
std::string dcpomatic_disk_writer();
std::string dcpomatic_editor();
std::string dcpomatic_encode_server();
std::string dcpomatic_kdm_creator();
std::string dcpomatic_player();
std::string dcpomatic_playlist_editor();
std::string dcpomatic_verifier();

std::string insert_dcpomatic(std::string const& s);
std::string insert_dcpomatic_encode_server(std::string const& s);
std::string insert_dcpomatic_kdm_creator(std::string const& s);

std::string dcpomatic_app();
std::string dcpomatic_batch_converter_app();
std::string dcpomatic_player_app();

}

