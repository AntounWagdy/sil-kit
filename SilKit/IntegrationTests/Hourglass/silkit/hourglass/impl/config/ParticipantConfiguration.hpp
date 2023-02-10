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

#include "silkit/capi/SilKit.h"

namespace SilKit {
namespace Hourglass {
namespace Impl {
namespace Config {

class ParticipantConfiguration : public SilKit::Config::IParticipantConfiguration
{
public:
    explicit ParticipantConfiguration(SilKit_ParticipantConfiguration* participantConfiguration);

    ~ParticipantConfiguration() override;

    auto Get() const -> SilKit_ParticipantConfiguration*;

private:
    SilKit_ParticipantConfiguration* _participantConfiguration{nullptr};
};

} // namespace Config
} // namespace Impl
} // namespace Hourglass
} // namespace SilKit

// ================================================================================
//  Inline Implementations
// ================================================================================

namespace SilKit {
namespace Hourglass {
namespace Impl {
namespace Config {

ParticipantConfiguration::ParticipantConfiguration(SilKit_ParticipantConfiguration* participantConfiguration)
    : _participantConfiguration{participantConfiguration}
{
}

ParticipantConfiguration::~ParticipantConfiguration()
{
    if (_participantConfiguration != nullptr)
    {
        SilKit_ParticipantConfiguration_Destroy(_participantConfiguration);
    }
}

auto ParticipantConfiguration::Get() const -> SilKit_ParticipantConfiguration*
{
    return _participantConfiguration;
}

} // namespace Config
} // namespace Impl
} // namespace Hourglass
} // namespace SilKit
