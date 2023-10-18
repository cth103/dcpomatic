#include "j2k_sync_encoder_thread.h"
#include <dcp/data.h>


class DCPVideo;


class CPUJ2KEncoderThread : public J2KSyncEncoderThread
{
public:
	CPUJ2KEncoderThread(J2KEncoder& encoder);

	void log_thread_start() const override;
	std::shared_ptr<dcp::ArrayData> encode(DCPVideo const& frame) override;
};

