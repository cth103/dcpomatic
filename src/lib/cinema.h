#include <libdcp/certificates.h>

class Screen
{
public:
	Screen (std::string const & n)
		: name (n)
	{}
	
	std::string name;
	libdcp::Certificate certificate;
};

class Cinema
{
public:
	Cinema (std::string const & n)
		: name (n)
	{}
	
	std::string name;
	std::list<Screen> screens;
};
