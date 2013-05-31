/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

BOOST_AUTO_TEST_CASE (make_dcp_test)
{
	shared_ptr<Film> film = new_test_film ("make_dcp_test");
	film->set_name ("test_film2");
	system ("pwd");
	film->examine_and_add_content (shared_ptr<FFmpegContent> (new FFmpegContent (film, "test/test.mp4")));

	/* Wait for the examine to finish */
	while (JobManager::instance()->work_to_do ()) {
		dcpomatic_sleep (1);
	}
	
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film->make_dcp ();
	film->write_metadata ();

	while (JobManager::instance()->work_to_do ()) {
		dcpomatic_sleep (1);
	}
	
	BOOST_CHECK_EQUAL (JobManager::instance()->errors(), false);
}

/** Test Film::have_dcp().  Requires the output from make_dcp_test above */
BOOST_AUTO_TEST_CASE (have_dcp_test)
{
	boost::filesystem::path p = test_film_dir ("make_dcp_test");
	Film f (p.string ());
	BOOST_CHECK (f.have_dcp());

	p /= f.dcp_name();
	p /= f.dcp_video_mxf_filename();
	boost::filesystem::remove (p);
	BOOST_CHECK (!f.have_dcp ());
}
