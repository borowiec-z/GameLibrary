#include "GameLibrary/Console/Cvar.h"

#include <string>

#include "GameLibrary/Utilities/Conversions/String.h"

using namespace GameLibrary::Console;
using namespace GameLibrary::Utilities;


Cvar::String Cvar::getAsString() const {
	return _value.getAsString();
}
