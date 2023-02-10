// Copyright (c) 2023 Vector Informatik GmbH
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "silkit/config/IParticipantConfiguration.hpp"

namespace SilKit {
namespace Hourglass {
namespace Config {

inline auto ParticipantConfigurationFromString(const std::string& text)
    -> std::shared_ptr<SilKit::Config::IParticipantConfiguration>;

inline auto ParticipantConfigurationFromFile(const std::string& filename)
    -> std::shared_ptr<SilKit::Config::IParticipantConfiguration>;

} // namespace Config
} // namespace Hourglass
} // namespace SilKit

// ================================================================================
//  Inline Implementations
// ================================================================================

#include "silkit/participant/exception.hpp"

#include "silkit/hourglass/impl/CheckReturnCode.hpp"
#include "silkit/hourglass/impl/config/ParticipantConfiguration.hpp"

#include <sstream>
#include <fstream>

namespace SilKit {
namespace Hourglass {
namespace Config {

auto ParticipantConfigurationFromString(const std::string& text)
    -> std::shared_ptr<SilKit::Config::IParticipantConfiguration>
{
    SilKit_ParticipantConfiguration* participantConfiguration{nullptr};

    const auto returnCode = SilKit_ParticipantConfiguration_FromString(&participantConfiguration, text.c_str());
    Impl::ThrowOnError(returnCode);

    return std::make_shared<Impl::Config::ParticipantConfiguration>(participantConfiguration);
}

auto ParticipantConfigurationFromFile(const std::string& filename)
    -> std::shared_ptr<SilKit::Config::IParticipantConfiguration>
{
    std::ifstream fs(filename);

    if (!fs.is_open())
        throw SilKit::ConfigurationError("the file could not be opened");

    std::stringstream buffer;
    buffer << fs.rdbuf();

    return ParticipantConfigurationFromString(buffer.str());
}

} // namespace Config
} // namespace Hourglass
} // namespace SilKit
