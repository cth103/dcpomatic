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


#include "variant.h"


static char const* _dcpomatic = "DCP-o-matic";
static char const* _dcpomatic_player = "DCP-o-matic Player";
static char const* _dcpomatic_kdm_creator = "DCP-o-matic KDM Creator";
static char const* _dcpomatic_verifier = "DCP-o-matic Verifier";
static char const* _dcpomatic_app = "DCP-o-matic 2.app";
static char const* _dcpomatic_player_app = "DCP-o-matic 2 Player.app";
static char const* _dcpomatic_disk_writer = "DCP-o-matic Disk Writer";
static char const* _dcpomatic_editor = "DCP-o-matic Editor";
static char const* _dcpomatic_encode_server = "DCP-o-matic Encode Server";
static char const* _dcpomatic_batch_converter_app = "DCP-o-matic 2 Batch Converter.app";
static char const* _dcpomatic_playlist_editor = "DCP-o-matic Playlist Editor";
static char const* _dcpomatic_combiner = "DCP-o-matic Combiner";
static char const* _dcpomatic_batch_converter = "DCP-o-matic Batch Converter";

static char const* _report_problem_email = "carl@dcpomatic.com";

static bool const _show_tagline = true;
static bool const _show_dcpomatic_website = true;
static bool const _show_report_a_problem = false;


std::string
variant::dcpomatic()
{
	return _dcpomatic;
}

std::string
variant::dcpomatic_batch_converter()
{
	return _dcpomatic_batch_converter;
}

std::string
variant::dcpomatic_combiner()
{
	return _dcpomatic_combiner;
}

std::string
variant::dcpomatic_disk_writer()
{
	return _dcpomatic_disk_writer;
}

std::string
variant::dcpomatic_editor()
{
	return _dcpomatic_editor;
}

std::string
variant::dcpomatic_encode_server()
{
	return _dcpomatic_encode_server;
}

std::string
variant::dcpomatic_kdm_creator()
{
	return _dcpomatic_kdm_creator;
}

std::string
variant::dcpomatic_player()
{
	return _dcpomatic_player;
}

std::string
variant::dcpomatic_playlist_editor()
{
	return _dcpomatic_playlist_editor;
}

std::string
variant::dcpomatic_verifier()
{
	return _dcpomatic_verifier;
}

std::string
variant::insert_dcpomatic(std::string const& s)
{
	return String::compose(s, _dcpomatic);
}

std::string
variant::insert_dcpomatic_encode_server(std::string const& s)
{
	return String::compose(s, _dcpomatic_encode_server);
}

std::string
variant::insert_dcpomatic_kdm_creator(std::string const& s)
{
	return String::compose(s, _dcpomatic_kdm_creator);
}

std::string
variant::dcpomatic_app()
{
	return _dcpomatic_app;
}

std::string
variant::dcpomatic_batch_converter_app()
{
	return _dcpomatic_batch_converter_app;
}

std::string
variant::dcpomatic_player_app()
{
	return _dcpomatic_player_app;
}

bool
variant::show_tagline()
{
	return _show_tagline;
}

bool
variant::show_dcpomatic_website()
{
	return _show_dcpomatic_website;
}

bool
variant::show_report_a_problem()
{
	return _show_report_a_problem;
}

std::string
variant::report_problem_email()
{
	return _report_problem_email;
}

