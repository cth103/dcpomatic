/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

BOOST_AUTO_TEST_CASE (film_metadata_test)
{
	string const test_film = "build/test/film_metadata_test";
	
	if (boost::filesystem::exists (test_film)) {
		boost::filesystem::remove_all (test_film);
	}

	BOOST_CHECK_THROW (new Film (test_film, true), OpenFileError);
	
	shared_ptr<Film> f (new Film (test_film, false));
	f->_dci_date = boost::gregorian::from_undelimited_string ("20130211");
	BOOST_CHECK (f->format() == 0);
	BOOST_CHECK (f->dcp_content_type() == 0);
	BOOST_CHECK (f->filters ().empty());

	f->set_name ("fred");
	BOOST_CHECK_THROW (f->set_content ("jim"), OpenFileError);
	f->set_dcp_content_type (DCPContentType::from_pretty_name ("Short"));
	f->set_format (Format::from_nickname ("Flat"));
	f->set_left_crop (1);
	f->set_right_crop (2);
	f->set_top_crop (3);
	f->set_bottom_crop (4);
	vector<Filter const *> f_filters;
	f_filters.push_back (Filter::from_id ("pphb"));
	f_filters.push_back (Filter::from_id ("unsharp"));
	f->set_filters (f_filters);
	f->set_trim_start (42);
	f->set_trim_end (99);
	f->set_dcp_ab (true);
	f->write_metadata ();

	stringstream s;
	s << "diff -u test/metadata.ref " << test_film << "/metadata";
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);

	shared_ptr<Film> g (new Film (test_film, true));

	BOOST_CHECK_EQUAL (g->name(), "fred");
	BOOST_CHECK_EQUAL (g->dcp_content_type(), DCPContentType::from_pretty_name ("Short"));
	BOOST_CHECK_EQUAL (g->format(), Format::from_nickname ("Flat"));
	BOOST_CHECK_EQUAL (g->crop().left, 1);
	BOOST_CHECK_EQUAL (g->crop().right, 2);
	BOOST_CHECK_EQUAL (g->crop().top, 3);
	BOOST_CHECK_EQUAL (g->crop().bottom, 4);
	vector<Filter const *> g_filters = g->filters ();
	BOOST_CHECK_EQUAL (g_filters.size(), 2);
	BOOST_CHECK_EQUAL (g_filters.front(), Filter::from_id ("pphb"));
	BOOST_CHECK_EQUAL (g_filters.back(), Filter::from_id ("unsharp"));
	BOOST_CHECK_EQUAL (g->trim_start(), 42);
	BOOST_CHECK_EQUAL (g->trim_end(), 99);
	BOOST_CHECK_EQUAL (g->dcp_ab(), true);
	
	g->write_metadata ();
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);
}
