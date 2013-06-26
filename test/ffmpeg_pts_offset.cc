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

BOOST_AUTO_TEST_CASE (ffmpeg_pts_offset_test)
{
	/* Sound == video so no offset required */
	BOOST_CHECK_EQUAL (FFmpegDecoder::compute_pts_offset (0, 0, 24), 0);

	/* Common offset should be removed */
	BOOST_CHECK_CLOSE (FFmpegDecoder::compute_pts_offset (42, 42, 24), -42, 1e-9);

	/* Video is on a frame boundary */
	BOOST_CHECK_EQUAL (FFmpegDecoder::compute_pts_offset (1.0 / 24.0, 0, 24), 0);

	/* Again, video is on a frame boundary */
	BOOST_CHECK_EQUAL (FFmpegDecoder::compute_pts_offset (1.0 / 23.97, 0, 23.97), 0);

	/* And again, video is on a frame boundary */
	BOOST_CHECK_EQUAL (FFmpegDecoder::compute_pts_offset (3.0 / 23.97, 0, 23.97), 0);

	/* Off a frame boundary */
	BOOST_CHECK_CLOSE (FFmpegDecoder::compute_pts_offset (1.0 / 24.0 - 0.0215, 0, 24), 0.0215, 1e-9);
}
