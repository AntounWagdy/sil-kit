// Copyright (c) Vector Informatik GmbH. All rights reserved.

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <typeinfo>
#include <future>

#include "ParticipantConfiguration.hpp"
#include "silkit/services/logging/ILogger.hpp"

#include "tuple_tools/for_each.hpp"
#include "tuple_tools/wrapped_tuple.hpp"

#include "SilKitLink.hpp"
#include "IVAsioPeer.hpp"
#include "VAsioReceiver.hpp"
#include "VAsioTransmitter.hpp"
#include "VAsioMsgKind.hpp"
#include "IServiceEndpoint.hpp"
#include "traits/SilKitMsgTraits.hpp"
#include "traits/SilKitServiceTraits.hpp"

// private data types for unit testing support:
#include "TestDataTypes.hpp"

#include "silkit/services/orchestration/string_utils.hpp"
#include "silkit/services/can/string_utils.hpp"

#include "asio.hpp"

#include "ProtocolVersion.hpp"
#include "SerializedMessage.hpp"

namespace SilKit {
namespace Core {

class VAsioConnection
{
public:
    // ----------------------------------------
    // Public Data Types

public:
    // ----------------------------------------
    // Constructors and Destructor
    VAsioConnection(const VAsioConnection&) = delete; //clang warning: this is implicity deleted by asio::io_context
    VAsioConnection(VAsioConnection&&) = delete; // ditto asio::io_context
    VAsioConnection(SilKit::Config::ParticipantConfiguration config, std::string participantName,
                    ParticipantId participantId, Services::Orchestration::ITimeProvider* timeProvider,
                    ProtocolVersion version = CurrentProtocolVersion());
    ~VAsioConnection();

public:
    // ----------------------------------------
    // Operator Implementations
    VAsioConnection& operator=(const VAsioConnection& other) = delete; // also implicitly deleted by asio::io_context
    VAsioConnection& operator=(VAsioConnection&& other) = delete;

public:
    // ----------------------------------------
    // Public methods
    void SetLogger(Services::Logging::ILogger* logger);
    void JoinDomain(std::string registryUri);

    template <class SilKitServiceT>
    void RegisterSilKitService(const std::string& link, EndpointId endpointId, SilKitServiceT* service)
    {
        std::future<void> allAcked;
        if (!SilKitServiceTraits<SilKitServiceT>::UseAsyncRegistration())
        {
            assert(_pendingSubscriptionAcknowledges.empty());
            _receivedAllSubscriptionAcknowledges = std::promise<void>{};
            allAcked = _receivedAllSubscriptionAcknowledges.get_future();
        }

        _ioContext.dispatch([this, link, endpointId, service]() {
            this->RegisterSilKitServiceImpl<SilKitServiceT>(link, endpointId, service);
        });

        if (!SilKitServiceTraits<SilKitServiceT>::UseAsyncRegistration())
        {
            _logger->Trace("VAsio waiting for subscription acknowledges for SilKitService {}.", typeid(*service).name());
            allAcked.wait();
            _logger->Trace("VAsio received all subscription acknowledges for SilKitService {}.", typeid(*service).name());
        }
    }

    template <class SilKitServiceT>
    void SetHistoryLengthForLink(const std::string& networkName, size_t historyLength, SilKitServiceT* /*service*/)
    {
        // NB: Dummy SilKitServiceT* is fed in here to deduce SilKitServiceT, as it is only used in 'typename SilKitServiceT',
        // which is not sufficient to get the type for some compilers (e.g. Clang)
        typename SilKitServiceT::SilKitSendMessagesTypes sendMessageTypes{};

        Util::tuple_tools::for_each(sendMessageTypes, [this, &networkName, historyLength](auto&& message) {
            using SilKitMessageT = std::decay_t<decltype(message)>;
            auto link = this->GetLinkByName<SilKitMessageT>(networkName); 
            link->SetHistoryLength(historyLength);
        });
    }

    template<typename SilKitMessageT>
    void SendMsg(const IServiceEndpoint* from, SilKitMessageT&& msg)
    {
        ExecuteOnIoThread(&VAsioConnection::SendMsgImpl<SilKitMessageT>, from, std::forward<SilKitMessageT>(msg));
    }

    template<typename SilKitMessageT>
    void SendMsg(const IServiceEndpoint* from, const std::string& targetParticipantName, SilKitMessageT&& msg)
    {
        ExecuteOnIoThread(&VAsioConnection::SendMsgToTargetImpl<SilKitMessageT>, from, targetParticipantName, std::forward<SilKitMessageT>(msg));
    }

    inline void OnAllMessagesDelivered(const std::function<void()>& callback)
    {
        callback();
    }

    void FlushSendBuffers() {}
    void ExecuteDeferred(std::function<void()> function)
    {
        asio::post(_ioContext.get_executor(), std::move(function));
    }

    inline auto Config() const -> const SilKit::Config::ParticipantConfiguration&
    {
        return _config;
    }

    // Temporary Helpers
    void RegisterMessageReceiver(std::function<void(IVAsioPeer* peer, ParticipantAnnouncement)> callback);
    void OnSocketData(IVAsioPeer* from, SerializedMessage&& buffer);

    // Listening Sockets (acceptors)
    void AcceptLocalConnections(const std::string& uniqueId);
    void AcceptTcpConnectionsOn(const std::string& hostname, uint16_t port);

    void StartIoWorker();

    void RegisterPeerShutdownCallback(std::function<void(IVAsioPeer* peer)> callback);
    void OnPeerShutdown(IVAsioPeer* peer);

    void NotifyShutdown();

public: //members
    static constexpr const ParticipantId RegistryParticipantId { 0 };
private:

    template<typename AcceptorT, typename EndpointT>
    void AcceptConnectionsOn(AcceptorT& acceptor, EndpointT endpoint);

    // ----------------------------------------
    // private data types
    template <class MsgT>
    using SilKitLinkMap = std::map<std::string, std::shared_ptr<SilKitLink<MsgT>>>;

    template <class MsgT>
    using SilKitServiceToReceiverMap = std::map<std::string, IMessageReceiver<MsgT>*>;
    template <class MsgT>
    using SilKitServiceToLinkMap = std::map<std::string, std::shared_ptr<SilKitLink<MsgT>>>;

    using ParticipantAnnouncementReceiver = std::function<void(IVAsioPeer* peer, ParticipantAnnouncement)>;

    using SilKitMessageTypes = std::tuple<
        Services::Logging::LogMsg,
        Services::Orchestration::NextSimTask,
        Services::Orchestration::SystemCommand,
        Services::Orchestration::ParticipantCommand,
        Services::Orchestration::ParticipantStatus,
        Services::Orchestration::WorkflowConfiguration,
        Services::PubSub::DataMessageEvent,
        Services::Rpc::FunctionCall,
        Services::Rpc::FunctionCallResponse,
        Services::Can::CanFrameEvent,
        Services::Can::CanFrameTransmitEvent,
        Services::Can::CanControllerStatus,
        Services::Can::CanConfigureBaudrate,
        Services::Can::CanSetControllerMode,
        Services::Ethernet::EthernetFrameEvent,
        Services::Ethernet::EthernetFrameTransmitEvent,
        Services::Ethernet::EthernetStatus,
        Services::Ethernet::EthernetSetMode,
        Services::Lin::LinSendFrameRequest,
        Services::Lin::LinSendFrameHeaderRequest,
        Services::Lin::LinTransmission,
        Services::Lin::LinWakeupPulse,
        Services::Lin::LinControllerConfig,
        Services::Lin::LinControllerStatusUpdate,
        Services::Lin::LinFrameResponseUpdate,
        Services::Flexray::FlexrayFrameEvent,
        Services::Flexray::FlexrayFrameTransmitEvent,
        Services::Flexray::FlexraySymbolEvent,
        Services::Flexray::FlexraySymbolTransmitEvent,
        Services::Flexray::FlexrayCycleStartEvent,
        Services::Flexray::FlexrayHostCommand,
        Services::Flexray::FlexrayControllerConfig,
        Services::Flexray::FlexrayTxBufferConfigUpdate,
        Services::Flexray::FlexrayTxBufferUpdate,
        Services::Flexray::FlexrayPocStatusEvent,
        Core::Discovery::ParticipantDiscoveryEvent,
        Core::Discovery::ServiceDiscoveryEvent,

        // Private testing data types
        Core::Tests::Version1::TestMessage,
        Core::Tests::Version2::TestMessage,
        Core::Tests::TestFrameEvent
    >;

private:
    // ----------------------------------------
    // private methods
    void ReceiveRawSilKitMessage(IVAsioPeer* from, SerializedMessage&& buffer);
    void ReceiveSubscriptionAnnouncement(IVAsioPeer* from, SerializedMessage&& buffer);
    void ReceiveSubscriptionAcknowledge(IVAsioPeer* from, SerializedMessage&& buffer);
    void ReceiveRegistryMessage(IVAsioPeer* from, SerializedMessage&& buffer);

    bool TryAddRemoteSubscriber(IVAsioPeer* from, const VAsioMsgSubscriber& subscriber);


    void UpdateParticipantStatusOnConnectionLoss(IVAsioPeer* peer);

    // Registry related send / receive methods
    void ReceiveKnownParticpants(IVAsioPeer* peer, SerializedMessage&& buffer);
    void SendParticipantAnnouncement(IVAsioPeer* peer);
    void ReceiveParticipantAnnouncement(IVAsioPeer* from, SerializedMessage&& buffer);

    void SendParticipantAnnouncementReply(IVAsioPeer* peer);
    void ReceiveParticipantAnnouncementReply(IVAsioPeer* from, SerializedMessage&& buffer);

    void NotifyNetworkIncompatibility(const RegistryMsgHeader& other, const std::string& otherParticipantName);

    void AddParticipantToLookup(const std::string& participantName);
    const std::string& GetParticipantFromLookup(std::uint64_t participantId) const;

    template<class SilKitMessageT>
    auto GetLinkByName(const std::string& networkName) -> std::shared_ptr<SilKitLink<SilKitMessageT>>
    {
        auto& link = std::get<SilKitLinkMap<SilKitMessageT>>(_links)[networkName];
        if (!link)
        {
            link = std::make_shared<SilKitLink<SilKitMessageT>>(networkName, _logger, _timeProvider);
        }
        return link;
    }

    template<class SilKitMessageT, class SilKitServiceT>
    void RegisterSilKitMsgReceiver(const std::string& networkName, SilKit::Core::IMessageReceiver<SilKitMessageT>* receiver)
    {
        assert(_logger);

        auto link = GetLinkByName<SilKitMessageT>(networkName);
        link->AddLocalReceiver(receiver);

        std::string msgSerdesName = SilKitMsgTraits<SilKitMessageT>::SerdesName();
        const std::string uniqueReceiverId = networkName + "/" + msgSerdesName;
        bool isNewReceiver = _vasioUniqueReceiverIds.insert(uniqueReceiverId).second;
        if (isNewReceiver)
        {
            // we have to subscribe to messages from other peers
            VAsioMsgSubscriber subscriptionInfo;
            subscriptionInfo.receiverIdx = static_cast<decltype(subscriptionInfo.receiverIdx)>(_vasioReceivers.size());
            subscriptionInfo.networkName = networkName;
            subscriptionInfo.msgTypeName = msgSerdesName;
            subscriptionInfo.version = SilKitMsgTraits<SilKitMessageT>::Version();

            std::unique_ptr<IVAsioReceiver> rawReceiver = std::make_unique<VAsioReceiver<SilKitMessageT>>(subscriptionInfo, link, _logger);
            auto* serviceEndpointPtr = dynamic_cast<IServiceEndpoint*>(rawReceiver.get());
            ServiceDescriptor tmpServiceDescriptor(dynamic_cast<Core::IServiceEndpoint&>(*receiver).GetServiceDescriptor());
            tmpServiceDescriptor.SetParticipantName(_participantName);
            // copy the Service Endpoint Id
            serviceEndpointPtr->SetServiceDescriptor(tmpServiceDescriptor);
            _vasioReceivers.emplace_back(std::move(rawReceiver));

            for (auto&& peer : _peers)
            {
                if (!SilKitServiceTraits<SilKitServiceT>::UseAsyncRegistration())
                {
                    _pendingSubscriptionAcknowledges.emplace_back(peer.get(), subscriptionInfo);
                }
                peer->Subscribe(subscriptionInfo);
            }
        }
    }

    template<class SilKitMessageT>
    void RegisterSilKitMsgSender(const std::string& networkName, const IServiceEndpoint* serviceId)
    {
        auto link = GetLinkByName<SilKitMessageT>(networkName);
        auto&& serviceLinkMap = std::get<SilKitServiceToLinkMap<SilKitMessageT>>(_serviceToLinkMap);
        serviceLinkMap[serviceId->GetServiceDescriptor().GetNetworkName()] = link;
    }

    template<class SilKitServiceT>
    inline void RegisterSilKitServiceImpl(const std::string& link, EndpointId /*endpointId*/, SilKitServiceT* service)
    {
        typename SilKitServiceT::SilKitReceiveMessagesTypes receiveMessageTypes{};
        typename SilKitServiceT::SilKitSendMessagesTypes sendMessageTypes{};

        Util::tuple_tools::for_each(receiveMessageTypes,
            [this, &link, service](auto&& message)
        {
            using SilKitMessageT = std::decay_t<decltype(message)>;
            this->RegisterSilKitMsgReceiver<SilKitMessageT, SilKitServiceT>(link, service);
        }
        );

        Util::tuple_tools::for_each(sendMessageTypes,
            [this, &link,  &service](auto&& message)
        {
            using SilKitMessageT = std::decay_t<decltype(message)>;
            auto& serviceId = dynamic_cast<IServiceEndpoint&>(*service);
            this->RegisterSilKitMsgSender<SilKitMessageT>(link, &serviceId);
        }
        );

        if (!SilKitServiceTraits<SilKitServiceT>::UseAsyncRegistration())
        {
            if (_pendingSubscriptionAcknowledges.empty())
            {
                _receivedAllSubscriptionAcknowledges.set_value();
            }
        }
    }

    template <class SilKitMessageT>
    void SendMsgImpl(const IServiceEndpoint* from, SilKitMessageT&& msg)
    {
        const auto& key = from->GetServiceDescriptor().GetNetworkName();

        auto& linkMap = std::get<SilKitServiceToLinkMap<std::decay_t<SilKitMessageT>>>(_serviceToLinkMap);
        if (linkMap.count(key) < 1)
        {
            throw std::runtime_error{ "VAsioConnection::SendMsgImpl: sending on empty link for " + key };
        }
        auto&& link = linkMap[key];
        link->DistributeLocalSilKitMessage(from, std::forward<SilKitMessageT>(msg));
    }

    template <class SilKitMessageT>
    void SendMsgToTargetImpl(const IServiceEndpoint* from, const std::string& targetParticipantName,
                                   SilKitMessageT&& msg)
    {
        const auto& key = from->GetServiceDescriptor().GetNetworkName();

        auto& linkMap = std::get<SilKitServiceToLinkMap<std::decay_t<SilKitMessageT>>>(_serviceToLinkMap);
        if (linkMap.count(key) < 1)
        {
            throw std::runtime_error{"VAsioConnection::SendMsgImpl: sending on empty link for " + key};
        }
        auto&& link = linkMap[key];
        link->DispatchSilKitMessageToTarget(from, targetParticipantName, std::forward<SilKitMessageT>(msg));
    }

    template <typename... MethodArgs, typename... Args>
    inline void ExecuteOnIoThread(void (VAsioConnection::*method)(MethodArgs...), Args&&... args)
    {
        if (std::this_thread::get_id() == _ioWorker.get_id())
        {
            (this->*method)(std::forward<Args>(args)...);
        }
        else
        {
            asio::dispatch(_ioContext.get_executor(), [=]() mutable { (this->*method)(std::move(args)...); });
        }
    }
    inline void ExecuteOnIoThread(std::function<void()> function)
    {
        if (std::this_thread::get_id() == _ioWorker.get_id())
        {
            function();
        }
        else
        {
            asio::dispatch(_ioContext.get_executor(), std::move(function));
        }
    }

    // TCP Related
    void AddPeer(std::shared_ptr<IVAsioPeer> peer);
    template<typename AcceptorT>
    void AcceptNextConnection(AcceptorT& acceptor);

private:
    // ----------------------------------------
    // private members
    SilKit::Config::ParticipantConfiguration _config;
    std::string _participantName;
    ParticipantId _participantId{0};
    Services::Logging::ILogger* _logger{nullptr};
    Services::Orchestration::ITimeProvider* _timeProvider{nullptr};

    //! \brief Virtual SilKit links by networkName according to SilKitConfig.
    Util::tuple_tools::wrapped_tuple<SilKitLinkMap, SilKitMessageTypes> _links;
    //! \brief Lookup for links by name.
    Util::tuple_tools::wrapped_tuple<SilKitServiceToLinkMap, SilKitMessageTypes> _serviceToLinkMap;

    std::vector<std::unique_ptr<IVAsioReceiver>> _vasioReceivers;
    std::unordered_set<std::string> _vasioUniqueReceiverIds;

    // FIXME: generalize the reception of registry data
    std::vector<ParticipantAnnouncementReceiver> _participantAnnouncementReceivers;
    std::vector<std::function<void(IVAsioPeer*)>> _peerShutdownCallbacks;

    // NB: The IO context must be listed before anything socket related.
    asio::io_context _ioContext;

    // NB: peers and acceptors must be listed AFTER the io_context. Otherwise,
    // their destructor will crash!
    std::unique_ptr<IVAsioPeer> _registry{nullptr};
    std::vector<std::shared_ptr<IVAsioPeer>> _peers;

    // We support IPv6, IPv4 and Local Domain sockets for incoming connections:
    asio::ip::tcp::acceptor _tcp4Acceptor;
    asio::ip::tcp::acceptor _tcp6Acceptor;
    asio::local::stream_protocol::acceptor _localAcceptor;

    // After receiving the list of known participants from the registry, we keep
    // track of the sent ParticipantAnnouncements and wait for the corresponding
    // replies.
    std::vector<IVAsioPeer*> _pendingParticipantReplies;
    std::promise<void> _receivedAllParticipantReplies;

    // Keep track of the sent Subscriptions when Registering an SilKit Service
    std::vector<std::pair<IVAsioPeer*, VAsioMsgSubscriber>> _pendingSubscriptionAcknowledges;
    std::promise<void> _receivedAllSubscriptionAcknowledges;

    // The worker thread should be the last members in this class. This ensures
    // that no callback is destroyed before the thread finishes.
    std::thread _ioWorker;

    //We violate the strict layering architecture, so that we can cleanly shutdown without false error messages.
    bool _isShuttingDown{false};

    // Hold mapping from hash to participantName
    std::map<uint64_t, std::string> _hashToParticipantName;

    // unit testing support
    ProtocolVersion _version;
    friend class VAsioConnectionTest;
};

} // namespace Core
} // namespace SilKit