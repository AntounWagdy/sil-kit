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

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "silkit/participant/exception.hpp"
#include "silkit/SilKitMacros.hpp"
#include "silkit/services/logging/LoggingDatatypes.hpp"

#include "Optional.hpp"

namespace SilKit {
namespace Config {

inline namespace v1 {

// ================================================================================
//  Shared configuration declarations
// ================================================================================

enum class NetworkType
{
    Undefined,
    Invalid,
    CAN,
    LIN,
    Ethernet,
    FlexRay,
    Data,
    RPC
};
inline auto to_string(NetworkType networkType) -> std::string;

// ================================================================================
//  Logging service
// ================================================================================

struct Sink
{
    enum class Type : uint8_t
    {
        Remote,
        Stdout,
        File
    };

    Type type{ Type::Remote };
    Services::Logging::Level level{ Services::Logging::Level::Info };
    std::string logName;
};

//! \brief Logger service
struct Logging
{
    bool logFromRemotes{ false };
    Services::Logging::Level flushLevel{ Services::Logging::Level::Off };
    std::vector<Sink> sinks;
};

// ================================================================================
//  Tracing service
// ================================================================================

struct TraceSink
{
    enum class Type
    {
        Undefined,
        PcapFile,
        PcapPipe,
        Mdf4File
    };

    Type type{ Type::Undefined };
    std::string name;
    std::string outputPath;
};

struct TraceSource
{
    enum class Type
    {
        Undefined,
        PcapFile,
        Mdf4File
    };

    Type type{ Type::Undefined };
    std::string name;
    std::string inputPath;
};

//! MdfChannel identification for replaying, refer to ASAM MDF 4.1 Specification, Chapter 5.4.3
struct MdfChannel
{
    // A user supplied empty string in the configuration is valid
    SilKit::Util::Optional<std::string> channelName; //!< maps to MDF cn_tx_name
    SilKit::Util::Optional<std::string> channelSource; //!< maps to MDF si_tx_name of cn_si_source
    SilKit::Util::Optional<std::string> channelPath; //!< maps to MDF si_tx_path of cn_si_source

    SilKit::Util::Optional<std::string> groupName; //!< maps to MDF cg_tx_name
    SilKit::Util::Optional<std::string> groupSource; //!< maps to MDF si_tx_name of cg_si_acq_source
    SilKit::Util::Optional<std::string> groupPath; //!< maps to MDF si_tx_path of cn_si_acq_source
};

struct Replay
{
    std::string useTraceSource;

    enum class Direction
    {
        Undefined,
        Send,
        Receive,
        Both,
    };
    Direction direction{ Direction::Undefined };
    std::vector<std::string> filterMessage;
    MdfChannel mdfChannel;
};

bool operator==(const Sink& lhs, const Sink& rhs);
bool operator==(const Logging& lhs, const Logging& rhs);
bool operator==(const TraceSink& lhs, const TraceSink& rhs);
bool operator==(const TraceSource& lhs, const TraceSource& rhs);
bool operator==(const Replay& lhs, const Replay& rhs);
bool operator==(const MdfChannel& lhs, const MdfChannel& rhs);

inline auto to_string(TraceSink::Type sinkType) -> std::string;

// ================================================================================
//  Inline Implementations
// ================================================================================

auto to_string(TraceSink::Type sinkType) -> std::string
{
    switch (sinkType)
    {
    case TraceSink::Type::Mdf4File:
        return "Mdf4File";
    case TraceSink::Type::PcapFile:
        return "PcapFile";
    case TraceSink::Type::PcapPipe:
        return "PcapPipe";
    case TraceSink::Type::Undefined:
        return "Undefined";
    default:
        throw SilKit::TypeConversionError{ "Invalid SinkType" };
    }
}

auto to_string(NetworkType networkType) -> std::string
{
    switch (networkType)
    {
    case NetworkType::Undefined: return "Undefined";
    case NetworkType::Invalid: return "Invalid";
    case NetworkType::CAN: return "CAN";
    case NetworkType::LIN: return "LIN";
    case NetworkType::Ethernet: return "Ethernet";
    case NetworkType::FlexRay: return "FlexRay";
    case NetworkType::Data: return "Data";
    case NetworkType::RPC: return "RPC";
    default: return "Unknown";
    }
}

} // inline namespace v1

} // namespace Config
} // namespace SilKit
