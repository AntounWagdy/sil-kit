#pragma once

#include "VAsioRegistry.hpp"

#include <functional>
#include <memory>

#if defined(_WIN32) && defined(_MSC_VER)
#    define HAS_SILKIT_REGISTRY_WINDOWS_SERVICE 1
#endif

namespace SilKitRegistry {

using StartFunction = std::function<std::unique_ptr<SilKit::Core::VAsioRegistry>()>;

#ifdef HAS_SILKIT_REGISTRY_WINDOWS_SERVICE

void RunWindowsService(StartFunction start);

inline constexpr bool HasWindowsServiceSupport()
{
    return true;
}

#else

inline void RunWindowsService(StartFunction)
{
    throw SilKit::LogicError("Running the registry as a Windows Service is not supported by this executable.");
}

inline constexpr bool HasWindowsServiceSupport()
{
    return false;
}

#endif

} // namespace SilKitRegistry