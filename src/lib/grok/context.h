/*
    Copyright (C) 2023 Grok Image Compression Inc.

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

#pragma once


#include "../config.h"
#include "../dcp_video.h"
#include "../film.h"
#include "../log.h"
#include "../dcpomatic_log.h"
#include "../writer.h"
#include "messenger.h"
#include <dcp/array_data.h>
#include <boost/filesystem.hpp>


static std::mutex launchMutex;

namespace grk_plugin
{

struct GrokLogger : public MessengerLogger {
	explicit GrokLogger(const std::string &preamble) : MessengerLogger(preamble)
	{}
	virtual ~GrokLogger() = default;
	void info(const char* fmt, ...) override{
		va_list arg;
		va_start(arg, fmt);
		dcpomatic_log->log(preamble_ + log_message(fmt, arg),LogEntry::TYPE_GENERAL);
		va_end(arg);
	}
	void warn(const char* fmt, ...) override{
		va_list arg;
		va_start(arg, fmt);
		dcpomatic_log->log(preamble_ + log_message(fmt, arg),LogEntry::TYPE_WARNING);
		va_end(arg);
	}
	void error(const char* fmt, ...) override{
		va_list arg;
		va_start(arg, fmt);
		dcpomatic_log->log(preamble_ + log_message(fmt, arg),LogEntry::TYPE_ERROR);
		va_end(arg);
	}
};


struct DcpomaticContext
{
	DcpomaticContext(
		std::shared_ptr<const Film> film_,
		Writer& writer_,
		EventHistory& history_,
		boost::filesystem::path const& location_
		)
		: film(film_)
		, writer(writer_)
		, history(history_)
		, location(location_)
	{

	}

	void set_dimensions(uint32_t w, uint32_t h)
	{
		width = w;
		height = h;
	}

	std::shared_ptr<const Film> film;
	Writer& writer;
	EventHistory& history;
	boost::filesystem::path location;
	uint32_t width = 0;
	uint32_t height = 0;
};


class GrokContext
{
public:
	explicit GrokContext(DcpomaticContext* dcpomatic_context)
		: _dcpomatic_context(dcpomatic_context)
	{
		auto grok = Config::instance()->grok().get_value_or({});
		if (!grok.enable) {
			return;
		}

		boost::filesystem::path folder(_dcpomatic_context->location);
		boost::filesystem::path binary_path = folder / "grk_compress";
		if (!boost::filesystem::exists(binary_path)) {
			getMessengerLogger()->error(
				"Invalid binary location %s", _dcpomatic_context->location.c_str()
				);
			return;
		}

		auto proc = [this](const std::string& str) {
			try {
				Msg msg(str);
				auto tag = msg.next();
				if (tag == GRK_MSGR_BATCH_SUBMIT_COMPRESSED) {
					auto clientFrameId = msg.nextUint();
					msg.nextUint(); // compressed frame ID
					auto compressedFrameLength = msg.nextUint();
					auto processor = [this](DCPVideo srcFrame, uint8_t* compressed, uint32_t compressedFrameLength) {
						auto compressed_data = std::make_shared<dcp::ArrayData>(compressed, compressedFrameLength);
						_dcpomatic_context->writer.write(compressed_data, srcFrame.index(), srcFrame.eyes());
						frame_done ();
					};

					int const minimum_size = 16384;

					bool needsRecompression = compressedFrameLength < minimum_size;
					_messenger->processCompressed(str, processor, needsRecompression);

					if (needsRecompression) {
						auto vf = _messenger->retrieve(clientFrameId);
						if (!vf) {
							return;
						}

						auto encoded = std::make_shared<dcp::ArrayData>(vf->encode_locally());
						_dcpomatic_context->writer.write(encoded, vf->index(), vf->eyes());
						frame_done ();
					}
				}
			} catch (std::exception& ex) {
				getMessengerLogger()->error("%s",ex.what());
			}
		};

		auto clientInit = MessengerInit(
			clientToGrokMessageBuf,
			clientSentSynch,
			grokReceiveReadySynch,
			grokToClientMessageBuf,
			grokSentSynch,
			clientReceiveReadySynch,
			proc,
			std::thread::hardware_concurrency()
			);

		_messenger = new ScheduledMessenger<DCPVideo>(clientInit);
	}

	~GrokContext()
	{
		if (!_messenger) {
			return;
		}

		std::unique_lock<std::mutex> lk_global(launchMutex);

		if (!_messenger) {
			return;
		}

		if (_launched) {
			_messenger->shutdown();
		}

		delete _messenger;
	}

	bool launch(DCPVideo dcpv, int device)
	{
		namespace fs = boost::filesystem;

		if (!_messenger) {
			return false;
		}
		if (_launched) {
			return true;
		}
		if (_launch_failed) {
			return false;
		}

		std::unique_lock<std::mutex> lk_global(launchMutex);

		if (!_messenger) {
			return false;
		}
		if (_launched) {
			return true;
		}
		if (_launch_failed) {
			return false;
		}

		if (MessengerInit::firstLaunch(true)) {

			if (!fs::exists(_dcpomatic_context->location) || !fs::is_directory(_dcpomatic_context->location)) {
				getMessengerLogger()->error("Invalid directory %s", _dcpomatic_context->location.c_str());
				return false;
			}

			auto s = dcpv.get_size();
			_dcpomatic_context->set_dimensions(s.width, s.height);
			auto grok = Config::instance()->grok().get_value_or({});
			if (!_messenger->launchGrok(
					_dcpomatic_context->location,
					_dcpomatic_context->width,
					_dcpomatic_context->width,
					_dcpomatic_context->height,
					3,
					12,
					device,
					_dcpomatic_context->film->resolution() == Resolution::FOUR_K,
					_dcpomatic_context->film->video_frame_rate(),
					_dcpomatic_context->film->video_bit_rate(VideoEncoding::JPEG2000),
					grok.licence_server,
					grok.licence_port,
					grok.licence)) {
				_launch_failed = true;
				return false;
			}
		}

		_launched = _messenger->waitForClientInit();
		_launch_failed = _launched;

		return _launched;
	}

	bool scheduleCompress(DCPVideo const& vf)
	{
		if (!_messenger) {
			return false;
		}

		auto cvt = [this, &vf](BufferSrc src) {
			vf.convert_to_xyz((uint16_t*)src.framePtr_);
		};

		return _messenger->scheduleCompress(vf, cvt);
	}

private:
	void frame_done()
	{
		_dcpomatic_context->history.event();
	}

	DcpomaticContext* _dcpomatic_context;
	ScheduledMessenger<DCPVideo>* _messenger = nullptr;
	bool _launched = false;
	bool _launch_failed = false;
};

}

