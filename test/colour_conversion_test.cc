/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/colour_conversion_test.cc
 *  @brief Various tests of ColourConversion.
 */

#include "lib/colour_conversion.h"
#include <dcp/colour_matrix.h>
#include <dcp/gamma_transfer_function.h>
#include <libxml++/libxml++.h>
#include <boost/test/unit_test.hpp>

using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (colour_conversion_test1)
{
	ColourConversion A (dcp::ColourConversion::srgb_to_xyz ());
	ColourConversion B (dcp::ColourConversion::rec709_to_xyz ());

	BOOST_CHECK_EQUAL (A.identifier(), "751dd37378fb52502d356ec334a104c8");
	BOOST_CHECK_EQUAL (B.identifier(), "621cf5bdd8de7e8adc3f4b8d77dc708a");
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
		"  <Matrix i=\"0\" j=\"0\">0.4124564</Matrix>\n"
		"  <Matrix i=\"0\" j=\"1\">0.3575761</Matrix>\n"
		"  <Matrix i=\"0\" j=\"2\">0.1804375</Matrix>\n"
		"  <Matrix i=\"1\" j=\"0\">0.2126729</Matrix>\n"
		"  <Matrix i=\"1\" j=\"1\">0.7151522</Matrix>\n"
		"  <Matrix i=\"1\" j=\"2\">0.072175</Matrix>\n"
		"  <Matrix i=\"2\" j=\"0\">0.0193339</Matrix>\n"
		"  <Matrix i=\"2\" j=\"1\">0.119192</Matrix>\n"
		"  <Matrix i=\"2\" j=\"2\">0.9503041</Matrix>\n"
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
		"    <Type>ModifiedGamma</Type>\n"
		"    <Power>2.222222222222222</Power>\n"
		"    <Threshold>0.081</Threshold>\n"
		"    <A>0.099</A>\n"
		"    <B>4.5</B>\n"
		"  </InputTransferFunction>\n"
		"  <Matrix i=\"0\" j=\"0\">0.4124564</Matrix>\n"
		"  <Matrix i=\"0\" j=\"1\">0.3575761</Matrix>\n"
		"  <Matrix i=\"0\" j=\"2\">0.1804375</Matrix>\n"
		"  <Matrix i=\"1\" j=\"0\">0.2126729</Matrix>\n"
		"  <Matrix i=\"1\" j=\"1\">0.7151522</Matrix>\n"
		"  <Matrix i=\"1\" j=\"2\">0.072175</Matrix>\n"
		"  <Matrix i=\"2\" j=\"0\">0.0193339</Matrix>\n"
		"  <Matrix i=\"2\" j=\"1\">0.119192</Matrix>\n"
		"  <Matrix i=\"2\" j=\"2\">0.9503041</Matrix>\n"
		"  <OutputGamma>2.6</OutputGamma>\n"
		"</Test>\n"
		);
}
