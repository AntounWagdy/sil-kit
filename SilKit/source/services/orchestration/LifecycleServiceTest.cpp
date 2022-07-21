// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include <chrono>
#include <functional>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "silkit/services/orchestration/string_utils.hpp"
#include "silkit/util/functional.hpp"

#include "LifecycleService.hpp"
#include "MockParticipant.hpp"
#include "ParticipantConfiguration.hpp"
#include "SyncDatatypeUtils.hpp"
#include "TimeSyncService.hpp"

namespace {

using namespace std::chrono_literals;

using namespace testing;

using namespace SilKit;
using namespace SilKit::Core;
using namespace SilKit::Services::Orchestration;
using namespace SilKit::Util;

using ::SilKit::Core::Tests::DummyParticipant;

class MockTimeSync : public TimeSyncService
{
public:
    using TimeSyncService::TimeSyncService;

public:
    MOCK_METHOD(void, SetSimulationStepHandler, (SimulationStepT task, std::chrono::nanoseconds initialStepSize), (override));
    MOCK_METHOD(void, SetSimulationStepHandlerAsync, (SimulationStepT task, std::chrono::nanoseconds initialStepSize),
                (override));
    MOCK_METHOD(void, CompleteSimulationStep, (), (override));
    MOCK_METHOD(void, SetSimulationStepHandler,
                (std::function<void(std::chrono::nanoseconds now)> task, std::chrono::nanoseconds initialStepSize),
                (override));
    MOCK_METHOD(void, SetPeriod, (std::chrono::nanoseconds));
    MOCK_METHOD(std::chrono::nanoseconds, Now, (), (override, const));
};

class MockParticipant : public DummyParticipant
{
public:
    MockParticipant() 
    { 
    }

public:
    MOCK_METHOD(TimeSyncService*, CreateTimeSyncService, (LifecycleService*));
    MOCK_METHOD(void, SendMsg, (const IServiceEndpoint*, const ParticipantStatus& msg));

public:
};

class MockServiceDescriptor : public IServiceEndpoint
{
public:
    ServiceDescriptor serviceDescriptor;
    MockServiceDescriptor(EndpointAddress ea, std::string participantName)
    {
        ServiceDescriptor id = from_endpointAddress(ea);
        id.SetParticipantName(participantName);
        SetServiceDescriptor(id);
    }
    void SetServiceDescriptor(const ServiceDescriptor& _serviceDescriptor) override
    {
        serviceDescriptor = _serviceDescriptor;
    }
    auto GetServiceDescriptor() const -> const ServiceDescriptor& override { return serviceDescriptor; }
};

// Factory method to create a ParticipantStatus matcher that checks the state field
auto AParticipantStatusWithState(ParticipantState expected)
{
    return MatcherCast<const ParticipantStatus&>(Field(&ParticipantStatus::state, expected));
}

class LifecycleServiceTest : public testing::Test
{
protected:
    struct Callbacks
    {
        MOCK_METHOD(void, CommunicationReadyHandler, ());
        MOCK_METHOD(void, StartingHandler, ());
        MOCK_METHOD(void, StopHandler, ());
        MOCK_METHOD(void, ShutdownHandler, ());
        MOCK_METHOD(void, SimTask, (std::chrono::nanoseconds));
    };

protected:
    // ----------------------------------------
    // Members
    EndpointAddress addr{1, 1024};
    EndpointAddress addrP2{2, 1024};
    EndpointAddress masterAddr{3, 1027};
    MockServiceDescriptor p2Id{addrP2, "P2"};
    MockServiceDescriptor masterId{masterAddr, "Master"};

    MockParticipant participant;
    Callbacks callbacks;
    Config::HealthCheck healthCheckConfig;
};

// will forward lifecycle with coordination up to 
// ParticipantState::ReadyToRun & SystemState::ReadyToRun
void PrepareLifecycle(LifecycleService* lifecycleService)
{
    lifecycleService->NewSystemState(SystemState::ServicesCreated);
    lifecycleService->NewSystemState(SystemState::CommunicationInitializing);
    lifecycleService->NewSystemState(SystemState::CommunicationInitialized);
    lifecycleService->NewSystemState(SystemState::ReadyToRun);
}

auto StartCoordinated() 
{
    LifecycleConfiguration sc;
    sc.coordinatedStart = true;
    sc.coordinatedStop = true;
    return sc;
}
TEST_F(LifecycleServiceTest, start_stop_uncoordinated)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    lifecycleService.SetCommunicationReadyHandler(bind_method(&callbacks, &Callbacks::CommunicationReadyHandler));
    lifecycleService.SetStartingHandler(bind_method(&callbacks, &Callbacks::StartingHandler));
    lifecycleService.SetStopHandler(bind_method(&callbacks, &Callbacks::StopHandler));
    lifecycleService.SetShutdownHandler(bind_method(&callbacks, &Callbacks::ShutdownHandler));
    
    EXPECT_CALL(callbacks, CommunicationReadyHandler())
        .Times(1);
    EXPECT_CALL(callbacks, StartingHandler())
        .Times(1);
    EXPECT_CALL(callbacks, StopHandler())
        .Times(1);
    EXPECT_CALL(callbacks, ShutdownHandler())
        .Times(1);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService,
                              AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService,
                              AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopping)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle({});
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    lifecycleService.Stop("");
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, start_restart_stop_coordinated)
{
    // Intended state order: Create, ..., start, stop, restart, create, start, stop, shutdown
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    lifecycleService.SetCommunicationReadyHandler(bind_method(&callbacks, &Callbacks::CommunicationReadyHandler));
    lifecycleService.SetStartingHandler(bind_method(&callbacks, &Callbacks::StartingHandler));
    lifecycleService.SetStopHandler(bind_method(&callbacks, &Callbacks::StopHandler));
    lifecycleService.SetShutdownHandler(bind_method(&callbacks, &Callbacks::ShutdownHandler));
    
    EXPECT_CALL(callbacks, CommunicationReadyHandler())
        .Times(2);
    EXPECT_CALL(callbacks, StartingHandler())
        .Times(2);
    EXPECT_CALL(callbacks, StopHandler())
        .Times(2);
    EXPECT_CALL(callbacks, ShutdownHandler())
        .Times(1);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(2);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService,
                              AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(2);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService,
                              AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(2);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(2);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(3);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Paused)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopping)))
        .Times(2);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(2);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ServicesCreated);
    lifecycleService.NewSystemState(SystemState::ServicesCreated);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::CommunicationInitializing);
    lifecycleService.NewSystemState(SystemState::CommunicationInitializing);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::CommunicationInitialized);
    lifecycleService.NewSystemState(SystemState::CommunicationInitialized);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    lifecycleService.NewSystemState(SystemState::ReadyToRun);
    // run, pause & stop
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    // transitions to own state must not fail
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    lifecycleService.Pause("Test");
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Paused);
    lifecycleService.Continue();
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    SystemCommand stopCommand{SystemCommand::Kind::Stop};
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Stopped);
    // transitions to own state must not fail
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Stopped);
    // restart
    ParticipantCommand restartCommand{descriptor.GetParticipantId(), ParticipantCommand::Kind::Restart};
    lifecycleService.ReceiveMsg(&masterId, restartCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ServicesCreated);
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run & stop again
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Stopped);
    // shutdown
    ParticipantCommand shutdownCommand{descriptor.GetParticipantId(), ParticipantCommand::Kind::Shutdown};
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}


TEST_F(LifecycleServiceTest, error_on_double_pause)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Paused)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Error)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run, pause & stop
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    lifecycleService.Pause("Test");
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Paused);
    EXPECT_THROW({ lifecycleService.Pause("Pause again"); }, std::runtime_error);
}



TEST_F(LifecycleServiceTest, error_handling_run_run_shutdown)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    lifecycleService.SetStopHandler(bind_method(&callbacks, &Callbacks::StopHandler));
    lifecycleService.SetShutdownHandler(bind_method(&callbacks, &Callbacks::ShutdownHandler));

    EXPECT_CALL(callbacks, StopHandler())
        .Times(0);
    EXPECT_CALL(callbacks, ShutdownHandler())
        .Times(0);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Error)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run & stop
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    // repeat signal (must be ignored)
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    // shutdown (invalid transition -> error)
    ParticipantCommand shutdownCommand{descriptor.GetParticipantId(), ParticipantCommand::Kind::Shutdown};
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Error);
}

TEST_F(LifecycleServiceTest, error_handling_error_recovery_restart)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    lifecycleService.SetShutdownHandler(bind_method(&callbacks, &Callbacks::ShutdownHandler));

    EXPECT_CALL(callbacks, ShutdownHandler())
        .Times(1);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(2);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(2);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(2);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(2);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Error)))
        .Times(2);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run & stop
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    // repeat signal (must be ignored)
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    // shutdown (invalid transition -> error)
    ParticipantCommand shutdownCommand{descriptor.GetParticipantId(), ParticipantCommand::Kind::Shutdown};
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Error);
    // recover via restart
    ParticipantCommand restartCommand{descriptor.GetParticipantId(), ParticipantCommand::Kind::Restart};
    lifecycleService.ReceiveMsg(&masterId, restartCommand);
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // break it again
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Error);
    // recover via shutdown
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, error_handling_exception_in_callback)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    lifecycleService.SetStopHandler(bind_method(&callbacks, &Callbacks::StopHandler));
    lifecycleService.SetShutdownHandler(bind_method(&callbacks, &Callbacks::ShutdownHandler));

    EXPECT_CALL(callbacks, StopHandler())
        .Times(1)
        .WillRepeatedly(Throw(std::runtime_error("StopCallbackException")));
    EXPECT_CALL(callbacks, ShutdownHandler())
        .Times(1)
        .WillRepeatedly(Throw(std::runtime_error("ShutdownCallbackException")));

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopping)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Error)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    // stop - callback throws -> expect error state
    SystemCommand stopCommand{SystemCommand::Kind::Stop};
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Error);
    // recover via shutdown - callback throws -> expect shutdown state (error will be ignored)
    ParticipantCommand shutdownCommand{descriptor.GetParticipantId(), ParticipantCommand::Kind::Shutdown};
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, Abort_ReadyToRun)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // Abort right away
    SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
    lifecycleService.ReceiveMsg(&masterId, abortCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, Abort_Running)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Running);
    // Abort right away
    SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
    lifecycleService.ReceiveMsg(&masterId, abortCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, Abort_Paused)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));
    
    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Paused)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    lifecycleService.Pause("Test");
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Paused);
    SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
    lifecycleService.ReceiveMsg(&masterId, abortCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}


TEST_F(LifecycleServiceTest, Abort_Stopping)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    lifecycleService.SetStopHandler(bind_method(&callbacks, &Callbacks::StopHandler));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(callbacks, StopHandler())
        .Times(1)
        .WillRepeatedly(Invoke([&]() 
            {
                SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
                lifecycleService.ReceiveMsg(&masterId, abortCommand);
                EXPECT_EQ(lifecycleService.State(), ParticipantState::Stopping);
            }));
    
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopping)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run & pause
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    SystemCommand stopCommand{SystemCommand::Kind::Stop};
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, Abort_Stop)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopping)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run & pause
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    SystemCommand stopCommand{SystemCommand::Kind::Stop};
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Stopped);
    // Abort right away
    SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
    lifecycleService.ReceiveMsg(&masterId, abortCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, Abort_ShuttingDown)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    lifecycleService.SetShutdownHandler(bind_method(&callbacks, &Callbacks::ShutdownHandler));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(callbacks, ShutdownHandler())
        .Times(1)
        .WillRepeatedly(Invoke([&]() 
            {
                SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
                lifecycleService.ReceiveMsg(&masterId, abortCommand);
                EXPECT_EQ(lifecycleService.State(), ParticipantState::ShuttingDown);
            }));

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopping)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run & pause
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    SystemCommand stopCommand{SystemCommand::Kind::Stop};
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    ParticipantCommand shutdownCommand;
    shutdownCommand.participant = descriptor.GetParticipantId();
    shutdownCommand.kind=ParticipantCommand::Kind::Shutdown;
    lifecycleService.ReceiveMsg(&masterId, shutdownCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, Abort_Shutdown)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Running)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopping)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Stopped)))
        .Times(1);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(1);
    EXPECT_CALL(participant, 
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run & pause
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    SystemCommand stopCommand{SystemCommand::Kind::Stop};
    lifecycleService.ReceiveMsg(&masterId, stopCommand);
    // Abort right away
    SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
    lifecycleService.ReceiveMsg(&masterId, abortCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Shutdown);
}

TEST_F(LifecycleServiceTest, Abort_LifecycleNotExecuted)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(0);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(0);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(0);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(0);
    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ShuttingDown)))
        .Times(0);
    EXPECT_CALL(participant, SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Shutdown)))
        .Times(0);

    EXPECT_EQ(lifecycleService.State(), ParticipantState::Invalid);
    // Abort right away
    SystemCommand abortCommand{SystemCommand::Kind::AbortSimulation};
    lifecycleService.ReceiveMsg(&masterId, abortCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Invalid);
}

TEST_F(LifecycleServiceTest, error_handling_exception_in_starting_callback)
{
    LifecycleService lifecycleService(&participant, healthCheckConfig);
    MockTimeSync mockTimeSync(&participant, &participant.mockTimeProvider, healthCheckConfig);
    lifecycleService.SetTimeSyncService(&mockTimeSync);
    ON_CALL(participant, CreateTimeSyncService(_)).WillByDefault(Return(&mockTimeSync));

    lifecycleService.SetStartingHandler(bind_method(&callbacks, &Callbacks::StartingHandler));
    EXPECT_CALL(callbacks, StartingHandler())
        .Times(1)
        .WillRepeatedly(Throw(std::runtime_error("StartingException")));

    auto descriptor = from_endpointAddress(addr);
    lifecycleService.SetServiceDescriptor(descriptor);

    EXPECT_CALL(participant,
                SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::ServicesCreated)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitializing)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::CommunicationInitialized)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService,
                                           AParticipantStatusWithState(ParticipantState::ReadyToRun)))
        .Times(1);
    EXPECT_CALL(participant, SendMsg(&lifecycleService, AParticipantStatusWithState(ParticipantState::Error)))
        .Times(1);

    lifecycleService.StartLifecycle(
        StartCoordinated());
    PrepareLifecycle(&lifecycleService);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::ReadyToRun);
    // run
    SystemCommand runCommand{SystemCommand::Kind::Run};
    lifecycleService.ReceiveMsg(&masterId, runCommand);
    EXPECT_EQ(lifecycleService.State(), ParticipantState::Error);
}

} // namespace
