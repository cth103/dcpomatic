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
#include "lib/film.h"
#include "test.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <asdcp/Metadata.h>
LIBDCP_ENABLE_WARNINGS
#include <dcp/sound_asset.h>
#include <dcp/util.h>
#include <boost/test/unit_test.hpp>


using std::shared_ptr;
using std::string;


bool
has_cpl_text(shared_ptr<const Film> film, string const& text)
{
	auto cpl = dcp::file_to_string(find_file(film->dir(film->dcp_name()), "cpl_"));
	return cpl.find(text) != std::string::npos;
}


bool
has_mxf_mca_subdescriptors(shared_ptr<const Film> film)
{
	/* One day hopefully libdcp will read these descriptors and we can find out from the SoundAsset
	 * whether they exist.
	 */

	Kumu::FileReaderFactory factory;
	ASDCP::PCM::MXFReader reader(factory);
	auto r = reader.OpenRead(find_file(film->dir(film->dcp_name()), "pcm_").string());
	BOOST_REQUIRE(!ASDCP_FAILURE(r));

	ASDCP::MXF::WaveAudioDescriptor* essence_descriptor = nullptr;
	auto const rr = reader.OP1aHeader().GetMDObjectByType(
		dcp::asdcp_smpte_dict->ul(ASDCP::MDD_WaveAudioDescriptor),
		reinterpret_cast<ASDCP::MXF::InterchangeObject**>(&essence_descriptor)
		);

	if (!KM_SUCCESS(rr)) {
		return false;
	}

	return essence_descriptor->SubDescriptors.size() > 0;
}


string
constraints_profile(shared_ptr<const Film> film)
{
	auto cpl = dcp::file_to_string(find_file(film->dir(film->dcp_name()), "cpl_"));
	cxml::Document xml("CompositionPlaylist");
	xml.read_string(cpl);

	auto reel_list = xml.node_child("ReelList");
	if (!reel_list) {
		return {};
	}

	auto reel = reel_list->node_child("Reel");
	if (!reel) {
		return {};
	}

	auto asset_list = reel->node_child("AssetList");
	if (!asset_list) {
		return {};
	}

	auto meta_asset = asset_list->node_child("CompositionMetadataAsset");
	if (!meta_asset) {
		return {};
	}

	auto extension = meta_asset->node_child("ExtensionMetadataList");
	if (!extension) {
		return {};
	}

	auto metadata = extension->node_child("ExtensionMetadata");
	if (!metadata) {
		return {};
	}

	auto property_list = metadata->node_child("PropertyList");
	if (!property_list) {
		return {};
	}

	auto property = property_list->node_child("Property");
	if (!property) {
		return {};
	}

	if (auto value = property->optional_string_child("Value")) {
		return *value;
	}

	return {};
}

bool
has_cpl_mca_subdescriptors(shared_ptr<const Film> film)
{
       return has_cpl_text(film, "MCASubDescriptors");
}


bool
has_rating_list(shared_ptr<const Film> film)
{
       return has_cpl_text(film, "RatingList");
}


bool
has_cpl_metadata(shared_ptr<const Film> film)
{
       return has_cpl_text(film, "CompositionMetadataAsset");
}


bool
has_markers(shared_ptr<const Film> film)
{
       return has_cpl_text(film, "MainMarkers");
}


bool
has_bv21_profile(shared_ptr<const Film> film)
{
       return has_cpl_text(film, "SMPTE-RDD-52:2020-Bv2.1");
}


BOOST_AUTO_TEST_CASE(bv21_done_right)
{
	auto picture = content_factory("test/data/flat_red.png");
	auto sound = content_factory("test/data/sine_440.wav");
	auto film = new_test_film("bv21_done_right", { picture.front(), sound.front() });

	make_and_verify_dcp(film);

	BOOST_CHECK(has_cpl_mca_subdescriptors(film));
	BOOST_CHECK(has_mxf_mca_subdescriptors(film));
	BOOST_CHECK(constraints_profile(film) == "SMPTE-RDD-52:2020-Bv2.1");

}


BOOST_AUTO_TEST_CASE(bv20_done_right)
{
	auto picture = content_factory("test/data/flat_red.png");
	auto sound = content_factory("test/data/sine_440.wav");
	auto film = new_test_film("bv20_done_right", { picture.front(), sound.front () });
	film->set_smpte_profile(dcp::Profile::SMPTE_BV20);

	make_and_verify_dcp(film);

	BOOST_CHECK(!has_cpl_mca_subdescriptors(film));
	BOOST_CHECK(!has_mxf_mca_subdescriptors(film));
	BOOST_CHECK(constraints_profile(film) == "SMPTE-RDD-52:2020-Bv2.0");
}


BOOST_AUTO_TEST_CASE(a_done_right)
{
       auto picture = content_factory("test/data/flat_red.png");
       auto sound = content_factory("test/data/sine_440.wav");
       auto film = new_test_film("a_done_right", { picture.front(), sound.front () });
       film->set_smpte_profile(dcp::Profile::SMPTE_A);

       make_and_verify_dcp(
               film,
               {
                       dcp::VerificationNote::Code::MISSING_FFOC,
                       dcp::VerificationNote::Code::MISSING_LFOC,
                       dcp::VerificationNote::Code::MISSING_CPL_METADATA,
               });

       BOOST_CHECK(!has_cpl_mca_subdescriptors(film));
       BOOST_CHECK(!has_mxf_mca_subdescriptors(film));
       BOOST_CHECK(!has_bv21_profile(film));
       BOOST_CHECK(!has_cpl_metadata(film));
       BOOST_CHECK(!has_markers(film));
}
