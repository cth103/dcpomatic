#include "exception_store.h"
#include "j2k_encoder_thread.h"
#include <dcp/data.h>


class DCPVideo;

namespace grk_plugin {
	class GrokContext;
}


class GrokJ2KEncoderThread : public J2KEncoderThread, public ExceptionStore
{
public:
	GrokJ2KEncoderThread(J2KEncoder& encoder, grk_plugin::GrokContext* context);

	void run() override;

private:
	grk_plugin::GrokContext* _context;
};

