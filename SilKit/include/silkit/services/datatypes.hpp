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

#include <cstdint>
#include <string>

namespace SilKit {
namespace Services {

/*! \brief Flag indicating the direction of a message
 */
enum class TransmitDirection : uint8_t
{
    // Undefined
    Undefined = 0,
    // Transmit
    TX = 1,
    // Receive
    RX = 2,
    // Send/Receive
    TXRX = 3,
};
using DirectionMask = uint8_t;

/*! \brief Struct that contains a label as used in PubSub and RPC for matching publisher, subscribers, servers, and clients
*/
struct Label
{
    std::string key;
    std::string value;
};

/*! \brief Struct that contains a label as used in PubSub and RPC for matching publisher, subscribers, servers, and clients
*/
struct MatchingLabel
{
    /*! \brief Enum defining the matching kind to apply for this label
    */
    enum class Kind : uint32_t
    {
        Preferred = 1, //!< If this label is available, its value must match
        Mandatory = 2 //!< This label must be available and its value must match
    };

    std::string key;
    std::string value;
    Kind kind;
};

} // namespace Services
} // namespace SilKit
