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


#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_examiner.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/j2k_transcode.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_text_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;


BOOST_AUTO_TEST_CASE (smpte_dcp_with_subtitles_can_be_decrypted)
{
	auto content = content_factory("test/data/15s.srt");
	auto film = new_test_film("smpte_dcp_with_subtitles_can_be_decrypted", content);
	film->set_interop (false);
	film->set_encrypt_picture(true);
	film->set_encrypt_sound(true);
	film->set_encrypt_text(true);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED,
			dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_START_TIME,
		});

	dcp::DCP dcp (film->dir(film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE (cpl->file());

	auto signer = Config::instance()->signer_chain();
	BOOST_REQUIRE(signer->valid());

	auto const decrypted_kdm = film->make_kdm(*cpl->file(), dcp::LocalTime(), dcp::LocalTime());
	auto const kdm = decrypted_kdm.encrypt(signer, Config::instance()->decryption_chain()->leaf(), {}, dcp::Formulation::MODIFIED_TRANSITIONAL_1, true, 0);

	auto dcp_content = make_shared<DCPContent>(film->dir(film->dcp_name()));
	dcp_content->add_kdm (kdm);
	DCPExaminer examiner (dcp_content, false);
	BOOST_CHECK (examiner.kdm_valid());
}


BOOST_AUTO_TEST_CASE(encrypt_only_picture)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto text = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("encrypt_only_picture", { picture, text });
	film->set_encrypt_picture(true);
	film->set_encrypt_sound(false);
	film->set_encrypt_text(false);
	/* clairmeta says "Encrypted is not coherent for all reels" */
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED,
			dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
		}, true, false);

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE(cpl->file());

	auto dcp_picture = dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(cpl->reels()[0]->main_picture()->asset());
	auto reader = dcp_picture->start_read();
	auto frame = reader->get_frame(0);
	BOOST_CHECK_THROW(dcp::decompress_j2k(frame->data(), frame->size(), 0), dcp::J2KDecompressionError);
}


BOOST_AUTO_TEST_CASE(encrypt_only_sound)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto text = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("encrypt_only_picture", { picture, text });
	film->set_encrypt_picture(false);
	film->set_encrypt_sound(true);
	film->set_encrypt_text(false);
	/* clairmeta says "Encrypted is not coherent for all reels" */
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED,
			dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
		}, true, false);

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE(cpl->file());

	auto dcp_sound = dynamic_pointer_cast<dcp::SoundAsset>(cpl->reels()[0]->main_sound()->asset());
	auto reader = dcp_sound->start_read();
	auto frame = reader->get_frame(0);
	int zeros = 0;
	for (int i = 0; i < 1024; ++i) {
		zeros += frame->data()[i] == 0;
	}
	BOOST_CHECK(zeros != 1024);
}


BOOST_AUTO_TEST_CASE(encrypt_only_text)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto text = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("encrypt_only_picture", { picture, text });
	film->set_encrypt_picture(false);
	film->set_encrypt_sound(false);
	film->set_encrypt_text(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED,
			dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED,
		});

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE(cpl->file());

	auto dcp_subtitle = dynamic_pointer_cast<dcp::TextAsset>(cpl->reels()[0]->main_subtitle()->asset());
	BOOST_CHECK_THROW(dcp_subtitle->xml_as_string(), dcp::ProgrammingError);
}

