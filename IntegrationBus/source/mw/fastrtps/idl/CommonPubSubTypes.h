// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*!
 * @file CommonPubSubTypes.h
 * This header file contains the declaration of the serialization functions.
 *
 * This file was generated by the tool fastcdrgen.
 */


#ifndef _IB_MW_IDL_COMMON_PUBSUBTYPES_H_
#define _IB_MW_IDL_COMMON_PUBSUBTYPES_H_

#include <fastrtps/config.h>
#include <fastrtps/TopicDataType.h>

#include "Common.h"

#if !defined(GEN_API_VER) || (GEN_API_VER != 1)
#error Generated Common is not compatible with current installed Fast-RTPS. Please, regenerate it with fastrtpsgen.
#endif

namespace ib
{
    namespace mw
    {
        namespace idl
        {
            typedef uint16_t ParticipantIdT;
            typedef uint16_t EndpointIdT;
            /*!
             * @brief This class represents the TopicDataType of the type EndpointAddress defined by the user in the IDL file.
             * @ingroup COMMON
             */
            class EndpointAddressPubSubType : public eprosima::fastrtps::TopicDataType {
            public:
                typedef EndpointAddress type;

                eProsima_user_DllExport EndpointAddressPubSubType();

                eProsima_user_DllExport virtual ~EndpointAddressPubSubType();
                eProsima_user_DllExport virtual bool serialize(void *data, eprosima::fastrtps::rtps::SerializedPayload_t *payload) override;
                eProsima_user_DllExport virtual bool deserialize(eprosima::fastrtps::rtps::SerializedPayload_t *payload, void *data) override;
                eProsima_user_DllExport virtual std::function<uint32_t()> getSerializedSizeProvider(void* data) override;
                eProsima_user_DllExport virtual bool getKey(void *data, eprosima::fastrtps::rtps::InstanceHandle_t *ihandle,
                    bool force_md5 = false) override;
                eProsima_user_DllExport virtual void* createData() override;
                eProsima_user_DllExport virtual void deleteData(void * data) override;
                MD5 m_md5;
                unsigned char* m_keyBuffer;
            };
        }
    }
}

#endif // _IB_MW_IDL_COMMON_PUBSUBTYPES_H_