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

#pragma once

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

namespace SilKit {
namespace Dashboard {

ENUM(ParticipantState, v_int32, //
     VALUE(Unknown, -1, "unknown"), //
     VALUE(Invalid, 0, "invalid"), //
     VALUE(ServicesCreated, 10, "servicescreated"), //
     VALUE(CommunicationInitializing, 20, "communicationinitializing"), //
     VALUE(CommunicationInitialized, 30, "communicationinitialized"), //
     VALUE(ReadyToRun, 40, "readytorun"), //
     VALUE(Running, 50, "running"), //
     VALUE(Paused, 60, "paused"), //
     VALUE(Stopping, 70, "stopping"), //
     VALUE(Stopped, 80, "stopped"), //
     VALUE(Error, 90, "error"), //
     VALUE(ShuttingDown, 100, "shuttingdown"), //
     VALUE(Shutdown, 110, "shutdown"), //
     VALUE(Aborting, 120, "aborting"))

class ParticipantStatusDto : public oatpp::DTO
{
    DTO_INIT(ParticipantStatusDto, DTO)

    DTO_FIELD_INFO(state) { info->description = "Name of the state"; }
    DTO_FIELD(Enum<ParticipantState>::AsString, state);

    DTO_FIELD_INFO(enterReason) { info->description = "Reason for entering the state"; }
    DTO_FIELD(String, enterReason);

    DTO_FIELD_INFO(enterTime) { info->description = "Time when state got entered"; }
    DTO_FIELD(UInt64, enterTime);
};

} // namespace Dashboard
} // namespace SilKit

#include OATPP_CODEGEN_END(DTO)