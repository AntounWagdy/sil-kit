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

#include <functional>
#include <vector>

#include "RequestReplyDatatypes.hpp"

namespace SilKit {
namespace Core {
namespace RequestReply {

class IRequestReplyService;

// Methods of prodecure classes (e.g. ParticipantReplies) used in RequestReplyService
class IRequestReplyProcedure
{
public:

    virtual ~IRequestReplyProcedure() = default;

    virtual void ReceiveCall(IRequestReplyService* requestReplyService, Util::Uuid callUuid, std::vector<uint8_t> callData) = 0;
    virtual void ReceiveCallReturn(Util::Uuid callUuid, std::vector<uint8_t> callReturnData, CallReturnStatus callReturnStatus) = 0;
    virtual void SetRequestReplyServiceEndpoint(IServiceEndpoint* requestReplyServiceEndpoint) = 0;
};

} // namespace RequestReply
} // namespace Core
} // namespace SilKit
