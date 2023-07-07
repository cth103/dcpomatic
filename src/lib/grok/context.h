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
#include "../log.h"
#include "../dcpomatic_log.h"
#include "../writer.h"
#include "messenger.h"

class Film;
using dcp::Data;
using namespace dcpomatic;

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

struct GrokInitializer {
	GrokInitializer(void) {
		setMessengerLogger(new GrokLogger("[GROK] "));
	}
	~GrokInitializer()  = default;
};

struct FrameProxy {
	FrameProxy(int index, Eyes eyes, DCPVideo dcpv) : index_(index), eyes_(eyes), vf(dcpv)
	{}
	int index() const {
		return index_;
	}
	Eyes eyes(void) const {
		return eyes_;
	}
	int index_;
	Eyes eyes_;
	DCPVideo vf;
};

struct DcpomaticContext {
	DcpomaticContext(std::shared_ptr<const Film> film, Writer& writer,
						EventHistory &history, const std::string &location) :
									film_(film), writer_(writer),
									history_(history), location_(location),
									width_(0), height_(0)
	{}
	void setDimensions(uint32_t w, uint32_t h) {
		width_ = w;
		height_ = h;
	}
	std::shared_ptr<const Film> film_;
	Writer& writer_;
	EventHistory &history_;
	std::string location_;
	uint32_t width_;
	uint32_t height_;
};

class GrokContext {
public:
	explicit GrokContext(const DcpomaticContext &dcpomaticContext) :
								dcpomaticContext_(dcpomaticContext),
								messenger_(nullptr),
								launched_(false)
	{
		struct CompressedData : public dcp::Data {
			explicit CompressedData(int dataLen) : data_(new uint8_t[dataLen]), dataLen_(dataLen)
			{}
			~CompressedData(void){
				delete[] data_;
			}
			uint8_t const * data () const override {
				return data_;
			}
			uint8_t * data () override {
				return data_;
			}
			int size () const override {
				return dataLen_;
			}
			uint8_t *data_;
			int dataLen_;
		};
		if (Config::instance()->enable_gpu ())  {
		    boost::filesystem::path folder(dcpomaticContext_.location_);
		    boost::filesystem::path binaryPath = folder / "grk_compress";
		    if (!boost::filesystem::exists(binaryPath)) {
		    	getMessengerLogger()->error("Invalid binary location %s",
		    			dcpomaticContext_.location_.c_str());
				return;
		    }
			auto proc = [this](const std::string& str) {
				try {
					Msg msg(str);
					auto tag = msg.next();
					if(tag == GRK_MSGR_BATCH_SUBMIT_COMPRESSED)
					{
						auto clientFrameId = msg.nextUint();
						auto compressedFrameId = msg.nextUint();
						(void)compressedFrameId;
						auto compressedFrameLength = msg.nextUint();
						auto  processor =
								[this](FrameProxy srcFrame, uint8_t* compressed, uint32_t compressedFrameLength)
						{
							auto compressedData = std::make_shared<CompressedData>(compressedFrameLength);
							memcpy(compressedData->data_,compressed,compressedFrameLength );
							dcpomaticContext_.writer_.write(compressedData, srcFrame.index(), srcFrame.eyes());
							frame_done ();
						};
						int const minimum_size = 16384;
						bool needsRecompression = compressedFrameLength < minimum_size;
						messenger_->processCompressed(str, processor, needsRecompression);
						if (needsRecompression) {
							auto fp = messenger_->retrieve(clientFrameId);
							if (!fp) {
								return;
							}

							auto encoded = std::make_shared<dcp::ArrayData>(fp->vf.encode_locally());
							dcpomaticContext_.writer_.write(encoded, fp->vf.index(), fp->vf.eyes());
							frame_done ();
						}
					}
				} catch (std::exception &ex){
					getMessengerLogger()->error("%s",ex.what());
				}
			};
			auto clientInit =
				MessengerInit(clientToGrokMessageBuf, clientSentSynch, grokReceiveReadySynch,
							  grokToClientMessageBuf, grokSentSynch, clientReceiveReadySynch, proc,
							  std::thread::hardware_concurrency());
			messenger_ = new ScheduledMessenger<FrameProxy>(clientInit);
		}
	}
	~GrokContext(void) {
		shutdown();
	}
	bool launch(DCPVideo dcpv, int device){
		if (!messenger_ )
			return false;
		if (launched_)
			return true;
		std::unique_lock<std::mutex> lk_global(launchMutex);
		if (!messenger_)
			return false;
		if (launched_)
			return true;
		if (MessengerInit::firstLaunch(true)) {
			auto s = dcpv.get_size();
			dcpomaticContext_.setDimensions(s.width, s.height);
			auto config = Config::instance();
			messenger_->launchGrok(dcpomaticContext_.location_,
					dcpomaticContext_.width_,dcpomaticContext_.width_,
					dcpomaticContext_.height_,
					3, 12, device,
					dcpomaticContext_.film_->resolution() == Resolution::FOUR_K,
					dcpomaticContext_.film_->video_frame_rate(),
					dcpomaticContext_.film_->j2k_bandwidth(),
					config->gpu_license_server(),
					config->gpu_license_port(),
					config->gpu_license());
		}
		launched_ =  messenger_->waitForClientInit();

		return launched_;
	}
	bool scheduleCompress(DCPVideo const& vf){
		if (!messenger_)
			return false;

		auto fp = FrameProxy(vf.index(), vf.eyes(), vf);
		auto cvt = [this, &fp](BufferSrc src){
			// xyz conversion
			fp.vf.convert_to_xyz((uint16_t*)src.framePtr_);
		};
		return messenger_->scheduleCompress(fp, cvt);
	}
	void shutdown(void){
		if (!messenger_)
			return;

		std::unique_lock<std::mutex> lk_global(launchMutex);
		if (!messenger_)
			return;
		if (launched_)
			messenger_->shutdown();
		delete messenger_;
		messenger_ = nullptr;
	}
	void frame_done () {
		dcpomaticContext_.history_.event ();
	}
private:
	DcpomaticContext dcpomaticContext_;
	ScheduledMessenger<FrameProxy> *messenger_;
	bool launched_;
};

}

