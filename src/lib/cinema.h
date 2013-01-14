#include <libdcp/certificates.h>

class Screen
{
public:
	Screen (std::string const & n, boost::shared_ptr<libdcp::Certificate> cert)
		: name (n)
		, certificate (cert)
	{}
	
	std::string name;
	boost::shared_ptr<libdcp::Certificate> certificate;
};

class Cinema
{
public:
	Cinema (std::string const & n, std::string const & e)
		: name (n)
		, email (e)
	{}
	
	std::string name;
	std::string email;
	std::list<boost::shared_ptr<Screen> > screens;
};
