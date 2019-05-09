/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/version.h"
#include "lib/decrypted_ecinema_kdm.h"
#include "lib/config.h"
#include <dcp/key.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/aes_ctr.h>
}
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <openssl/rand.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <iostream>

using std::string;
using std::cerr;
using std::cout;
using boost::optional;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] <FILE>\n"
	     << "  -v, --version        show DCP-o-matic version\n"
	     << "  -h, --help           show this help\n"
	     << "  -o, --output         output directory\n"
	     << "\n"
	     << "<FILE> is the unencrypted .mp4 file.\n";
}

int
main (int argc, char* argv[])
{
	optional<boost::filesystem::path> output;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v'},
			{ "help", no_argument, 0, 'h'},
			{ "output", required_argument, 0, 'o'},
		};

		int c = getopt_long (argc, argv, "vho:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'v':
			cout << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
			exit (EXIT_SUCCESS);
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 'o':
			output = optarg;
			break;
		}
	}

	if (optind >= argc) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	if (!output) {
		cerr << "You must specify --output-media or -o\n";
		exit (EXIT_FAILURE);
	}

	boost::filesystem::path input = argv[optind];
	boost::filesystem::path output_mp4 = *output / (input.filename().string() + ".ecinema");

	if (!boost::filesystem::is_directory(*output)) {
		boost::filesystem::create_directory (*output);
	}

	av_register_all ();

	AVFormatContext* input_fc = avformat_alloc_context ();
	if (avformat_open_input(&input_fc, input.string().c_str(), 0, 0) < 0) {
		cerr << "Could not open input file\n";
		exit (EXIT_FAILURE);
	}

	if (avformat_find_stream_info (input_fc, 0) < 0) {
		cerr << "Could not read stream information\n";
		exit (EXIT_FAILURE);
	}

	AVFormatContext* output_fc;
	avformat_alloc_output_context2 (&output_fc, av_guess_format("mp4", 0, 0), 0, 0);

	for (uint32_t i = 0; i < input_fc->nb_streams; ++i) {
		AVStream* is = input_fc->streams[i];
		AVStream* os = avformat_new_stream (output_fc, is->codec->codec);
		if (avcodec_copy_context (os->codec, is->codec) < 0) {
			cerr << "Could not set up output stream.\n";
			exit (EXIT_FAILURE);
		}

		switch (os->codec->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
			os->time_base = is->time_base;
			os->avg_frame_rate = is->avg_frame_rate;
			os->r_frame_rate = is->r_frame_rate;
			os->sample_aspect_ratio = is->sample_aspect_ratio;
			os->codec->time_base = is->codec->time_base;
			os->codec->framerate = is->codec->framerate;
			os->codec->pix_fmt = is->codec->pix_fmt;
			break;
		case AVMEDIA_TYPE_AUDIO:
			os->codec->sample_fmt = is->codec->sample_fmt;
			os->codec->bits_per_raw_sample = is->codec->bits_per_raw_sample;
			os->codec->sample_rate = is->codec->sample_rate;
			os->codec->channel_layout = is->codec->channel_layout;
			os->codec->channels = is->codec->channels;
			break;
		default:
			/* XXX */
			break;
		}
	}

	if (avio_open2 (&output_fc->pb, output_mp4.string().c_str(), AVIO_FLAG_WRITE, 0, 0) < 0) {
		cerr << "Could not open output file `" << output_mp4.string() << "'\n";
		exit (EXIT_FAILURE);
	}

	dcp::Key key (AES_CTR_KEY_SIZE);
	AVDictionary* options = 0;
	av_dict_set (&options, "encryption_key", key.hex().c_str(), 0);

	if (avformat_write_header (output_fc, &options) < 0) {
		cerr << "Could not write header to output.\n";
		exit (EXIT_FAILURE);
	}

	AVPacket packet;
	while (av_read_frame(input_fc, &packet) >= 0) {
		AVStream* is = input_fc->streams[packet.stream_index];
		AVStream* os = output_fc->streams[packet.stream_index];
		packet.pts = av_rescale_q_rnd(packet.pts, is->time_base, os->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		packet.dts = av_rescale_q_rnd(packet.dts, is->time_base, os->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		packet.duration = av_rescale_q(packet.duration, is->time_base, os->time_base);
		packet.pos = -1;
		if (av_interleaved_write_frame (output_fc, &packet) < 0) {
			cerr << "Could not write frame to output.\n";
			exit (EXIT_FAILURE);
		}
	}

	av_write_trailer (output_fc);

	avformat_free_context (input_fc);
	avformat_free_context (output_fc);

	DecryptedECinemaKDM decrypted_kdm (key);
	EncryptedECinemaKDM encrypted_kdm = decrypted_kdm.encrypt (Config::instance()->decryption_chain());
	cout << encrypted_kdm.as_xml() << "\n";
}
