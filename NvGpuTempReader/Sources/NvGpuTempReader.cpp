#include "NvGpuTempReader.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN

#include "nvapi/nvapi.h"

#include <optional>
#include <span>
#include <string>

#ifndef jassertfalse
#include <cassert>
#define jassertfalse assert (false)
#endif

namespace bbmp
{

class NvAPIInitialiser
{
public:
    static auto& get()
    {
        static NvAPIInitialiser instance;
        return instance;
    }

    ~NvAPIInitialiser()
    {
        NvAPI_Unload();
    }

private:
    NvAPIInitialiser()
    {
        if (NvAPI_Initialize() != NVAPI_OK)
            jassertfalse;
    }
};

class PhysicalGpuHandles
{
public:
    static auto& get()
    {
        static PhysicalGpuHandles instance;
        return instance;
    }

    std::span<NvPhysicalGpuHandle> getHandles()
    {
        return { handles, numHandles };
    }

private:
    PhysicalGpuHandles()
    {
        NvAPIInitialiser::get();

        if (NvAPI_EnumPhysicalGPUs (handles, &numHandles) != NVAPI_OK)
            jassertfalse;
    }

    NvPhysicalGpuHandle handles[NVAPI_MAX_PHYSICAL_GPUS];
    NvU32 numHandles = 0;
};

auto makeNvGpuThermalSettings()
{
    NV_GPU_THERMAL_SETTINGS settings;
    settings.version = NV_GPU_THERMAL_SETTINGS_VER;
    return settings;
}

std::string NvThermalTargetToString (NV_THERMAL_TARGET target)
{
    switch (target)
    {
        case NVAPI_THERMAL_TARGET_NONE:
            return "None";

        case NVAPI_THERMAL_TARGET_GPU:
            return "GPU";

        case NVAPI_THERMAL_TARGET_MEMORY:
            return "Memory";

        case NVAPI_THERMAL_TARGET_POWER_SUPPLY:
            return "Power supply";

        case NVAPI_THERMAL_TARGET_BOARD:
            return "Board";

        case NVAPI_THERMAL_TARGET_VCD_BOARD:
            return "VCD board";

        case NVAPI_THERMAL_TARGET_VCD_INLET:
            return "VCD inlet";

        case NVAPI_THERMAL_TARGET_VCD_OUTLET:
            return "VCD outlet";

        case NVAPI_THERMAL_TARGET_ALL:
            return "All";

        case NVAPI_THERMAL_TARGET_UNKNOWN:
            return "Unknown";
    }

    jassertfalse;
    return "Unknown";
}

std::optional<int> getNvGpuTemp()
{
    const auto handles = PhysicalGpuHandles::get().getHandles();

    for (const auto& h : handles)
    {
        auto settings = makeNvGpuThermalSettings();

        if (NvAPI_GPU_GetThermalSettings (h, NVAPI_THERMAL_TARGET_ALL, &settings) != NVAPI_OK)
            jassertfalse;

        for (NvU32 i = 0; i < settings.count; ++i)
        {
            auto& sensor = settings.sensor[i];

            if (sensor.target == NVAPI_THERMAL_TARGET_GPU)
                return (int) sensor.currentTemp;
        }
    }

    return std::nullopt;
}

} // namespace bbmp
