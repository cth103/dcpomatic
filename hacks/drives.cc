#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>

using std::cout;
using std::cerr;
using std::string;

int main ()
{
	if (!boost::filesystem::exists("/sys/block")) {
		cerr << "Could not find /sys/block\n";
		exit (EXIT_FAILURE);
	}

	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator("/sys/block"); i != boost::filesystem::directory_iterator(); ++i) {
		if (boost::starts_with(i->path().filename().string(), "loop")) {
			continue;
		}

		string model;
		boost::filesystem::path device = i->path() / "device" / "model";
		FILE* f = fopen(device.string().c_str(), "r");
		if (f) {
			char buffer[128];
			fgets (buffer, 128, f);
			model = buffer;
			boost::trim (model);
			fclose (f);
		}

		cout << i->path() << " " << model << "\n";
	}

	return 0;
}

