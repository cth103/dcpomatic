#include <string>

class Ratio
{
public:
	Ratio ();
	Ratio (float);
	
	float ratio () const {
		return _ratio;
	}

	std::string name () const;

private:
	float _ratio;
};
