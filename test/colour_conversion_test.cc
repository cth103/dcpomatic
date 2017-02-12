/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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
 *  @brief Various tests of ColourConversion.
 */

#include "lib/colour_conversion.h"
#include "lib/film.h"
#include <dcp/gamma_transfer_function.h>
#include <libxml++/libxml++.h>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (colour_conversion_test1)
{
	ColourConversion A (dcp::ColourConversion::srgb_to_xyz ());
	ColourConversion B (dcp::ColourConversion::rec709_to_xyz ());

	BOOST_CHECK_EQUAL (A.identifier(), "9840c601d2775bf1b3847254bbaa36a9");
	BOOST_CHECK_EQUAL (B.identifier(), "58151ac92fdf333663a62c9a8ba5c5f4");
}

BOOST_AUTO_TEST_CASE (colour_conversion_test2)
{
	ColourConversion A (dcp::ColourConversion::srgb_to_xyz ());
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Test");
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
	ColourConversion A (dcp::ColourConversion::rec709_to_xyz ());
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Test");
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
	BOOST_FOREACH (PresetColourConversion const & i, PresetColourConversion::all ()) {
		xmlpp::Document out;
		xmlpp::Element* out_root = out.create_root_node ("Test");
		i.conversion.as_xml (out_root);
		shared_ptr<cxml::Document> in (new cxml::Document ("Test"));
		in->read_string (out.write_to_string ("UTF-8"));
		BOOST_CHECK (ColourConversion::from_xml (in, Film::current_state_version).get () == i.conversion);
	}
}
