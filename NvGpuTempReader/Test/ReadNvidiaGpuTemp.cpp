#include "NvGpuTempReader.h"

int main()
{
    if (const auto tempOpt = bbmp::getNvGpuTemp())
        return tempOpt.value();

    return 0;
}
