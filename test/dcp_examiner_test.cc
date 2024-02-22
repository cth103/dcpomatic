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


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_examiner.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(check_examine_vfs)
{
	auto image = content_factory("test/data/scope_red.png")[0];
	auto ov = new_test_film2("check_examine_vfs_ov", { image });
	ov->set_container(Ratio::from_id("239"));
	make_and_verify_dcp(ov);

	auto ov_dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	auto second_reel = content_factory("test/data/scope_red.png")[0];
	auto vf = new_test_film2("check_examine_vfs_vf", { ov_dcp, second_reel });
	vf->set_container(Ratio::from_id("239"));
	vf->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	ov_dcp->set_reference_video(true);
	make_and_verify_dcp(vf, { dcp::VerificationNote::Code::EXTERNAL_ASSET }, false);

	auto vf_dcp = make_shared<DCPContent>(vf->dir(vf->dcp_name()));
	DCPExaminer examiner(vf_dcp, false);
}

