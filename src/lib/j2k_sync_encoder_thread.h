#ifndef DCPOMATIC_J2K_SYNC_ENCODER_THREAD_H
#define DCPOMATIC_J2K_SYNC_ENCODER_THREAD_H


#include "exception_store.h"
#include "j2k_encoder_thread.h"
#include <dcp/array_data.h>
#include <boost/thread.hpp>


class DCPVideo;
class J2KEncoder;


class J2KSyncEncoderThread : public J2KEncoderThread, public ExceptionStore
{
public:
	J2KSyncEncoderThread(J2KEncoder& encoder);

	J2KSyncEncoderThread(J2KSyncEncoderThread const&) = delete;
	J2KSyncEncoderThread& operator=(J2KSyncEncoderThread const&) = delete;

	virtual ~J2KSyncEncoderThread() {}

	void run() override;

	virtual void log_thread_start() const = 0;
	virtual std::shared_ptr<dcp::ArrayData> encode(DCPVideo const& frame) = 0;
	/** @return number of seconds we should wait between attempts to use this thread
	 *  for encoding. Used to avoid flooding non-responsive network servers with
	 *  requests.
	 */
	virtual int backoff() const { return 0; }
};


#endif
