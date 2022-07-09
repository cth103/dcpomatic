/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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
#include "lib/font.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(full_dcp_subtitle_font_id_test)
{
	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film2("full_dcp_subtitle_font_id_test", { dcp });

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	auto text = content[0]->only_text();
	BOOST_REQUIRE(text);

	BOOST_REQUIRE_EQUAL(text->fonts().size(), 1U);
	auto font = text->fonts().front();
	BOOST_CHECK_EQUAL(font->id(), "0_theFontId");
	BOOST_REQUIRE(font->data());
	BOOST_CHECK_EQUAL(font->data()->size(), 367112);
}


BOOST_AUTO_TEST_CASE(dcp_subtitle_font_id_test)
{
	auto subs = content_factory(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV" / "8b48f6ae-c74b-4b80-b994-a8236bbbad74_sub.mxf");
	auto film = new_test_film2("dcp_subtitle_font_id_test", subs);

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	auto text = content[0]->only_text();
	BOOST_REQUIRE(text);

	BOOST_REQUIRE_EQUAL(text->fonts().size(), 1U);
	auto font = text->fonts().front();
	BOOST_CHECK_EQUAL(font->id(), "theFontId");
	BOOST_REQUIRE(font->data());
	BOOST_CHECK_EQUAL(font->data()->size(), 367112);
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_interop_dcp)
{
	auto dcp = make_shared<DCPContent>("test/data/Iopsubs_FTR-1_F_XX-XX_MOS_2K_20220710_IOP_OV");
	auto film = new_test_film2("make_dcp_with_subs_from_interop_dcp", { dcp });
	dcp->text.front()->set_use(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		}
	);
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_smpte_dcp)
{
	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film2("make_dcp_with_subs_from_smpte_dcp", { dcp });
	dcp->text.front()->set_use(true);
	make_and_verify_dcp(film);
}

