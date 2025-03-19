#include "encode_server_description.h"
#include "j2k_sync_encoder_thread.h"


class RemoteJ2KEncoderThread : public J2KSyncEncoderThread
{
public:
	RemoteJ2KEncoderThread(J2KEncoder& encoder, EncodeServerDescription server);

	void log_thread_start() const override;
	std::shared_ptr<dcp::ArrayData> encode(DCPVideo const& frame) override;

	EncodeServerDescription server() const {
		return _server;
	}

	int backoff() const override {
		return _remote_backoff;
	}

private:
	EncodeServerDescription _server;
	/** Number of seconds that we currently wait between attempts to connect to the server */
	int _remote_backoff = 0;
};
