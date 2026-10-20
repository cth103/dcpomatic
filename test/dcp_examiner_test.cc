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


#include "lib/audio_content.h"
#include "lib/audio_mapping.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_examiner.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "test.h"
#include <dcp/types.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE(check_examine_vfs)
{
	auto image = content_factory("test/data/scope_red.png")[0];
	auto ov = new_test_film("check_examine_vfs_ov", { image });
	ov->set_container(Ratio::from_id("239"));
	make_and_verify_dcp(ov);

	auto ov_dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	auto second_reel = content_factory("test/data/scope_red.png")[0];
	auto vf = new_test_film("check_examine_vfs_vf", { ov_dcp, second_reel });
	vf->set_container(Ratio::from_id("239"));
	vf->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	ov_dcp->set_reference_video(true);
	make_and_verify_dcp(vf, { dcp::VerificationNote::Code::EXTERNAL_ASSET }, false);

	auto vf_dcp = make_shared<DCPContent>(vf->dir(vf->dcp_name()));
	DCPExaminer examiner(vf_dcp, false);
}



/** HI and VI are mapped by default for an imported DCP only if the DCP has those channels */
BOOST_AUTO_TEST_CASE(dcp_default_mapping_hi_vi_test)
{
	auto check = [](string name, vector<dcp::Channel> extra, bool hi, bool vi) {
		auto source = make_shared<FFmpegContent>("test/data/L.wav");
		auto ov = new_test_film(name + "_ov", { source });
		ov->set_audio_channels(16);
		AudioMapping mapping(1, MAX_DCP_AUDIO_CHANNELS);
		mapping.set(0, dcp::Channel::CENTRE, 1);
		for (auto channel: extra) {
			mapping.set(0, channel, 1);
		}
		source->audio->set_mapping(mapping);
		make_and_verify_dcp(ov, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

		auto dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
		auto copy = new_test_film(name + "_copy", { dcp });

		auto mapped = [dcp](dcp::Channel channel) {
			auto const map = dcp->audio->stream()->mapping();
			for (int output = 0; output < map.output_channels(); ++output) {
				if (map.get(static_cast<int>(channel), output) > 0) {
					return true;
				}
			}
			return false;
		};

		BOOST_CHECK_EQUAL(mapped(dcp::Channel::HI), hi);
		BOOST_CHECK_EQUAL(mapped(dcp::Channel::VI), vi);
		/* The basic channels are always mapped */
		BOOST_CHECK(mapped(dcp::Channel::CENTRE));
	};

	check("dcp_default_mapping_none", {}, false, false);
	check("dcp_default_mapping_hi", { dcp::Channel::HI }, true, false);
	check("dcp_default_mapping_vi", { dcp::Channel::VI }, false, true);
	check("dcp_default_mapping_both", { dcp::Channel::HI, dcp::Channel::VI }, true, true);
}
