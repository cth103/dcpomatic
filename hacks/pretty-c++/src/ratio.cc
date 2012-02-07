#include <sstream>
#include "ratio.h"

using namespace std;

Ratio::Ratio ()
	: _ratio (1.85)
{

}

Ratio::Ratio (float r)
	: _ratio (r)
{

}

string
Ratio::name () const
{
	switch (_ratio) {
	case 1.85:
		return "Flat (1.85:1)";
		break;
	case 2.39:
		return "Scope (2.39:1)";
		break;
	}

	stringstream s;
	s << _ratio;
	return s.str ();
}

	
