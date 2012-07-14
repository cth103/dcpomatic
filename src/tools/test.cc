#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include "image.h"
#include "server.h"

using namespace boost;

int main ()
{
	uint8_t* rgb = new uint8_t[256];
	shared_ptr<Image> image (new Image (rgb, 0, 32, 32, 24));
	Server* s = new Server ("localhost", 2);
	image->encode_remotely (s);
	return 0;
}
