/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/colour_conversion_test.cc
 *  @brief Test ColourConversion class.
 *  @ingroup selfcontained
 */


#include "lib/colour_conversion.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/gamma_transfer_function.h>
#include <dcp/j2k_transcode.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <libxml++/libxml++.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::make_shared;


BOOST_AUTO_TEST_CASE (colour_conversion_test1)
{
	ColourConversion A (dcp::ColourConversion::srgb_to_xyz());
	ColourConversion B (dcp::ColourConversion::rec709_to_xyz());

	BOOST_CHECK_EQUAL (A.identifier(), "ef8af1b1fda1dfe9656dc99b7a9532c7");
	BOOST_CHECK_EQUAL (B.identifier(), "e6bd82fd7ebeabe75742fbece0cbf652");
}


BOOST_AUTO_TEST_CASE (colour_conversion_test2)
{
	ColourConversion A (dcp::ColourConversion::srgb_to_xyz ());
	xmlpp::Document doc;
	auto root = doc.create_root_node ("Test");
	A.as_xml (root);
	BOOST_CHECK_EQUAL (
		doc.write_to_string_formatted ("UTF-8"),
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Test>\n"
		"  <InputTransferFunction>\n"
		"    <Type>ModifiedGamma</Type>\n"
		"    <Power>2.4</Power>\n"
		"    <Threshold>0.04045</Threshold>\n"
		"    <A>0.055</A>\n"
		"    <B>12.92</B>\n"
		"  </InputTransferFunction>\n"
		"  <YUVToRGB>0</YUVToRGB>\n"
		"  <RedX>0.64</RedX>\n"
		"  <RedY>0.33</RedY>\n"
		"  <GreenX>0.3</GreenX>\n"
		"  <GreenY>0.6</GreenY>\n"
		"  <BlueX>0.15</BlueX>\n"
		"  <BlueY>0.06</BlueY>\n"
		"  <WhiteX>0.3127</WhiteX>\n"
		"  <WhiteY>0.329</WhiteY>\n"
		"  <OutputGamma>2.6</OutputGamma>\n"
		"</Test>\n"
		);
}


BOOST_AUTO_TEST_CASE (colour_conversion_test3)
{
	ColourConversion A (dcp::ColourConversion::rec709_to_xyz());
	xmlpp::Document doc;
	auto root = doc.create_root_node ("Test");
	A.as_xml (root);
	BOOST_CHECK_EQUAL (
		doc.write_to_string_formatted ("UTF-8"),
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Test>\n"
		"  <InputTransferFunction>\n"
		"    <Type>Gamma</Type>\n"
		"    <Gamma>2.2</Gamma>\n"
		"  </InputTransferFunction>\n"
		"  <YUVToRGB>1</YUVToRGB>\n"
		"  <RedX>0.64</RedX>\n"
		"  <RedY>0.33</RedY>\n"
		"  <GreenX>0.3</GreenX>\n"
		"  <GreenY>0.6</GreenY>\n"
		"  <BlueX>0.15</BlueX>\n"
		"  <BlueY>0.06</BlueY>\n"
		"  <WhiteX>0.3127</WhiteX>\n"
		"  <WhiteY>0.329</WhiteY>\n"
		"  <OutputGamma>2.6</OutputGamma>\n"
		"</Test>\n"
		);
}


/** Test a round trip via the XML representation */
BOOST_AUTO_TEST_CASE (colour_conversion_test4)
{
	for (auto const& i: PresetColourConversion::all()) {
		xmlpp::Document out;
		auto out_root = out.create_root_node("Test");
		i.conversion.as_xml (out_root);
		auto in = make_shared<cxml::Document> ("Test");
		in->read_string (out.write_to_string("UTF-8"));
		BOOST_CHECK (ColourConversion::from_xml(in, Film::current_state_version).get() == i.conversion);
	}
}


static void check(std::shared_ptr<const dcp::OpenJPEGImage> image, int px, int py, int cx, int cy, int cz)
{
	BOOST_CHECK_EQUAL(image->data(0)[px + py * image->size().width], cx);
	BOOST_CHECK_EQUAL(image->data(1)[px + py * image->size().width], cy);
	BOOST_CHECK_EQUAL(image->data(2)[px + py * image->size().width], cz);
}


BOOST_AUTO_TEST_CASE(end_to_end_rgb_black_test)
{
	auto pattern = content_factory(TestPaths::private_data() / "BlackCal_Levels_4k.png");
	auto film = new_test_film("end_to_end_rgb_black_test", pattern);
	make_and_verify_dcp(film);

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL(cpl->reels().size(), 1U);
	auto reel = cpl->reels()[0];
	auto picture = std::dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(reel->main_picture()->j2k_asset());
	BOOST_REQUIRE(picture);

	auto reader = picture->start_read();
	auto frame = reader->get_frame(0);
	auto image = dcp::decompress_j2k(*frame.get(), 0);

	/* Border is black.  Make sure we move past the pillarboxing */
	check(image, 158, 2, 0, 0, 0);
}


BOOST_AUTO_TEST_CASE(end_to_end_rgb_white_test)
{
	auto pattern = content_factory(TestPaths::private_data() / "WhiteCal_Levels_4k.png");
	auto film = new_test_film("end_to_end_rgb_white_test", pattern);
	make_and_verify_dcp(film);

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL(cpl->reels().size(), 1U);
	auto reel = cpl->reels()[0];
	auto picture = std::dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(reel->main_picture()->j2k_asset());
	BOOST_REQUIRE(picture);

	auto reader = picture->start_read();
	auto frame = reader->get_frame(0);
	auto image = dcp::decompress_j2k(*frame.get(), 0);

	/* Border is white.  Make sure we move past the pillarboxing.
	 * These XYZ values are from the ISDCF framing chart.
	 */
	check(image, 158, 2, 3883, 3960, 4092);
}

