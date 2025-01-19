/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/player.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(crash_rendering_vf_interop_subs_test)
{
	auto prefix = std::string("crash_rendering_vf_interop_subs_test");

	auto video = content_factory("test/data/flat_red.png");
	auto ov = new_test_film(prefix + "_ov", video);
	ov->set_interop(true);

	make_and_verify_dcp(
		ov,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
		});

	auto ov_dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	auto subtitles = content_factory("test/data/short.srt");
	auto vf = new_test_film(prefix + "_vf", { ov_dcp, subtitles.front() });
	vf->set_interop(true);
	vf->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	ov_dcp->set_reference_video(true);
	ov_dcp->set_reference_audio(true);
	subtitles[0]->text[0]->set_language(dcp::LanguageTag("de"));

	make_and_verify_dcp(
		vf,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::EXTERNAL_ASSET,
		});

	auto vf_dcp = make_shared<DCPContent>(vf->dir(vf->dcp_name()));
	vf_dcp->add_ov(ov->dir(ov->dcp_name()));
	auto test = new_test_film(prefix + "_test", { vf_dcp });
	vf_dcp->text[0]->set_use(true);

	auto player = make_shared<Player>(test, Image::Alignment::COMPACT, false);
	player->set_always_burn_open_subtitles();
	while (!player->pass()) {}
}

