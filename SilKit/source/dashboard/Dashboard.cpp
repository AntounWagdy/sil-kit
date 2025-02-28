/* Copyright (c) 2022 Vector Informatik GmbH
 
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "Dashboard.hpp"

#include <chrono>

#include "oatpp/core/macro/component.hpp"
#include "oatpp/web/client/HttpRequestExecutor.hpp"
#include "oatpp/network/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

#include "silkit/SilKit.hpp"
#include "silkit/config/IParticipantConfiguration.hpp"
#include "ILogger.hpp"
#include "IParticipantInternal.hpp"
#include "IServiceDiscovery.hpp"
#include "SetThreadName.hpp"

#include "ParticipantConfiguration.hpp"
#include "CreateParticipantImpl.hpp"

#include "CachingSilKitEventHandler.hpp"
#include "SilKitEventQueue.hpp"
#include "DashboardRetryPolicy.hpp"
#include "DashboardSystemServiceClient.hpp"
#include "SilKitEventHandler.hpp"
#include "SilKitToOatppMapper.hpp"

using namespace std::chrono_literals;

namespace SilKit {
namespace Dashboard {

Dashboard::Dashboard(std::shared_ptr<SilKit::Config::IParticipantConfiguration> participantConfig,
                     const std::string& registryUri)
{
    _dashboardParticipant = SilKit::CreateParticipantImpl(participantConfig, "__SilKitDashboard", registryUri);
    _participantInternal = dynamic_cast<Core::IParticipantInternal*>(_dashboardParticipant.get());
    _systemMonitor = _participantInternal->CreateSystemMonitor();
    _serviceDiscovery = _participantInternal->GetServiceDiscovery();
    _logger = _participantInternal->GetLogger();
    _retryPolicy = std::make_shared<DashboardRetryPolicy>(3);
    OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper);
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ClientConnectionProvider>, connectionProvider);
    auto requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(connectionProvider, _retryPolicy);
    auto apiClient = DashboardSystemApiClient::createShared(requestExecutor, objectMapper);
    auto silKitToOatppMapper = std::make_shared<SilKitToOatppMapper>();
    auto serviceClient =
        std::make_shared<DashboardSystemServiceClient>(_logger, apiClient, objectMapper);
    auto eventHandler = std::make_shared<SilKitEventHandler>(_logger, serviceClient, silKitToOatppMapper);
    auto eventQueue = std::make_shared<SilKitEventQueue>();
    _cachingEventHandler = std::make_unique<CachingSilKitEventHandler>(registryUri, _logger, eventHandler, eventQueue);

    _systemMonitor->SetParticipantConnectedHandler([this](auto&& participantInformation) {
        OnParticipantConnected(participantInformation);
    });
    _systemMonitor->SetParticipantDisconnectedHandler([this](auto&& participantInformation) {
        OnParticipantDisconnected(participantInformation);
    });
    _participantStatusHandlerId = _systemMonitor->AddParticipantStatusHandler([this](auto&& participantStatus) {
        OnParticipantStatusChanged(participantStatus);
    });
    _systemStateHandlerId = _systemMonitor->AddSystemStateHandler([this](auto&& systemState) {
        OnSystemStateChanged(systemState);
    });
    _serviceDiscovery->RegisterServiceDiscoveryHandler([this](auto&& discoveryType, auto&& serviceDescriptor) {
        OnServiceDiscoveryEvent(discoveryType, serviceDescriptor);
    });
}

Dashboard::~Dashboard()
{
    _systemMonitor->RemoveParticipantStatusHandler(_participantStatusHandlerId);
    _systemMonitor->RemoveSystemStateHandler(_systemStateHandlerId);
    _retryPolicy->AbortAllRetries();
    std::unique_lock<decltype(_cachingEventHandlerMx)> lock(_cachingEventHandlerMx);
    _cachingEventHandler.reset();
}

void Dashboard::OnParticipantConnected(
    const Services::Orchestration::ParticipantConnectionInformation& participantInformation)
{
    if (participantInformation.participantName == _participantInternal->GetParticipantName())
    {
        return;
    }
    {
        std::unique_lock<decltype(_connectedParticipantsMx)> lock(_connectedParticipantsMx);
        _connectedParticipants.push_back(participantInformation.participantName);
    }
    AccessCachingEventHandler(
        [this](auto cachingEventHandler, auto&& participantInformation) {
            cachingEventHandler->OnParticipantConnected(participantInformation);
        },
        participantInformation);
}

void Dashboard::OnParticipantDisconnected(
    const Services::Orchestration::ParticipantConnectionInformation& participantInformation)
{
    if (participantInformation.participantName == _participantInternal->GetParticipantName())
    {
        return;
    }
    if (LastParticipantDisconnected(participantInformation))
    {
        AccessCachingEventHandler(
            [this](auto cachingEventHandler, auto&& participantInformation) {
                cachingEventHandler->OnLastParticipantDisconnected();
            },
            participantInformation);
    }
}

void Dashboard::OnParticipantStatusChanged(const Services::Orchestration::ParticipantStatus& participantStatus)
{
    if (participantStatus.participantName == _participantInternal->GetParticipantName())
    {
        return;
    }
    AccessCachingEventHandler(
        [this](auto cachingEventHandler, auto&& participantStatus) {
            cachingEventHandler->OnParticipantStatusChanged(participantStatus);
        },
        participantStatus);
}

void Dashboard::OnSystemStateChanged(Services::Orchestration::SystemState systemState)
{
    AccessCachingEventHandler(
        [this](auto cachingEventHandler, auto&& systemState) {
            cachingEventHandler->OnSystemStateChanged(systemState);
        },
        systemState);
}

void Dashboard::OnServiceDiscoveryEvent(Core::Discovery::ServiceDiscoveryEvent::Type discoveryType,
                                        const Core::ServiceDescriptor& serviceDescriptor)
{
    if (serviceDescriptor.GetParticipantName() == _participantInternal->GetParticipantName())
    {
        return;
    }
    AccessCachingEventHandler(
        [this](auto cachingEventHandler, auto&& discoveryType, auto&& serviceDescriptor) {
            cachingEventHandler->OnServiceDiscoveryEvent(discoveryType, serviceDescriptor);
        },
        discoveryType, serviceDescriptor);
}

bool Dashboard::LastParticipantDisconnected(
    const Services::Orchestration::ParticipantConnectionInformation& participantInformation)
{
    std::unique_lock<decltype(_connectedParticipantsMx)> lock(_connectedParticipantsMx);
    _connectedParticipants.erase(std::remove_if(_connectedParticipants.begin(), _connectedParticipants.end(),
                                                [&participantInformation](const auto& participantName) {
                                                    return participantName == participantInformation.participantName;
                                                }),
                                 _connectedParticipants.end());
    Services::Logging::Debug(_logger, "Dashboard: {} connected participant(s)", _connectedParticipants.size());
    return _connectedParticipants.empty();
}

} // namespace Dashboard
} // namespace SilKit