/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/audio_buffers.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/job.h"
#include "lib/video_content.h"
#include "lib/writer.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <memory>


using std::make_shared;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE (test_write_odd_amount_of_silence)
{
	auto content = content_factory("test/data/flat_red.png").front();
	auto film = new_test_film2 ("test_write_odd_amount_of_silence", {content});
	content->video->set_length(24);
	auto writer = make_shared<Writer>(film, shared_ptr<Job>());

	auto audio = make_shared<AudioBuffers>(6, 48000);
	audio->make_silent ();
	writer->write (audio, dcpomatic::DCPTime(1));
}

