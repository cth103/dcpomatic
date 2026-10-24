/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_examiner.h"
#include "lib/ffmpeg_file_encoder.h"
#include "lib/ffmpeg_film_encoder.h"
#include "lib/ffmpeg_wrapper.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/image_content.h"
#include "lib/ratio.h"
#include "lib/raw_image_proxy.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "lib/transcode_job.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/file.h>
#include <dcp/raw_convert.h>
#include <boost/test/unit_test.hpp>
#include <numeric>


using std::make_shared;
using std::string;
using std::vector;
using namespace dcpomatic;


/** Export must make the same rate-control decisions as x264 fed consecutive frame timestamps.
 *  Checking playback timestamps alone does not catch encoder timestamps expressed in muxer units.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_time_base)
{
	for (auto const frame_rate: {24, 25}) {
		auto const size = dcp::Size(320, 180);
		int const frames = 96;
		auto const path = fmt::format("build/test/ffmpeg_encoder_h264_time_base_{}.mp4", frame_rate);
		Cleanup cleanup;
		cleanup.add(path);
		boost::filesystem::create_directories("build/test");

		auto codec = avcodec_find_encoder_by_name("libx264");
		BOOST_REQUIRE(codec);
		auto free_context = [](AVCodecContext* context) { avcodec_free_context(&context); };
		std::unique_ptr<AVCodecContext, decltype(free_context)> reference(avcodec_alloc_context3(codec), free_context);
		BOOST_REQUIRE(reference);
		reference->width = size.width;
		reference->height = size.height;
		reference->pix_fmt = AV_PIX_FMT_YUV420P;
		reference->time_base = AVRational{1, frame_rate};
		reference->flags |= AV_CODEC_FLAG_QSCALE | AV_CODEC_FLAG_GLOBAL_HEADER;
		AVDictionary* options = nullptr;
		av_dict_set(&options, "crf", "18", 0);
		auto const opened = avcodec_open2(reference.get(), codec, &options);
		av_dict_free(&options);
		BOOST_REQUIRE_EQUAL(opened, 0);

		vector<int> reference_sizes;
		auto receive_reference = [&]() {
			ffmpeg::Packet packet;
			int result;
			while ((result = avcodec_receive_packet(reference.get(), packet.get())) == 0) {
				reference_sizes.push_back(packet->size);
				av_packet_unref(packet.get());
			}
			BOOST_REQUIRE(result == AVERROR(EAGAIN) || result == AVERROR_EOF);
		};

		auto free_frame = [](AVFrame* frame) { av_frame_free(&frame); };
		std::unique_ptr<AVFrame, decltype(free_frame)> frame(av_frame_alloc(), free_frame);
		BOOST_REQUIRE(frame);
		frame->format = AV_PIX_FMT_YUV420P;
		frame->width = size.width;
		frame->height = size.height;
		BOOST_REQUIRE_EQUAL(av_frame_get_buffer(frame.get(), 32), 0);
		{
			FFmpegFileEncoder encoder(size, frame_rate, 48000, 2, ExportFormat::H264_AAC, false, 18, path);
			for (int n = 0; n < frames; ++n) {
				auto image = make_shared<Image>(AV_PIX_FMT_YUV420P, size, Image::Alignment::PADDED);
				BOOST_REQUIRE_EQUAL(av_frame_make_writable(frame.get()), 0);
				for (int plane = 0; plane < 3; ++plane) {
					int const width = plane ? size.width / 2 : size.width;
					int const height = plane ? size.height / 2 : size.height;
					for (int y = 0; y < height; ++y) {
						for (int x = 0; x < width; ++x) {
							auto const value = plane ? 128 : 16 + ((x + n) * 13 + y * 7 + (x + n) * y) % 220;
							image->data()[plane][y * image->stride()[plane] + x] = value;
							frame->data[plane][y * frame->linesize[plane] + x] = value;
						}
					}
				}
				auto video = make_shared<PlayerVideo>(
					make_shared<RawImageProxy>(image), Crop(), boost::optional<double>(), size, size,
					Eyes::BOTH, Part::WHOLE, boost::optional<ColourConversion>(), VideoRange::VIDEO,
					std::weak_ptr<Content>(), boost::optional<ContentTime>(), false
					);
				encoder.video(video, DCPTime::from_frames(n, frame_rate));
				frame->pts = n;
				BOOST_REQUIRE_EQUAL(avcodec_send_frame(reference.get(), frame.get()), 0);
				receive_reference();
			}
			encoder.flush();
		}
		BOOST_REQUIRE_EQUAL(avcodec_send_frame(reference.get(), nullptr), 0);
		receive_reference();

		AVFormatContext* input = nullptr;
		BOOST_REQUIRE_EQUAL(avformat_open_input(&input, path.c_str(), nullptr, nullptr), 0);
		auto close_input = [](AVFormatContext* context) { avformat_close_input(&context); };
		std::unique_ptr<AVFormatContext, decltype(close_input)> input_owner(input, close_input);
		BOOST_REQUIRE_EQUAL(avformat_find_stream_info(input, nullptr), 0);
		auto const stream = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		BOOST_REQUIRE(stream >= 0);
		auto const time_base = input->streams[stream]->time_base;
		vector<int> exported_sizes;
		vector<bool> seen(frames, false);
		int64_t previous_dts = AV_NOPTS_VALUE;
		ffmpeg::Packet packet;
		int result;
		while ((result = av_read_frame(input, packet.get())) == 0) {
			if (packet->stream_index == stream) {
				exported_sizes.push_back(packet->size);
				BOOST_REQUIRE(packet->pts != AV_NOPTS_VALUE);
				BOOST_REQUIRE(packet->dts != AV_NOPTS_VALUE);
				auto const n = av_rescale_q(packet->pts, time_base, AVRational{1, frame_rate});
				BOOST_REQUIRE(n >= 0 && n < frames);
				BOOST_CHECK(!seen[n]);
				seen[n] = true;
				BOOST_CHECK_EQUAL(packet->pts, av_rescale_q(n, AVRational{1, frame_rate}, time_base));
				BOOST_CHECK_EQUAL(packet->duration, av_rescale_q(1, AVRational{1, frame_rate}, time_base));
				BOOST_CHECK(previous_dts == AV_NOPTS_VALUE || packet->dts > previous_dts);
				previous_dts = packet->dts;
			}
			av_packet_unref(packet.get());
		}
		BOOST_CHECK_EQUAL(result, AVERROR_EOF);
		BOOST_REQUIRE_EQUAL(reference_sizes.size(), frames);
		BOOST_REQUIRE_EQUAL(exported_sizes.size(), frames);
		/* MP4 replaces Annex B start codes with NAL lengths, so allow a small framing overhead. */
		auto const reference_bytes = std::accumulate(reference_sizes.begin(), reference_sizes.end(), int64_t{0});
		auto const exported_bytes = std::accumulate(exported_sizes.begin(), exported_sizes.end(), int64_t{0});
		BOOST_CHECK_CLOSE_FRACTION(double(exported_bytes), double(reference_bytes), 0.01);
		input_owner.reset();
		cleanup.run();
	}
}


static void
ffmpeg_content_test(int number, boost::filesystem::path content, ExportFormat format)
{
	Cleanup cl;

	string name = "ffmpeg_encoder_";
	string extension;
	switch (format) {
	case ExportFormat::H264_AAC:
		name += "h264";
		extension = "mp4";
		break;
	case ExportFormat::PRORES_4444:
		name += "prores-444";
		extension = "mov";
		break;
	case ExportFormat::PRORES_HQ:
		name += "prores-hq";
		extension = "mov";
		break;
	case ExportFormat::PRORES_LT:
		name += "prores-lt";
		extension = "mov";
		break;
	case ExportFormat::SUBTITLES_DCP:
	case ExportFormat::WAV_16:
	case ExportFormat::WAV_24:
		BOOST_REQUIRE(false);
	}

	name = fmt::format("{}_test{}", name, number);

	auto c = make_shared<FFmpegContent>(content);
	auto film = new_test_film(name, {c}, &cl);
	film->set_name(name);
	film->set_audio_channels(6);

	film->write_metadata();
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	auto file = boost::filesystem::path("build") / "test" / fmt::format("{}.{}", name, extension);
	cl.add(file);
	FFmpegFilmEncoder encoder(film, job, file, format, false, false, false, 23);
	encoder.go();

	cl.run();
}


/** Red / green / blue MP4 -> Prores */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_test1)
{
	ffmpeg_content_test(1, "test/data/test.mp4", ExportFormat::PRORES_HQ);
}


/** Dolby Aurora trailer VOB -> Prores */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_test2)
{
	ffmpeg_content_test(2, TestPaths::private_data() / "dolby_aurora.vob", ExportFormat::PRORES_HQ);
}


/** Sintel trailer -> Prores */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_test3)
{
	ffmpeg_content_test(3, TestPaths::private_data() / "Sintel_Trailer1.480p.DivX_Plus_HD.mkv", ExportFormat::PRORES_HQ);
}


/** Big Buck Bunny trailer -> Prores */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_test4)
{
	ffmpeg_content_test(4, TestPaths::private_data() / "big_buck_bunny_trailer_480p.mov", ExportFormat::PRORES_HQ);
}


/** Still image -> Prores */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_test5)
{
	boost::filesystem::path const output = "build/test/ffmpeg_encoder_prores_test5.mov";
	Cleanup cl;
	cl.add(output);

	auto c = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_prores_test5", { c });
	film->set_audio_channels(6);

	c->video->set_length(240);

	film->write_metadata();
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, output, ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go();

	cl.run();
}


/** Subs -> Prores */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_test6)
{
	auto s = make_shared<StringTextFileContent>("test/data/subrip2.srt");
	auto film = new_test_film("ffmpeg_encoder_prores_test6", { s });
	film->set_audio_channels(6);

	s->only_text()->set_colour(dcp::Colour(255, 255, 0));
	s->only_text()->set_effect(dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour(dcp::Colour(0, 255, 255));
	film->write_metadata();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_prores_test6.mov", ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go();
}


/** Video + subs -> Prores */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_test7)
{
	boost::filesystem::path const output = "build/test/ffmpeg_encoder_prores_test7.mov";
	Cleanup cl;
	cl.add(output);

	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	auto s = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film("ffmpeg_encoder_prores_test7", { c, s });
	film->set_audio_channels(6);

	s->only_text()->set_colour(dcp::Colour(255, 255, 0));
	s->only_text()->set_effect(dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour(dcp::Colour(0, 255, 255));

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, output, ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go();

	cl.run();
}


/** Red / green / blue MP4 -> H264 */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test1)
{
	ffmpeg_content_test(1, "test/data/test.mp4", ExportFormat::H264_AAC);
}


/** Just subtitles -> H264 */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test2)
{
	auto s = make_shared<StringTextFileContent>("test/data/subrip2.srt");
	auto film = new_test_film("ffmpeg_encoder_h264_test2", { s });
	film->set_audio_channels(6);

	s->only_text()->set_colour(dcp::Colour(255, 255, 0));
	s->only_text()->set_effect(dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour(dcp::Colour(0, 255, 255));
	film->write_metadata();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test2.mp4", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go();
}


/** Video + subs -> H264 */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test3)
{
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	auto s = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film("ffmpeg_encoder_h264_test3", { c, s });
	film->set_audio_channels(6);

	s->only_text()->set_colour(dcp::Colour(255, 255, 0));
	s->only_text()->set_effect(dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour(dcp::Colour(0, 255, 255));
	film->write_metadata();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test3.mp4", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go();
}


/** Scope-in-flat DCP -> H264 */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test4)
{
	auto film = new_test_film("ffmpeg_encoder_h264_test4", {make_shared<DCPContent>("test/data/scope_dcp")});
	BOOST_REQUIRE(!wait_for_jobs());

	film->set_container(Ratio::from_id("185"));

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test4.mp4", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go();
}


/** Test mixdown from 5.1 to stereo */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test5)
{
	auto L = make_shared<FFmpegContent>("test/data/L.wav");
	auto R = make_shared<FFmpegContent>("test/data/R.wav");
	auto C = make_shared<FFmpegContent>("test/data/C.wav");
	auto Ls = make_shared<FFmpegContent>("test/data/Ls.wav");
	auto Rs = make_shared<FFmpegContent>("test/data/Rs.wav");
	auto Lfe = make_shared<FFmpegContent>("test/data/Lfe.wav");

	auto film = new_test_film("ffmpeg_encoder_h264_test5", { L, R, C, Ls, Rs, Lfe });
	film->set_audio_channels(6);

	AudioMapping map(1, MAX_DCP_AUDIO_CHANNELS);

	L->set_position(film, DCPTime::from_seconds(0));
	map.make_zero();
	map.set(0, 0, 1);
	L->audio->set_mapping(map);
	R->set_position(film, DCPTime::from_seconds(1));
	map.make_zero();
	map.set(0, 1, 1);
	R->audio->set_mapping(map);
	C->set_position(film, DCPTime::from_seconds(2));
	map.make_zero();
	map.set(0, 2, 1);
	C->audio->set_mapping(map);
	Lfe->set_position(film, DCPTime::from_seconds(3));
	map.make_zero();
	map.set(0, 3, 1);
	Lfe->audio->set_mapping(map);
	Ls->set_position(film, DCPTime::from_seconds(4));
	map.make_zero();
	map.set(0, 4, 1);
	Ls->audio->set_mapping(map);
	Rs->set_position(film, DCPTime::from_seconds(5));
	map.make_zero();
	map.set(0, 5, 1);
	Rs->audio->set_mapping(map);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test5.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();

	check_ffmpeg("build/test/ffmpeg_encoder_h264_test5.mp4", "test/data/ffmpeg_encoder_h264_test5.mp4", -43);
}


/** Test export of a VF */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test6)
{
	auto film = new_test_film("ffmpeg_encoder_h264_test6_ov");
	film->examine_and_add_content({make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png")});
	BOOST_REQUIRE(!wait_for_jobs());
	make_and_verify_dcp(film);

	auto film2 = new_test_film("ffmpeg_encoder_h264_test6_vf");
	auto ov = make_shared<DCPContent>("build/test/ffmpeg_encoder_h264_test6_ov/" + film->dcp_name(false));
	film2->examine_and_add_content({ov});
	BOOST_REQUIRE(!wait_for_jobs());
	ov->set_reference_video(true);
	auto subs = content_factory("test/data/subrip.srt")[0];
	film2->examine_and_add_content({subs});
	BOOST_REQUIRE(!wait_for_jobs());
	for (auto i: subs->text) {
		i->set_use(true);
	}

	auto job = make_shared<TranscodeJob>(film2, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film2, job, "build/test/ffmpeg_encoder_h264_test6_vf.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();
}


/** Test export of a 3D DCP in a 2D project */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_3d_dcp_to_h264)
{
	boost::filesystem::path const output = "build/test/ffmpeg_encoder_3d_dcp_to_h264.mp4";
	Cleanup cl;
	cl.add(output);

	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "xm");
	auto film2 = new_test_film("ffmpeg_encoder_3d_dcp_to_h264_export", {dcp});

	auto job = make_shared<TranscodeJob>(film2, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film2, job, output, ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();

	cl.run();
}


/** Test export of a 3D DCP in a 2D project */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test7)
{
	auto L = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto R = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_h264_test7_data", {L, R});
	L->video->set_frame_type(VideoFrameType::THREE_D_LEFT);
	L->set_position(film, DCPTime());
	R->video->set_frame_type(VideoFrameType::THREE_D_RIGHT);
	R->set_position(film, DCPTime());
	film->set_three_d(true);
	make_and_verify_dcp(film);

	auto dcp = make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film("ffmpeg_encoder_h264_test7_export", {dcp});

	auto job = make_shared<TranscodeJob>(film2, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film2, job, "build/test/ffmpeg_encoder_h264_test7.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();
}


BOOST_AUTO_TEST_CASE(ffmpeg_encoder_2d_content_in_3d_project)
{
	auto content = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_2d_content_in_3d_project", { content });
	film->set_three_d(true);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_2d_content_in_3d_project.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();
}


/** Stereo project with mixdown-to-stereo set */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test8)
{
	auto film = new_test_film("ffmpeg_encoder_h264_test4");
	film->examine_and_add_content({make_shared<DCPContent>("test/data/scope_dcp")});
	BOOST_REQUIRE(!wait_for_jobs());
	film->set_audio_channels(2);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test8.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();
}


/** 7.1/HI/VI (i.e. 12-channel) project */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_test9)
{
	auto c = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_h264_test9", { c });
	film->set_name("ffmpeg_encoder_h264_test9");
	film->set_container(Ratio::from_id("185"));
	film->set_audio_channels(12);

	film->examine_and_add_content({c});
	BOOST_REQUIRE(!wait_for_jobs());

	c->video->set_length(240);

	film->write_metadata();
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test9.mov", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go();
}


/** DCP -> Prores with crop */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_from_dcp_with_crop)
{
	auto dcp = make_shared<DCPContent>("test/data/import_dcp_test2");
	auto film = new_test_film("ffmpeg_encoder_prores_from_dcp_with_crop", { dcp });
	dcp->video->set_left_crop(32);
	dcp->video->set_right_crop(32);
	film->write_metadata();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_prores_from_dcp_with_crop.mov", ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go();
}


/** DCP -> H264 with crop */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_from_dcp_with_crop)
{
	auto dcp = make_shared<DCPContent>("test/data/import_dcp_test2");
	auto film = new_test_film("ffmpeg_encoder_h264_from_dcp_with_crop", { dcp });
	dcp->video->set_left_crop(32);
	dcp->video->set_right_crop(32);
	film->write_metadata();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_prores_from_dcp_with_crop.mov", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go();
}


/** Export to H264 with reels */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_h264_with_reels)
{
	auto content1 = content_factory("test/data/flat_red.png")[0];
	auto content2 = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("ffmpeg_encoder_h264_with_reels", { content1, content2 });
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	content1->video->set_length(240);
	content2->video->set_length(240);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_with_reels.mov", ExportFormat::H264_AAC, false, true, false, 23);
	encoder.go();

	auto check = [](boost::filesystem::path path) {
		auto reel = std::dynamic_pointer_cast<FFmpegContent>(content_factory(path)[0]);
		BOOST_REQUIRE(reel);
		FFmpegExaminer examiner(reel);
		BOOST_CHECK_EQUAL(examiner.video_length(), 240U);
	};

	check("build/test/ffmpeg_encoder_h264_with_reels_reel1.mov");
	check("build/test/ffmpeg_encoder_h264_with_reels_reel2.mov");
}


/** Regression test for "Error during decoding: Butler finished" (#2097) */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_regression_1)
{
	Cleanup cl;

	auto content = content_factory(TestPaths::private_data() / "arrietty_JP-EN.mkv")[0];
	auto film = new_test_film("ffmpeg_encoder_prores_regression_1", { content });

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_prores_regression_1.mov", ExportFormat::PRORES_HQ, false, true, false, 23);
	encoder.go();

	cl.add("build/test/ffmpeg_encoder_prores_regression_1.mov");
	cl.run();
}


/** Regression test for Butler video buffers reached 480 frames (audio is 0) (#2101) */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_prores_regression_2)
{
	Cleanup cl;

	auto logs = dcpomatic_log->types();
	dcpomatic_log->set_types(logs | LogEntry::TYPE_DEBUG_PLAYER);

	auto content = content_factory(TestPaths::private_data() / "tge_clip.mkv")[0];
	auto film = new_test_film("ffmpeg_encoder_prores_regression_2", { content });

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_prores_regression_2.mov", ExportFormat::PRORES_HQ, false, true, false, 23);
	encoder.go();

	dcpomatic_log->set_types(logs);

	cl.add("build/test/ffmpeg_encoder_prores_regression_2.mov");
	cl.run();
}


BOOST_AUTO_TEST_CASE(ffmpeg_encoder_missing_frame_at_end)
{
	auto content = content_factory(TestPaths::private_data() / "1s1f.mov");
	auto film = new_test_film("ffmpeg_encoder_missing_frame_at_end", content);

	boost::filesystem::path output("build/test/ffmpeg_encoder_missing_frame_at_end.mov");
	boost::filesystem::path log("build/test/ffmpeg_encoder_missing_frame_at_end.log");

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, output, ExportFormat::PRORES_HQ, false, true, false, 23);
	encoder.go();

	run_ffprobe(output, log, false, "-show_frames -show_format -show_streams -select_streams v:0");

	dcp::File read_log(log, "r");
	BOOST_REQUIRE(read_log);

	int nb_read_frames = 0;
	int nb_frames = 0;
	char buffer[256];

	while (!read_log.eof()) {
		read_log.gets(buffer, sizeof(buffer));
		vector<string> parts;
		boost::algorithm::split(parts, buffer, boost::is_any_of("="));
		if (parts.size() == 2) {
			if (parts[0] == "nb_read_frames") {
				nb_read_frames = dcp::raw_convert<int>(parts[1]);
			} else if (parts[0] == "nb_frames") {
				nb_frames = dcp::raw_convert<int>(parts[1]);
			}
		}
	}

	BOOST_CHECK_EQUAL(nb_frames, 26);
	BOOST_CHECK_EQUAL(nb_read_frames, 26);
}


BOOST_AUTO_TEST_CASE(test_ffmpeg_encoder_with_clipping_dcp_audio)
{
	string const name = "test_ffmpeg_encoder_with_clipping_dcp_audio";
	auto content = content_factory("test/data/sine_16_48_440_10.wav");
	auto film = new_test_film(name + "_in", content);
	film->set_audio_channels(8);
	content[0]->audio->set_gain(0.5);

	make_and_verify_dcp(
		film,
		{ dcp::VerificationNote::Code::MISSING_CPL_METADATA }
	);

	auto dcp = make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film(name + "_out", { dcp });

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	auto const out = boost::filesystem::path("build/test") / (name + "_out.mov");
	FFmpegFilmEncoder encoder(film, job, out, ExportFormat::PRORES_HQ, false, true, false, 23);
	encoder.go();

	check_ffmpeg(out, boost::filesystem::path("test/data") / (name + ".mov"), -96);
}

/** Aurora trailer -> WAV */
BOOST_AUTO_TEST_CASE(ffmpeg_encoder_vob_to_wav_test)
{
	Cleanup cl;

	auto const name = fmt::format("ffmpeg_encoder_vob_to_wav_test");

	auto content = make_shared<FFmpegContent>(TestPaths::private_data() / "dolby_aurora.vob");
	auto film = new_test_film(name, {content}, &cl);
	film->set_name(name);
	film->set_audio_channels(6);
	film->write_metadata();

	auto make_wav = [&cl, film, name](int bits, ExportFormat format) {
		auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
		auto const file = boost::filesystem::path("build") / "test" / fmt::format("{}_{}.wav", name, bits);
		cl.add(file);

		FFmpegFilmEncoder encoder(film, job, file, format, false, false, false, 0);
		encoder.go();
		check_ffmpeg(file, TestPaths::private_data() / fmt::format("{}_{}.wav", name, bits), -88);
	};

	make_wav(16, ExportFormat::WAV_16);
	make_wav(24, ExportFormat::WAV_24);
	cl.run();
}


