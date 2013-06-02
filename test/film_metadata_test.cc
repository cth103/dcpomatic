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

	shared_ptr<Film> f (new Film (test_film));
	f->_dci_date = boost::gregorian::from_undelimited_string ("20130211");
	BOOST_CHECK (f->container() == 0);
	BOOST_CHECK (f->dcp_content_type() == 0);

	f->set_name ("fred");
	f->set_dcp_content_type (DCPContentType::from_pretty_name ("Short"));
	f->set_container (Ratio::from_id ("185"));
	f->set_ab (true);
	f->write_metadata ();

	stringstream s;
	s << "diff -u test/data/metadata.xml.ref " << test_film << "/metadata.xml";
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);

	shared_ptr<Film> g (new Film (test_film));
	g->read_metadata ();

	BOOST_CHECK_EQUAL (g->name(), "fred");
	BOOST_CHECK_EQUAL (g->dcp_content_type(), DCPContentType::from_pretty_name ("Short"));
	BOOST_CHECK_EQUAL (g->container(), Ratio::from_id ("185"));
	BOOST_CHECK_EQUAL (g->ab(), true);
	
	g->write_metadata ();
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);
}
