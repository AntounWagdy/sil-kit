// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include "SystemController.hpp"

namespace ib {
namespace mw {
namespace sync {

SystemController::SystemController(IComAdapterInternal* comAdapter)
    : _comAdapter{comAdapter}
{
}

void SystemController::Initialize(ParticipantId participantId) const
{
    SendParticipantCommand(participantId, ParticipantCommand::Kind::Initialize);
}

void SystemController::ReInitialize(ParticipantId participantId) const
{
    SendParticipantCommand(participantId, ParticipantCommand::Kind::ReInitialize);
}

void SystemController::Initialize(std::string participantName) const
{
    SendParticipantCommand(hash(participantName), ParticipantCommand::Kind::Initialize);
}

void SystemController::ReInitialize(std::string participantName) const
{
    SendParticipantCommand(hash(participantName), ParticipantCommand::Kind::ReInitialize);
}

void SystemController::Run() const
{
    SendSystemCommand(SystemCommand::Kind::Run);
}

void SystemController::Stop() const
{
    SendSystemCommand(SystemCommand::Kind::Stop);
}

void SystemController::Shutdown() const
{
    SendSystemCommand(SystemCommand::Kind::Shutdown);
}

void SystemController::PrepareColdswap() const
{
    SendSystemCommand(SystemCommand::Kind::PrepareColdswap);
}

void SystemController::ExecuteColdswap() const
{
    _comAdapter->FlushSendBuffers();
    SendSystemCommand(SystemCommand::Kind::ExecuteColdswap);
}

void SystemController::SetEndpointAddress(const mw::EndpointAddress& addr)
{
    _serviceDescriptor.legacyEpa = addr;
}

auto SystemController::EndpointAddress() const -> const mw::EndpointAddress&
{
    return _serviceDescriptor.legacyEpa;
}

} // namespace sync
} // namespace mw
} // namespace ib
