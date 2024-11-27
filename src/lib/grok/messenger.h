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

#include <iostream>
#include <string>
#include <cstring>
#include <atomic>
#include <functional>
#include <sstream>
#include <future>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cassert>
#include <cstdarg>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <tlhelp32.h>
#pragma warning(disable : 4100)
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#endif

namespace grk_plugin
{
static std::string grokToClientMessageBuf = "Global\\grok_to_client_message";
static std::string grokSentSynch = "Global\\grok_sent";
static std::string clientReceiveReadySynch = "Global\\client_receive_ready";
static std::string clientToGrokMessageBuf = "Global\\client_to_grok_message";
static std::string clientSentSynch = "Global\\client_sent";
static std::string grokReceiveReadySynch = "Global\\grok_receive_ready";
static std::string grokUncompressedBuf = "Global\\grok_uncompressed_buf";
static std::string grokCompressedBuf = "Global\\grok_compressed_buf";
static const std::string GRK_MSGR_BATCH_IMAGE = "GRK_MSGR_BATCH_IMAGE";
static const std::string GRK_MSGR_BATCH_COMPRESS_INIT = "GRK_MSGR_BATCH_COMPRESS_INIT";
static const std::string GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED = "GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED";
static const std::string GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED =
	"GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED";
static const std::string GRK_MSGR_BATCH_SUBMIT_COMPRESSED = "GRK_MSGR_BATCH_SUBMIT_COMPRESSED";
static const std::string GRK_MSGR_BATCH_PROCESSSED_COMPRESSED =
	"GRK_MSGR_BATCH_PROCESSSED_COMPRESSED";
static const std::string GRK_MSGR_BATCH_SHUTDOWN = "GRK_MSGR_BATCH_SHUTDOWN";
static const std::string GRK_MSGR_BATCH_FLUSH = "GRK_MSGR_BATCH_FLUSH";
static const size_t messageBufferLen = 256;
struct IMessengerLogger
{
	virtual ~IMessengerLogger(void) = default;
	virtual void info(const char* fmt, ...) = 0;
	virtual void warn(const char* fmt, ...) = 0;
	virtual void error(const char* fmt, ...) = 0;

  protected:
	template<typename... Args>
	std::string log_message(char const* const format, Args&... args) noexcept
	{
		constexpr size_t message_size = 512;
		char message[message_size];

		std::snprintf(message, message_size, format, args...);
		return std::string(message);
	}
};
struct MessengerLogger : public IMessengerLogger
{
	explicit MessengerLogger(const std::string &preamble) : preamble_(preamble) {}
	virtual ~MessengerLogger() = default;
	virtual void info(const char* fmt, ...) override
	{
		va_list args;
		std::string new_fmt = preamble_ + fmt + "\n";
		va_start(args, fmt);
		vfprintf(stdout, new_fmt.c_str(), args);
		va_end(args);
	}
	virtual void warn(const char* fmt, ...) override
	{
		va_list args;
		std::string new_fmt = preamble_ + fmt + "\n";
		va_start(args, fmt);
		vfprintf(stdout, new_fmt.c_str(), args);
		va_end(args);
	}
	virtual void error(const char* fmt, ...) override
	{
		va_list args;
		std::string new_fmt = preamble_ + fmt + "\n";
		va_start(args, fmt);
		vfprintf(stderr, new_fmt.c_str(), args);
		va_end(args);
	}

  protected:
	std::string preamble_;
};

extern IMessengerLogger* sLogger;
void setMessengerLogger(IMessengerLogger* logger);
IMessengerLogger* getMessengerLogger(void);

struct MessengerInit
{
	MessengerInit(const std::string &outBuf, const std::string &outSent,
				  const std::string &outReceiveReady, const std::string &inBuf,
				  const std::string &inSent,
				  const std::string &inReceiveReady,
				  std::function<void(std::string)> processor,
				  size_t numProcessingThreads)
		: outboundMessageBuf(outBuf), outboundSentSynch(outSent),
		  outboundReceiveReadySynch(outReceiveReady), inboundMessageBuf(inBuf),
		  inboundSentSynch(inSent), inboundReceiveReadySynch(inReceiveReady), processor_(processor),
		  numProcessingThreads_(numProcessingThreads),
		  uncompressedFrameSize_(0), compressedFrameSize_(0),
		  numFrames_(0)
	{
		if(firstLaunch(true))
			unlink();
	}
	void unlink(void)
	{
#ifndef _WIN32
		shm_unlink(grokToClientMessageBuf.c_str());
		shm_unlink(clientToGrokMessageBuf.c_str());
#endif
	}
	static bool firstLaunch(bool isClient)
	{
		bool debugGrok = false;
		return debugGrok != isClient;
	}
	std::string outboundMessageBuf;
	std::string outboundSentSynch;
	std::string outboundReceiveReadySynch;

	std::string inboundMessageBuf;
	std::string inboundSentSynch;
	std::string inboundReceiveReadySynch;

	std::function<void(std::string)> processor_;
	size_t numProcessingThreads_;

	size_t uncompressedFrameSize_;
	size_t compressedFrameSize_;
	size_t numFrames_;
};

/*************************** Synchronization *******************************/
enum SynchDirection
{
	SYNCH_SENT,
	SYNCH_RECEIVE_READY
};

typedef int grk_handle;
struct Synch
{
	Synch(const std::string &sentSemName, const std::string &receiveReadySemName)
		: sentSemName_(sentSemName), receiveReadySemName_(receiveReadySemName)
	{
		// unlink semaphores in case of previous crash
		if(MessengerInit::firstLaunch(true))
			unlink();
		open();
	}
	~Synch()
	{
		close();
		if(MessengerInit::firstLaunch(true))
			unlink();
	}
	void post(SynchDirection dir)
	{
		auto sem = (dir == SYNCH_SENT ? sentSem_ : receiveReadySem_);
		int rc = sem_post(sem);
		if(rc)
			getMessengerLogger()->error("Error posting to semaphore: %s", strerror(errno));
	}
	void wait(SynchDirection dir)
	{
		auto sem = dir == SYNCH_SENT ? sentSem_ : receiveReadySem_;
		int rc = sem_wait(sem);
		if(rc)
			getMessengerLogger()->error("Error waiting for semaphore: %s", strerror(errno));
	}
	void open(void)
	{
		sentSem_ = sem_open(sentSemName_.c_str(), O_CREAT, 0666, 0);
		if(!sentSem_)
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
		receiveReadySem_ = sem_open(receiveReadySemName_.c_str(), O_CREAT, 0666, 1);
		if(!receiveReadySem_)
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
	}
	void close(void)
	{
		int rc = sem_close(sentSem_);
		if(rc)
			getMessengerLogger()->error("Error closing semaphore %s: %s", sentSemName_.c_str(),
										strerror(errno));
		rc = sem_close(receiveReadySem_);
		if(rc)
			getMessengerLogger()->error("Error closing semaphore %s: %s",
										receiveReadySemName_.c_str(), strerror(errno));
	}
	void unlink(void)
	{
		int rc = sem_unlink(sentSemName_.c_str());
		if(rc == -1 && errno != ENOENT)
			getMessengerLogger()->error("Error unlinking semaphore %s: %s", sentSemName_.c_str(),
										strerror(errno));
		rc = sem_unlink(receiveReadySemName_.c_str());
		if(rc == -1 && errno != ENOENT)
			getMessengerLogger()->error("Error unlinking semaphore %s: %s",
										receiveReadySemName_.c_str(), strerror(errno));
	}
	sem_t* sentSem_;
	sem_t* receiveReadySem_;

  private:
	std::string sentSemName_;
	std::string receiveReadySemName_;
};
struct SharedMemoryManager
{
	static bool initShm(const std::string &name, size_t len, grk_handle* shm_fd, char** buffer)
	{
		*shm_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
		if(*shm_fd < 0)
		{
			getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
			return false;
		}
		int rc = ftruncate(*shm_fd, sizeof(char) * len);
		if(rc)
		{
			getMessengerLogger()->error("Error truncating shared memory: %s", strerror(errno));
			rc = close(*shm_fd);
			if(rc)
				getMessengerLogger()->error("Error closing shared memory: %s", strerror(errno));
			rc = shm_unlink(name.c_str());
			// 2 == No such file or directory
			if(rc && errno != 2)
				getMessengerLogger()->error("Error unlinking shared memory: %s", strerror(errno));
			return false;
		}
		*buffer = static_cast<char*>(mmap(0, len, PROT_WRITE, MAP_SHARED, *shm_fd, 0));
		if(!*buffer)
		{
			getMessengerLogger()->error("Error mapping shared memory: %s", strerror(errno));
			rc = close(*shm_fd);
			if(rc)
				getMessengerLogger()->error("Error closing shared memory: %s", strerror(errno));
			rc = shm_unlink(name.c_str());
			// 2 == No such file or directory
			if(rc && errno != 2)
				getMessengerLogger()->error("Error unlinking shared memory: %s", strerror(errno));
		}

		return *buffer != nullptr;
	}
	static bool deinitShm(const std::string &name, size_t len, grk_handle &shm_fd, char** buffer)
	{
		if (!*buffer || !shm_fd)
			return true;

		int rc = munmap(*buffer, len);
		*buffer = nullptr;
		if(rc)
			getMessengerLogger()->error("Error unmapping shared memory %s: %s", name.c_str(), strerror(errno));
		rc = close(shm_fd);
		shm_fd = 0;
		if(rc)
			getMessengerLogger()->error("Error closing shared memory %s: %s", name.c_str(), strerror(errno));
		rc = shm_unlink(name.c_str());
		// 2 == No such file or directory
		if(rc && errno != 2)
			fprintf(stderr,"Error unlinking shared memory %s : %s\n", name.c_str(), strerror(errno));

		return true;
	}
};

template<typename Data>
class MessengerBlockingQueue
{
  public:
	explicit MessengerBlockingQueue(size_t max) : active_(true), max_size_(max) {}
	MessengerBlockingQueue() : MessengerBlockingQueue(UINT_MAX) {}
	size_t size() const
	{
		return queue_.size();
	}
	// deactivate and clear queue
	void deactivate()
	{
		{
			std::lock_guard<std::mutex> lk(mutex_);
			active_ = false;
			while(!queue_.empty())
				queue_.pop();
		}

		// release all waiting threads
		can_pop_.notify_all();
		can_push_.notify_all();
	}
	void activate()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		active_ = true;
	}
	bool push(Data const& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			rc = push_(value);
		}
		if(rc)
			can_pop_.notify_one();

		return rc;
	}
	bool waitAndPush(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			if(!active_)
				return false;
			// in case of spurious wakeup, loop until predicate in lambda
			// is satisfied.
			can_push_.wait(lk, [this] { return queue_.size() < max_size_ || !active_; });
			rc = push_(value);
		}
		if(rc)
			can_pop_.notify_one();

		return rc;
	}
	bool pop(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			rc = pop_(value);
		}
		if(rc)
			can_push_.notify_one();

		return rc;
	}
	bool waitAndPop(Data& value)
	{
		bool rc;
		{
			std::unique_lock<std::mutex> lk(mutex_);
			if(!active_)
				return false;
			// in case of spurious wakeup, loop until predicate in lambda
			// is satisfied.
			can_pop_.wait(lk, [this] { return !queue_.empty() || !active_; });
			rc = pop_(value);
		}
		if(rc)
			can_push_.notify_one();

		return rc;
	}

  private:
	bool push_(Data const& value)
	{
		if(queue_.size() == max_size_ || !active_)
			return false;
		queue_.push(value);

		return true;
	}
	bool pop_(Data& value)
	{
		if(queue_.empty() || !active_)
			return false;
		value = queue_.front();
		queue_.pop();

		return true;
	}
	std::queue<Data> queue_;
	mutable std::mutex mutex_;
	std::condition_variable can_pop_;
	std::condition_variable can_push_;
	bool active_;
	size_t max_size_;
};
struct BufferSrc
{
	BufferSrc(void) : BufferSrc("") {}
	explicit BufferSrc(const std::string &file) : file_(file), clientFrameId_(0), frameId_(0), framePtr_(nullptr)
	{}
	BufferSrc(size_t clientFrameId, size_t frameId, uint8_t* framePtr)
		: file_(""), clientFrameId_(clientFrameId), frameId_(frameId), framePtr_(framePtr)
	{}
	bool fromDisk(void)
	{
		return !file_.empty() && framePtr_ == nullptr;
	}
	size_t index() const
	{
		return clientFrameId_;
	}
	std::string file_;
	size_t clientFrameId_;
	size_t frameId_;
	uint8_t* framePtr_;
};

struct Messenger;
static void outboundThread(Messenger* messenger, const std::string &sendBuf, Synch* synch);
static void inboundThread(Messenger* messenger, const std::string &receiveBuf, Synch* synch);
static void processorThread(Messenger* messenger, std::function<void(std::string)> processor);

struct Messenger
{
	explicit Messenger(MessengerInit init)
		: running(true), _initialized(false), _shutdown(false), init_(init),
		  outboundSynch_(nullptr),
		  inboundSynch_(nullptr), uncompressed_buffer_(nullptr), compressed_buffer_(nullptr),
		  uncompressed_fd_(0), compressed_fd_(0)
	{}
	virtual ~Messenger(void)
	{
		running = false;
		sendQueue.deactivate();
		receiveQueue.deactivate();

		if (outboundSynch_) {
			outboundSynch_->post(SYNCH_RECEIVE_READY);
			outbound.join();
		}

		if (inboundSynch_) {
			inboundSynch_->post(SYNCH_SENT);
			inbound.join();
		}

		for(auto& p : processors_)
			p.join();

		delete outboundSynch_;
		delete inboundSynch_;

		deinitShm();
	}
	void startThreads(void) {
		outboundSynch_ =
			new Synch(init_.outboundSentSynch, init_.outboundReceiveReadySynch);
		outbound = std::thread(outboundThread, this, init_.outboundMessageBuf, outboundSynch_);

		inboundSynch_ =
			new Synch(init_.inboundSentSynch, init_.inboundReceiveReadySynch);
		inbound = std::thread(inboundThread, this, init_.inboundMessageBuf, inboundSynch_);

		for(size_t i = 0; i < init_.numProcessingThreads_; ++i)
			processors_.push_back(std::thread(processorThread, this, init_.processor_));
	}
	bool initBuffers(void)
	{
		bool rc = true;
		if(init_.uncompressedFrameSize_)
		{
			rc = rc && SharedMemoryManager::initShm(grokUncompressedBuf,
													init_.uncompressedFrameSize_ * init_.numFrames_,
													&uncompressed_fd_, &uncompressed_buffer_);
		}
		if(init_.compressedFrameSize_)
		{
			rc = rc && SharedMemoryManager::initShm(grokCompressedBuf,
													init_.compressedFrameSize_ * init_.numFrames_,
													&compressed_fd_, &compressed_buffer_);
		}

		return rc;
	}

	bool deinitShm(void)
	{
		bool rc = SharedMemoryManager::deinitShm(grokUncompressedBuf,
												 init_.uncompressedFrameSize_ * init_.numFrames_,
												 uncompressed_fd_, &uncompressed_buffer_);
		rc = rc && SharedMemoryManager::deinitShm(grokCompressedBuf,
												  init_.compressedFrameSize_ * init_.numFrames_,
												  compressed_fd_, &compressed_buffer_);

		return rc;
	}
	template<typename... Args>
	void send(const std::string& str, Args... args)
	{
		std::ostringstream oss;
		oss << str;
		int dummy[] = {0, ((void)(oss << ',' << args), 0)...};
		static_cast<void>(dummy);

		sendQueue.push(oss.str());
	}

	bool launchGrok(
		boost::filesystem::path const& dir,
		uint32_t width,
		uint32_t stride,
		uint32_t height,
		uint32_t samplesPerPixel,
		uint32_t depth,
		int device,
		bool is4K,
		uint32_t fps,
		uint32_t bandwidth,
		const std::string server,
		uint32_t port,
		const std::string license
		)
	{

		std::unique_lock<std::mutex> lk(shutdownMutex_);
		if (async_result_.valid())
			return true;
		if(MessengerInit::firstLaunch(true))
			init_.unlink();
		startThreads();
		char _cmd[4096];
		auto fullServer = server + ":" + std::to_string(port);
		sprintf(_cmd,
				"./grk_compress -batch_src %s,%d,%d,%d,%d,%d -out_fmt j2k -k 1 "
				"-G %d -%s %d,%d -j %s -J %s -v",
				GRK_MSGR_BATCH_IMAGE.c_str(), width, stride, height, samplesPerPixel, depth,
				device, is4K ? "cinema4K" : "cinema2K", fps, bandwidth,
				license.c_str(), fullServer.c_str());

		return launch(_cmd, dir);
	}
	void initClient(size_t uncompressedFrameSize, size_t compressedFrameSize, size_t numFrames)
	{
		// client fills queue with pending uncompressed buffers
		init_.uncompressedFrameSize_ = uncompressedFrameSize;
		init_.compressedFrameSize_ = compressedFrameSize;
		init_.numFrames_ = numFrames;
		initBuffers();
		auto ptr = uncompressed_buffer_;
		for(size_t i = 0; i < init_.numFrames_; ++i)
		{
			availableBuffers_.push(BufferSrc(0, i, (uint8_t*)ptr));
			ptr += init_.uncompressedFrameSize_;
		}

		std::unique_lock<std::mutex> lk(shutdownMutex_);
		_initialized = true;
		clientInitializedCondition_.notify_all();
	}

	bool waitForClientInit()
	{
		if (_initialized) {
			return true;
		} else if (_shutdown) {
			return false;
		}

		std::unique_lock<std::mutex> lk(shutdownMutex_);

		if (_initialized) {
			return true;
		} else if (_shutdown) {
			return false;
		}

		while (true) {
			if (clientInitializedCondition_.wait_for(lk, std::chrono::seconds(1), [this]{ return _initialized || _shutdown; })) {
				break;
			}
			auto status = async_result_.wait_for(std::chrono::milliseconds(100));
			if (status == std::future_status::ready) {
				getMessengerLogger()->error("Grok exited unexpectedly during initialization");
				return false;
			}
		}

		return _initialized && !_shutdown;
	}

	static size_t uncompressedFrameSize(uint32_t w, uint32_t h, uint32_t samplesPerPixel)
	{
		return sizeof(uint16_t) * w * h * samplesPerPixel;
	}
	void reclaimCompressed(size_t frameId)
	{
		availableBuffers_.push(BufferSrc(0, frameId, getCompressedFrame(frameId)));
	}
	void reclaimUncompressed(size_t frameId)
	{
		availableBuffers_.push(BufferSrc(0, frameId, getUncompressedFrame(frameId)));
	}
	uint8_t* getUncompressedFrame(size_t frameId)
	{
		assert(frameId < init_.numFrames_);
		if(frameId >= init_.numFrames_)
			return nullptr;

		return (uint8_t*)(uncompressed_buffer_ + frameId * init_.uncompressedFrameSize_);
	}
	uint8_t* getCompressedFrame(size_t frameId)
	{
		assert(frameId < init_.numFrames_);
		if(frameId >= init_.numFrames_)
			return nullptr;

		return (uint8_t*)(compressed_buffer_ + frameId * init_.compressedFrameSize_);
	}
	std::atomic_bool running;
	bool _initialized;
	bool _shutdown;
	MessengerBlockingQueue<std::string> sendQueue;
	MessengerBlockingQueue<std::string> receiveQueue;
	MessengerBlockingQueue<BufferSrc> availableBuffers_;
	MessengerInit init_;
	std::string cmd_;
	std::future<int> async_result_;
	std::mutex shutdownMutex_;
	std::condition_variable shutdownCondition_;

  protected:
	std::condition_variable clientInitializedCondition_;
  private:
	bool launch(std::string const& cmd, boost::filesystem::path const& dir)
	{
		// Change the working directory
		if(!dir.empty())
		{
			boost::system::error_code ec;
			boost::filesystem::current_path(dir, ec);
			if (ec) {
				getMessengerLogger()->error("Error: failed to change the working directory");
				return false;
			}
		}
		// Execute the command using std::async and std::system
		cmd_ = cmd;
		getMessengerLogger()->info(cmd.c_str());
		async_result_ = std::async(std::launch::async, [this]() { return std::system(cmd_.c_str()); });
		bool success = async_result_.valid();
		if (!success)
			getMessengerLogger()->error("Grok launch failed");

		return success;

	}
	std::thread outbound;
	Synch* outboundSynch_;

	std::thread inbound;
	Synch* inboundSynch_;

	std::vector<std::thread> processors_;
	char* uncompressed_buffer_;
	char* compressed_buffer_;

	grk_handle uncompressed_fd_;
	grk_handle compressed_fd_;
};

static void outboundThread(Messenger* messenger, const std::string &sendBuf, Synch* synch)
{
	grk_handle shm_fd = 0;
	char* send_buffer = nullptr;

	if(!SharedMemoryManager::initShm(sendBuf, messageBufferLen, &shm_fd, &send_buffer))
		return;
	while(messenger->running)
	{
		synch->wait(SYNCH_RECEIVE_READY);
		if(!messenger->running)
			break;
		std::string message;
		if(!messenger->sendQueue.waitAndPop(message))
			break;
		if(!messenger->running)
			break;
		memcpy(send_buffer, message.c_str(), message.size() + 1);
		synch->post(SYNCH_SENT);
	}
	SharedMemoryManager::deinitShm(sendBuf, messageBufferLen, shm_fd, &send_buffer);
}

static void inboundThread(Messenger* messenger, const std::string &receiveBuf, Synch* synch)
{
	grk_handle shm_fd = 0;
	char* receive_buffer = nullptr;

	if(!SharedMemoryManager::initShm(receiveBuf, messageBufferLen, &shm_fd, &receive_buffer))
		return;
	while(messenger->running)
	{
		synch->wait(SYNCH_SENT);
		if(!messenger->running)
			break;
		auto message = std::string(receive_buffer);
		synch->post(SYNCH_RECEIVE_READY);
		messenger->receiveQueue.push(message);
	}
	SharedMemoryManager::deinitShm(receiveBuf, messageBufferLen, shm_fd, &receive_buffer);
}
struct Msg
{
	explicit Msg(const std::string &msg) : ct_(0)
	{
		std::stringstream ss(msg);
		while(ss.good())
		{
			std::string substr;
			std::getline(ss, substr, ',');
			cs_.push_back(substr);
		}
	}
	std::string next()
	{
		if(ct_ == cs_.size())
		{
			getMessengerLogger()->error("Msg: comma separated list exhausted. returning empty.");
			return "";
		}
		return cs_[ct_++];
	}

	uint32_t nextUint(void)
	{
		return (uint32_t)std::stoi(next());
	}

	std::vector<std::string> cs_;
	size_t ct_;
};

static void processorThread(Messenger* messenger, std::function<void(std::string)> processor)
{
	while (messenger->running) {
		std::string message;
		if (!messenger->receiveQueue.waitAndPop(message)) {
			break;
		}

		if (!messenger->running) {
			break;
		}

		Msg msg(message);
		auto tag = msg.next();
		if (tag == GRK_MSGR_BATCH_COMPRESS_INIT) {
			auto width = msg.nextUint();
			msg.nextUint(); // stride
			auto height = msg.nextUint();
			auto samples_per_pixel = msg.nextUint();
			msg.nextUint(); // depth
			messenger->init_.uncompressedFrameSize_ = Messenger::uncompressedFrameSize(width, height, samples_per_pixel);
			auto compressed_frame_size = msg.nextUint();
			auto num_frames = msg.nextUint();
			messenger->initClient(compressed_frame_size, compressed_frame_size, num_frames);
		} else if (tag == GRK_MSGR_BATCH_PROCESSED_UNCOMPRESSED) {
			messenger->reclaimUncompressed(msg.nextUint());
		} else if (tag == GRK_MSGR_BATCH_PROCESSSED_COMPRESSED) {
			messenger->reclaimCompressed(msg.nextUint());
		}
		processor(message);
	}
}

template<typename F>
struct ScheduledFrames
{
	void store(F const& val)
	{
		std::unique_lock<std::mutex> lk(mapMutex_);
		auto it = map_.find(val.index());
		if (it == map_.end())
			map_.emplace(std::make_pair(val.index(), val));
	}
	boost::optional<F> retrieve(size_t index)
	{
		std::unique_lock<std::mutex> lk(mapMutex_);
		auto it = map_.find(index);
		if(it == map_.end())
			return {};

		F val = it->second;
		map_.erase(index);

		return val;
	}

 private:
	std::mutex mapMutex_;
	std::map<size_t, F> map_;
};

template<typename F>
struct ScheduledMessenger : public Messenger
{
	explicit ScheduledMessenger(MessengerInit init) : Messenger(init),
											framesScheduled_(0),
											framesCompressed_(0)
	{}
	~ScheduledMessenger(void) {
		shutdown();
	}
	bool scheduleCompress(F const& proxy, std::function<void(BufferSrc const&)> converter){
		size_t frameSize = init_.uncompressedFrameSize_;
		assert(frameSize >= init_.uncompressedFrameSize_);
		BufferSrc src;
		if(!availableBuffers_.waitAndPop(src))
			return false;
		converter(src);
		scheduledFrames_.store(proxy);
		framesScheduled_++;
		send(GRK_MSGR_BATCH_SUBMIT_UNCOMPRESSED, proxy.index(), src.frameId_);

		return true;
	}
	void processCompressed(const std::string &message, std::function<void(F,uint8_t*,uint32_t)> processor, bool needsRecompression) {
		Msg msg(message);
		msg.next();
		auto clientFrameId = msg.nextUint();
		auto compressedFrameId = msg.nextUint();
		auto compressedFrameLength = msg.nextUint();
		if (!needsRecompression) {
			auto src_frame = scheduledFrames_.retrieve(clientFrameId);
			if (!src_frame) {
				return;
			}
			processor(*src_frame, getCompressedFrame(compressedFrameId),compressedFrameLength);
		}
		++framesCompressed_;
		send(GRK_MSGR_BATCH_PROCESSSED_COMPRESSED, compressedFrameId);
		if (_shutdown && framesCompressed_ == framesScheduled_)
			shutdownCondition_.notify_all();
	}
	void shutdown(void){
		try {
			std::unique_lock<std::mutex> lk(shutdownMutex_);
			if (!async_result_.valid())
				return;
			_shutdown = true;
			if (framesScheduled_) {
				uint32_t scheduled = framesScheduled_;
				send(GRK_MSGR_BATCH_FLUSH, scheduled);
				shutdownCondition_.wait(lk, [this] { return framesScheduled_ == framesCompressed_; });
			}
			availableBuffers_.deactivate();
			send(GRK_MSGR_BATCH_SHUTDOWN);
			int result = async_result_.get();
			if(result != 0)
				getMessengerLogger()->error("Accelerator failed with return code: %d\n",result);
		} catch (std::exception &ex) {
			getMessengerLogger()->error("%s",ex.what());
		}

	}

	boost::optional<F> retrieve(size_t index) {
		return scheduledFrames_.retrieve(index);
	}

	void store(F& val) {
		scheduledFrames_.store(val);
	}

private:
	ScheduledFrames<F> scheduledFrames_;
	std::atomic<uint32_t> framesScheduled_;
	std::atomic<uint32_t> framesCompressed_;
};

} // namespace grk_plugin
