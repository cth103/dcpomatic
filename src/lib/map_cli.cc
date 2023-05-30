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


#include "compose.hpp"
#include "config.h"
#include "util.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/font_asset.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/reel_file_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <boost/optional.hpp>
#include <getopt.h>
#include <algorithm>
#include <memory>
#include <string>


using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using std::make_shared;
using std::vector;
using boost::optional;


static void
help(std::function<void (string)> out)
{
	out(String::compose("Syntax: %1 [OPTION} <cpl-file> [<cpl-file> ... ]", program_name));
	out("  -V, --version    show libdcp version");
	out("  -h, --help       show this help");
	out("  -o, --output     output directory");
	out("  -l, --hard-link  using hard links instead of copying");
	out("  -s, --soft-link  using soft links instead of copying");
	out("  -d, --assets-dir look in this directory for assets (can be given more than once)");
	out("  -r, --rename     rename all files to <uuid>.<mxf|xml>");
	out("  --config <dir>   directory containing config.xml and cinemas.xml");
}


optional<string>
map_cli(int argc, char* argv[], std::function<void (string)> out)
{
	optional<boost::filesystem::path> output_dir;
	bool hard_link = false;
	bool soft_link = false;
	bool rename = false;
	vector<boost::filesystem::path> assets_dir;
	optional<boost::filesystem::path> config_dir;

	/* This makes it possible to call getopt several times in the same executable, for tests */
	optind = 0;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "output", required_argument, 0, 'o' },
			{ "hard-link", no_argument, 0, 'l' },
			{ "soft-link", no_argument, 0, 's' },
			{ "assets-dir", required_argument, 0, 'd' },
			{ "rename", no_argument, 0, 'r' },
			{ "config", required_argument, 0, 'c' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long(argc, argv, "ho:lsd:rc:", long_options, &option_index);

		if (c == -1) {
			break;
		} else if (c == '?' || c == ':') {
			exit(EXIT_FAILURE);
		}

		switch (c) {
		case 'h':
			help(out);
			exit(EXIT_SUCCESS);
		case 'o':
			output_dir = optarg;
			break;
		case 'l':
			hard_link = true;
			break;
		case 's':
			soft_link = true;
			break;
		case 'd':
			assets_dir.push_back(optarg);
			break;
		case 'r':
			rename = true;
			break;
		case 'c':
			config_dir = optarg;
			break;
		}
	}

	program_name = argv[0];

	if (argc <= optind) {
		help(out);
		exit(EXIT_FAILURE);
	}

	if (config_dir) {
		State::override_path = *config_dir;
	}

	vector<boost::filesystem::path> cpl_filenames;
	for (int i = optind; i < argc; ++i) {
		cpl_filenames.push_back(argv[i]);
	}

	if (cpl_filenames.empty()) {
		return string{"No CPL specified."};
	}

	if (!output_dir) {
		return string{"Missing -o or --output"};
	}

	if (boost::filesystem::exists(*output_dir)) {
		return String::compose("Output directory %1 already exists.", *output_dir);
	}

	if (hard_link && soft_link) {
		return string{"Specify either -s,--soft-link or -l,--hard-link, not both."};
	}

	boost::system::error_code ec;
	boost::filesystem::create_directory(*output_dir, ec);
	if (ec) {
		return String::compose("Could not create output directory %1: %2", *output_dir, ec.message());
	}

	/* Find all the assets in the asset directories.  This assumes that the asset directories are in fact
	 * DCPs (with AssetMaps and so on).  We could search for assets ourselves here but interop fonts are
	 * a little tricky because they don't contain their own UUID within the DCP.
	 */
	vector<shared_ptr<dcp::Asset>> assets;
	for (auto dir: assets_dir) {
		dcp::DCP dcp(dir);
		dcp.read();
		auto dcp_assets = dcp.assets(true);
		std::copy(dcp_assets.begin(), dcp_assets.end(), back_inserter(assets));
	}

	dcp::DCP dcp(*output_dir);

	/* Find all the CPLs */
	vector<shared_ptr<dcp::CPL>> cpls;
	for (auto filename: cpl_filenames) {
		try {
			auto cpl = make_shared<dcp::CPL>(filename);
			cpl->resolve_refs(assets);
			cpls.push_back(cpl);
		} catch (std::exception& e) {
			return String::compose("Could not read CPL %1: %2", filename, e.what());
		}
	}

	class CopyError : public std::runtime_error
	{
	public:
		CopyError(std::string message) : std::runtime_error(message) {}
	};

	auto maybe_copy = [&assets, output_dir](
		string asset_id,
		bool rename,
		bool hard_link,
		bool soft_link,
		boost::optional<boost::filesystem::path> extra = boost::none
		) {
		auto iter = std::find_if(assets.begin(), assets.end(), [asset_id](shared_ptr<const dcp::Asset> a) { return a->id() == asset_id; });
		if (iter != assets.end()) {
			DCP_ASSERT((*iter)->file());

			auto const input_path = (*iter)->file().get();
			boost::filesystem::path output_path = *output_dir;
			if (extra) {
				output_path /= *extra;
			}

			if (rename) {
				output_path /= String::compose("%1%2", (*iter)->id(), boost::filesystem::extension((*iter)->file().get()));
				(*iter)->rename_file(output_path);
			} else {
				output_path /= (*iter)->file()->filename();
			}

			boost::filesystem::create_directories(output_path.parent_path());

			boost::system::error_code ec;
			if (hard_link) {
				boost::filesystem::create_hard_link(input_path, output_path, ec);
				if (ec) {
					throw CopyError(String::compose("Could not hard-link asset %1: %2", input_path.string(), ec.message()));
				}
			} else if (soft_link) {
				boost::filesystem::create_symlink(input_path, output_path, ec);
				if (ec) {
					throw CopyError(String::compose("Could not soft-link asset %1: %2", input_path.string(), ec.message()));
				}
			} else {
				boost::filesystem::copy_file(input_path, output_path, ec);
				if (ec) {
					throw CopyError(String::compose("Could not copy asset %1: %2", input_path.string(), ec.message()));
				}
			}
			(*iter)->set_file(output_path);
		} else {
			boost::system::error_code ec;
			boost::filesystem::remove_all(*output_dir, ec);
			throw CopyError(String::compose("Could not find required asset %1", asset_id));
		}
	};

	auto maybe_copy_from_reel = [output_dir, &maybe_copy](
		shared_ptr<dcp::ReelFileAsset> asset,
		bool rename,
		bool hard_link,
		bool soft_link,
		boost::optional<boost::filesystem::path> extra = boost::none
		) {
		if (asset && asset->asset_ref().resolved()) {
			maybe_copy(asset->asset_ref().id(), rename, hard_link, soft_link, extra);
		}
	};

	/* Copy assets that the CPLs need */
	try {
		for (auto cpl: cpls) {
			for (auto reel: cpl->reels()) {
				maybe_copy_from_reel(reel->main_picture(), rename, hard_link, soft_link);
				maybe_copy_from_reel(reel->main_sound(), rename, hard_link, soft_link);
				boost::optional<boost::filesystem::path> extra;
				if (reel->main_subtitle()) {
					auto interop = dynamic_pointer_cast<dcp::InteropSubtitleAsset>(reel->main_subtitle()->asset());
					if (interop) {
						extra = interop->id();
						for (auto font_asset: interop->font_assets()) {
							maybe_copy(font_asset->id(), rename, hard_link, soft_link, extra);
						}
					}
				}
				maybe_copy_from_reel(reel->main_subtitle(), rename, hard_link, soft_link, extra);
				for (auto ccap: reel->closed_captions()) {
					maybe_copy_from_reel(ccap, rename, hard_link, soft_link);
				}
				maybe_copy_from_reel(reel->atmos(), rename, hard_link, soft_link);
			}

			dcp.add(cpl);
		}
	} catch (CopyError& e) {
		return string{e.what()};
	}

	dcp.resolve_refs(assets);
	dcp.set_annotation_text(cpls[0]->annotation_text().get_value_or(""));
	dcp.write_xml(Config::instance()->signer_chain());

	return {};
}

