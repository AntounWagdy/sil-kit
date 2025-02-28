# Copyright (c) 2023 Vector Informatik GmbH
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


add_library(
    S_Test_Hourglass
    STATIC
    MockCapi.hpp
    MockCapi.cpp
)

target_include_directories(
    S_Test_Hourglass
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    S_Test_Hourglass
    PUBLIC SilKitInterface
    PRIVATE gtest
    PRIVATE gmock
)

if (MSVC)
    target_compile_options(S_Test_Hourglass PRIVATE /bigobj)
endif ()


add_silkit_test(Test_HourglassMappingParticipantLogger
    SOURCES 
        Test_HourglassParticipantLogger.cpp
    LIBS 
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingCan
    SOURCES
        Test_HourglassCan.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingEthernet
    SOURCES
        Test_HourglassEthernet.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingLin
    SOURCES
        Test_HourglassLin.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingFlexRay
    SOURCES
        Test_HourglassFlexray.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingRpc
    SOURCES
        Test_HourglassRpc.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingPubSub
    SOURCES
        Test_HourglassPubSub.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingOrchestration
    SOURCES
        Test_HourglassOrchestration.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingVendor
    SOURCES
        Test_HourglassVendor.cpp
    LIBS
        S_Test_Hourglass
)

add_silkit_test(Test_HourglassMappingVersion
    SOURCES
        Test_HourglassVersion.cpp
    LIBS
        S_Test_Hourglass
)
