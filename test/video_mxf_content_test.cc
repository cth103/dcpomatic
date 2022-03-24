/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/video_mxf_content_test.cc
 *  @brief Test use of Video MXF content.
 *  @ingroup feature
 */



#include "lib/film.h"
#include "lib/video_mxf_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "test.h"
#include <dcp/mono_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::dynamic_pointer_cast;


static boost::filesystem::path ref_mxf = "test/data/scaling_test_185_185/j2c_6a2ffab1-9ea5-4428-9027-9d458363c95f.mxf";


static void note (dcp::NoteType, std::string)
{

}


/** Basic test of using video MXF content */
BOOST_AUTO_TEST_CASE (video_mxf_content_test)
{
	auto film = new_test_film ("video_mxf_content_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("video_mxf_content_test");

	auto content = content_factory(ref_mxf).front();
	auto check = dynamic_pointer_cast<VideoMXFContent> (content);
	BOOST_REQUIRE (check);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K
		});

	auto ref = make_shared<dcp::MonoPictureAsset>(ref_mxf);
	boost::filesystem::directory_iterator i ("build/test/video_mxf_content_test/video");
	auto comp = make_shared<dcp::MonoPictureAsset>(*i);
	dcp::EqualityOptions op;
	BOOST_CHECK (ref->equals (comp, op, note));
}
