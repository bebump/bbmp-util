#pragma once

#include <optional>

namespace bbmp
{

/* Returns the temperature of the first NVidia GPU that it can find.

   Required state is set up in static variables the first time you call this function. Subsequent
   calls should be cheap.
*/
std::optional<int> getNvGpuTemp();

}
