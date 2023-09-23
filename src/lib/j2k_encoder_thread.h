#ifndef DCPOMATIC_J2K_ENCODER_THREAD
#define DCPOMATIC_J2K_ENCODER_THREAD


#include <boost/thread.hpp>


class J2KEncoder;


class J2KEncoderThread
{
public:
	J2KEncoderThread(J2KEncoder& encoder);

	J2KEncoderThread(J2KEncoderThread const&) = delete;
	J2KEncoderThread& operator=(J2KEncoderThread const&) = delete;

	void start();
	void stop();

	virtual void run() = 0;

protected:
	J2KEncoder& _encoder;

private:
	boost::thread _thread;
};


#endif
