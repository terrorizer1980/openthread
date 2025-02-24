/*
 *  Copyright (c) 2020, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for SRP server.
 */

#ifndef NET_SRP_SERVER_HPP_
#define NET_SRP_SERVER_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_SRP_SERVER_ENABLE

#if !OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE
#error "OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE is required for OPENTHREAD_CONFIG_SRP_SERVER_ENABLE"
#endif

#if !OPENTHREAD_CONFIG_ECDSA_ENABLE
#error "OPENTHREAD_CONFIG_ECDSA_ENABLE is required for OPENTHREAD_CONFIG_SRP_SERVER_ENABLE"
#endif

#include <openthread/ip6.h>
#include <openthread/srp_server.h>

#include "common/clearable.hpp"
#include "common/linked_list.hpp"
#include "common/locator.hpp"
#include "common/non_copyable.hpp"
#include "common/notifier.hpp"
#include "common/timer.hpp"
#include "crypto/ecdsa.hpp"
#include "net/dns_types.hpp"
#include "net/ip6.hpp"
#include "net/ip6_address.hpp"
#include "net/udp6.hpp"

namespace ot {
namespace Srp {

/**
 * This class implements the SRP server.
 *
 */
class Server : public InstanceLocator, private NonCopyable
{
    friend class ot::Notifier;
    friend class UpdateMetadata;
    friend class Service;
    friend class Host;

public:
    enum : uint16_t
    {
        kUdpPortMin = OPENTHREAD_CONFIG_SRP_SERVER_UDP_PORT_MIN, ///< The reserved min SRP Server UDP listening port.
        kUdpPortMax = OPENTHREAD_CONFIG_SRP_SERVER_UDP_PORT_MAX, ///< The reserved max SRP Server UDP listening port.
    };
    static_assert(kUdpPortMin <= kUdpPortMax, "invalid port range");

    /**
     * The ID of SRP service update transaction.
     *
     */
    typedef otSrpServerServiceUpdateId ServiceUpdateId;

    class Host;
    class Service;

    /**
     * This class implements a server-side SRP service.
     *
     */
    class Service : public LinkedListEntry<Service>, private NonCopyable
    {
        friend class LinkedListEntry<Service>;
        friend class Server;

    public:
        /**
         * This method creates a new Service object with given full name.
         *
         * @param[in]  aFullName  The full name of the service instance.
         *
         * @returns  A pointer to the newly created Service object, nullptr if
         *           cannot allocate memory for the object.
         *
         */
        static Service *New(const char *aFullName);

        /**
         * This method frees the Service object.
         *
         */
        void Free(void);

        /**
         * This method tells if the SRP service has been deleted.
         *
         * A SRP service can be deleted but retains its name for future uses.
         * In this case, the service instance is not removed from the SRP server/registry.
         * It is guaranteed that all services are deleted if the host is deleted.
         *
         * @returns  TRUE if the service has been deleted, FALSE if not.
         *
         */
        bool IsDeleted(void) const { return mIsDeleted; }

        /**
         * This method returns the full name of the service.
         *
         * @returns  A pointer to the null-terminated service name string.
         *
         */
        const char *GetFullName(void) const { return mFullName; }

        /**
         * This method returns the port of the service instance.
         *
         * @returns  The port of the service.
         *
         */
        uint16_t GetPort(void) const { return mPort; }

        /**
         * This method returns the weight of the service instance.
         *
         * @returns  The weight of the service.
         *
         */
        uint16_t GetWeight(void) const { return mWeight; }

        /**
         * This method returns the priority of the service instance.
         *
         * @param[in]  aService  A pointer to the SRP service.
         *
         * @returns  The priority of the service.
         *
         */
        uint16_t GetPriority(void) const { return mPriority; }

        /**
         * This method returns the TXT record data of the service instance.
         *
         * @returns A pointer to the buffer containing the TXT record data.
         *
         */
        const uint8_t *GetTxtData(void) const { return mTxtData; }

        /**
         * This method returns the TXT recored data length of the service instance.
         *
         * @return The TXT record data length (number of bytes in buffer returned from `GetTxtData()`).
         *
         */
        uint16_t GetTxtDataLength(void) const { return mTxtLength; }

        /**
         * This method returns the host which the service instance reside on.
         *
         * @returns  A reference to the host instance.
         *
         */
        const Host &GetHost(void) const { return *static_cast<const Host *>(mHost); }

        /**
         * This method returns the expire time (in milliseconds) of the service.
         *
         * @returns  The service expire time in milliseconds.
         *
         */
        TimeMilli GetExpireTime(void) const;

        /**
         * This method returns the key expire time (in milliseconds) of the service.
         *
         * @returns  The service key expire time in milliseconds.
         *
         */
        TimeMilli GetKeyExpireTime(void) const;

        /**
         * This method tells whether this service matches a given full name.
         *
         * @param[in]  aFullName  The full name.
         *
         * @returns  TRUE if the service matches the full name, FALSE if doesn't match.
         *
         */
        bool Matches(const char *aFullName) const;

        /**
         * This method tells whether this service matches a given service name <Service>.<Domain>.
         *
         * @param[in] aServiceName  The full service name to match.
         *
         * @retval  TRUE   If the service matches the full service name.
         * @retval  FALSE  If the service does not match the full service name.
         *
         */
        bool MatchesServiceName(const char *aServiceName) const;

    private:
        explicit Service(void);
        Error SetFullName(const char *aFullName);
        Error SetTxtData(const uint8_t *aTxtData, uint16_t aTxtDataLength);
        Error SetTxtDataFromMessage(const Message &aMessage, uint16_t aOffset, uint16_t aLength);
        Error CopyResourcesFrom(const Service &aService);
        void  ClearResources(void);

        char *           mFullName;
        uint16_t         mPriority;
        uint16_t         mWeight;
        uint16_t         mPort;
        uint16_t         mTxtLength;
        uint8_t *        mTxtData;
        otSrpServerHost *mHost;
        Service *        mNext;
        TimeMilli        mTimeLastUpdate;
        bool             mIsDeleted;
    };

    /**
     * This class implements the Host which registers services on the SRP server.
     *
     */
    class Host : public LinkedListEntry<Host>, public InstanceLocator, private NonCopyable
    {
        friend class LinkedListEntry<Host>;
        friend class Server;
        friend class UpdateMetadata;

    public:
        /**
         * This method creates a new Host object.
         *
         * @param[in]  aInstance  A reference to the OpenThread instance.
         *
         * @returns  A pointer to the newly created Host object, nullptr if
         *           cannot allocate memory for the object.
         *
         */
        static Host *New(Instance &aInstance);

        /**
         * This method Frees the Host object.
         *
         */
        void Free(void);

        /**
         * This method tells whether the Host object has been deleted.
         *
         * The Host object retains event if the host has been deleted by the SRP client,
         * because the host name may retain.
         *
         * @returns  TRUE if the host is deleted, FALSE if the host is not deleted.
         *
         */
        bool IsDeleted(void) const { return (mLease == 0); }

        /**
         * This method returns the full name of the host.
         *
         * @returns  A pointer to the null-terminated full host name.
         *
         */
        const char *GetFullName(void) const { return mFullName; }

        /**
         * This method returns addresses of the host.
         *
         * @param[out]  aAddressesNum  The number of the addresses.
         *
         * @returns  A pointer to the addresses array.
         *
         */
        const Ip6::Address *GetAddresses(uint8_t &aAddressesNum) const
        {
            aAddressesNum = mAddressesNum;
            return mAddresses;
        }

        /**
         * This method returns the LEASE time of the host.
         *
         * @returns  The LEASE time in seconds.
         *
         */
        uint32_t GetLease(void) const { return mLease; }

        /**
         * This method returns the KEY-LEASE time of the key of the host.
         *
         * @returns  The KEY-LEASE time in seconds.
         *
         */
        uint32_t GetKeyLease(void) const { return mKeyLease; }

        /**
         * This method returns the KEY resource of the host.
         *
         * @returns  A pointer to the ECDSA P 256 public key if there is valid one.
         *           nullptr if no valid key exists.
         *
         */
        const Dns::Ecdsa256KeyRecord *GetKey(void) const { return mKey.IsValid() ? &mKey : nullptr; }

        /**
         * This method returns the expire time (in milliseconds) of the host.
         *
         * @returns  The expire time in milliseconds.
         *
         */
        TimeMilli GetExpireTime(void) const;

        /**
         * This method returns the expire time (in milliseconds) of the key of the host.
         *
         * @returns  The expire time of the key in milliseconds.
         *
         */
        TimeMilli GetKeyExpireTime(void) const;

        /**
         * This method returns the next service of the host.
         *
         * @param[in]  aService  A pointer to current service.
         *
         * @returns  A pointer to the next service or NULL if no more services exist.
         *
         */
        const Service *GetNextService(const Service *aService) const
        {
            return aService ? aService->GetNext() : mServices.GetHead();
        }

        /**
         * This method tells whether the host matches a given full name.
         *
         * @param[in]  aFullName  The full name.
         *
         * @returns  A boolean that indicates whether the host matches the given name.
         *
         */
        bool Matches(const char *aName) const;

    private:
        enum : uint8_t
        {
            kMaxAddressesNum = OPENTHREAD_CONFIG_SRP_SERVER_MAX_ADDRESSES_NUM,
        };

        explicit Host(Instance &aInstance);
        Error    SetFullName(const char *aFullName);
        void     SetKey(Dns::Ecdsa256KeyRecord &aKey);
        void     SetLease(uint32_t aLease);
        void     SetKeyLease(uint32_t aKeyLease);
        Service *GetNextService(Service *aService) { return aService ? aService->GetNext() : mServices.GetHead(); }
        Service *AddService(const char *aFullName);
        void     RemoveService(Service *aService, bool aRetainName, bool aNotifyServiceHandler);
        void     FreeAllServices(void);
        void     ClearResources(void);
        void     CopyResourcesFrom(const Host &aHost);
        Service *FindService(const char *aFullName);
        const Service *FindService(const char *aFullName) const;
        Error          AddIp6Address(const Ip6::Address &aIp6Address);

        char *       mFullName;
        Ip6::Address mAddresses[kMaxAddressesNum];
        uint8_t      mAddressesNum;
        Host *       mNext;

        Dns::Ecdsa256KeyRecord mKey;
        uint32_t               mLease;    // The LEASE time in seconds.
        uint32_t               mKeyLease; // The KEY-LEASE time in seconds.
        TimeMilli              mTimeLastUpdate;
        LinkedList<Service>    mServices;
    };

    /**
     * This class handles LEASE and KEY-LEASE configurations.
     *
     */
    class LeaseConfig : public otSrpServerLeaseConfig
    {
        friend class Server;

    public:
        /**
         * This constructor initialize to default LEASE and KEY-LEASE configurations.
         *
         */
        LeaseConfig(void);

    private:
        bool     IsValid(void) const;
        uint32_t GrantLease(uint32_t aLease) const;
        uint32_t GrantKeyLease(uint32_t aKeyLease) const;
    };

    /**
     * This constructor initializes the SRP server object.
     *
     * @param[in]  aInstance  A reference to the OpenThread instance.
     *
     */
    explicit Server(Instance &aInstance);
    ~Server(void);

    /**
     * This method sets the SRP service events handler.
     *
     * @param[in]  aServiceHandler         A service events handler.
     * @param[in]  aServiceHandlerContext  A pointer to arbitrary context information.
     *
     * @note  The handler SHOULD call HandleServiceUpdateResult to report the result of its processing.
     *        Otherwise, a SRP update will be considered failed.
     *
     * @sa  HandleServiceUpdateResult
     *
     */
    void SetServiceHandler(otSrpServerServiceUpdateHandler aServiceHandler, void *aServiceHandlerContext);

    /**
     * This method returns the domain authorized to the SRP server.
     *
     * If the domain if not set by SetDomain, "default.service.arpa." will be returned.
     * A trailing dot is always appended even if the domain is set without it.
     *
     * @returns A pointer to the dot-joined domain string.
     *
     */
    const char *GetDomain(void) const;

    /**
     * This method sets the domain on the SRP server.
     *
     * A trailing dot will be appended to @p aDomain if it is not already there.
     * This method should only be called before the SRP server is enabled.
     *
     * @param[in]  aDomain  The domain to be set. MUST NOT be nullptr.
     *
     * @retval  kErrorNone          Successfully set the domain to @p aDomain.
     * @retval  kErrorInvalidState  The SRP server is already enabled and the Domain cannot be changed.
     * @retval  kErrorInvalidArgs   The argument @p aDomain is not a valid DNS domain name.
     * @retval  kErrorNoBufs        There is no memory to store content of @p aDomain.
     *
     */
    Error SetDomain(const char *aDomain);

    /**
     * This method tells whether the SRP server is currently running.
     *
     * @returns  A boolean that indicates whether the server is running.
     *
     */
    bool IsRunning(void) const;

    /**
     * This method enables/disables the SRP server.
     *
     * @param[in]  aEnabled  A boolean to enable/disable the SRP server.
     *
     */
    void SetEnabled(bool aEnabled);

    /**
     * This method returns the LEASE and KEY-LEASE configurations.
     *
     * @param[out]  aLeaseConfig  A reference to the `LeaseConfig` instance.
     *
     */
    void GetLeaseConfig(LeaseConfig &aLeaseConfig) const;

    /**
     * This method sets the LEASE and KEY-LEASE configurations.
     *
     * When a LEASE time is requested from a client, the granted value will be
     * limited in range [aMinLease, aMaxLease]; and a KEY-LEASE will be granted
     * in range [aMinKeyLease, aMaxKeyLease].
     *
     * @param[in]  aLeaseConfig  A reference to the `LeaseConfig` instance.
     *
     * @retval  kErrorNone         Successfully set the LEASE and KEY-LEASE ranges.
     * @retval  kErrorInvalidArgs  The LEASE or KEY-LEASE range is not valid.
     *
     */
    Error SetLeaseConfig(const LeaseConfig &aLeaseConfig);

    /**
     * This method returns the next registered SRP host.
     *
     * @param[in]  aHost  The current SRP host; use nullptr to get the first SRP host.
     *
     * @returns  A pointer to the next SRP host or nullptr if no more SRP hosts can be found.
     *
     */
    const Host *GetNextHost(const Host *aHost);

    /**
     * This method receives the service update result from service handler set by
     * SetServiceHandler.
     *
     * @param[in]  aId     The ID of the service update transaction.
     * @param[in]  aError  The service update result.
     *
     */
    void HandleServiceUpdateResult(ServiceUpdateId aId, Error aError);

private:
    enum : uint16_t
    {
        kUdpPayloadSize = Ip6::Ip6::kMaxDatagramLength - sizeof(Ip6::Udp::Header), // Max UDP payload size
    };

    enum : uint32_t
    {
        kDefaultMinLease             = 60u * 30,        // Default minimum lease time, 30 min (in seconds).
        kDefaultMaxLease             = 3600u * 2,       // Default maximum lease time, 2 hours (in seconds).
        kDefaultMinKeyLease          = 3600u * 24,      // Default minimum key-lease time, 1 day (in seconds).
        kDefaultMaxKeyLease          = 3600u * 24 * 14, // Default maximum key-lease time, 14 days (in seconds).
        kDefaultEventsHandlerTimeout = OPENTHREAD_CONFIG_SRP_SERVER_SERVICE_UPDATE_TIMEOUT,
    };

    // This class includes metadata for processing a SRP update (register, deregister)
    // and sending DNS response to the client.
    class UpdateMetadata : public InstanceLocator, public LinkedListEntry<UpdateMetadata>
    {
        friend class LinkedListEntry<UpdateMetadata>;

    public:
        static UpdateMetadata *  New(Instance &               aInstance,
                                     const Dns::UpdateHeader &aHeader,
                                     Host *                   aHost,
                                     const Ip6::MessageInfo & aMessageInfo);
        void                     Free(void);
        TimeMilli                GetExpireTime(void) const { return mExpireTime; }
        const Dns::UpdateHeader &GetDnsHeader(void) const { return mDnsHeader; }
        ServiceUpdateId          GetId(void) const { return mId; }
        Host &                   GetHost(void) { return *mHost; }
        const Ip6::MessageInfo & GetMessageInfo(void) const { return mMessageInfo; }
        bool                     Matches(ServiceUpdateId aId) const { return mId == aId; }

    private:
        UpdateMetadata(Instance &               aInstance,
                       const Dns::UpdateHeader &aHeader,
                       Host *                   aHost,
                       const Ip6::MessageInfo & aMessageInfo);

        TimeMilli         mExpireTime;
        Dns::UpdateHeader mDnsHeader;
        ServiceUpdateId   mId;          // The ID of this service update transaction.
        Host *            mHost;        // The host will be updated. The UpdateMetadata has no ownership of this host.
        Ip6::MessageInfo  mMessageInfo; // The message info of the DNS update request.
        UpdateMetadata *  mNext;
    };

    void  Start(void);
    void  Stop(void);
    void  HandleNotifierEvents(Events aEvents);
    Error PublishServerData(void);
    void  UnpublishServerData(void);

    ServiceUpdateId AllocateId(void) { return mServiceUpdateId++; }

    void  CommitSrpUpdate(Error                    aError,
                          const Dns::UpdateHeader &aDnsHeader,
                          Host &                   aHost,
                          const Ip6::MessageInfo & aMessageInfo);
    void  HandleDnsUpdate(Message &                aMessage,
                          const Ip6::MessageInfo & aMessageInfo,
                          const Dns::UpdateHeader &aDnsHeader,
                          uint16_t                 aOffset);
    Error ProcessUpdateSection(Host &                   aHost,
                               const Message &          aMessage,
                               const Dns::UpdateHeader &aDnsHeader,
                               const Dns::Zone &        aZone,
                               uint16_t &               aOffset) const;
    Error ProcessAdditionalSection(Host *                   aHost,
                                   const Message &          aMessage,
                                   const Dns::UpdateHeader &aDnsHeader,
                                   uint16_t &               aOffset) const;
    Error VerifySignature(const Dns::Ecdsa256KeyRecord &aKey,
                          const Message &               aMessage,
                          Dns::UpdateHeader             aDnsHeader,
                          uint16_t                      aSigOffset,
                          uint16_t                      aSigRdataOffset,
                          uint16_t                      aSigRdataLength,
                          const char *                  aSignerName) const;
    Error ProcessZoneSection(const Message &          aMessage,
                             const Dns::UpdateHeader &aDnsHeader,
                             uint16_t &               aOffset,
                             Dns::Zone &              aZone) const;
    Error ProcessHostDescriptionInstruction(Host &                   aHost,
                                            const Message &          aMessage,
                                            const Dns::UpdateHeader &aDnsHeader,
                                            const Dns::Zone &        aZone,
                                            uint16_t                 aOffset) const;
    Error ProcessServiceDiscoveryInstructions(Host &                   aHost,
                                              const Message &          aMessage,
                                              const Dns::UpdateHeader &aDnsHeader,
                                              const Dns::Zone &        aZone,
                                              uint16_t                 aOffset) const;
    Error ProcessServiceDescriptionInstructions(Host &                   aHost,
                                                const Message &          aMessage,
                                                const Dns::UpdateHeader &aDnsHeader,
                                                const Dns::Zone &        aZone,
                                                uint16_t &               aOffset) const;

    static bool    IsValidDeleteAllRecord(const Dns::ResourceRecord &aRecord);
    const Service *FindService(const char *aFullName) const;

    void        HandleUpdate(const Dns::UpdateHeader &aDnsHeader, Host *aHost, const Ip6::MessageInfo &aMessageInfo);
    void        AddHost(Host *aHost);
    void        RemoveHost(Host *aHost, bool aRetainName, bool aNotifyServiceHandler);
    bool        HasNameConflictsWith(Host &aHost) const;
    void        SendResponse(const Dns::UpdateHeader &   aHeader,
                             Dns::UpdateHeader::Response aResponseCode,
                             const Ip6::MessageInfo &    aMessageInfo);
    void        SendResponse(const Dns::UpdateHeader &aHeader,
                             uint32_t                 aLease,
                             uint32_t                 aKeyLease,
                             const Ip6::MessageInfo & aMessageInfo);
    static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);
    void        HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    static void HandleLeaseTimer(Timer &aTimer);
    void        HandleLeaseTimer(void);
    static void HandleOutstandingUpdatesTimer(Timer &aTimer);
    void        HandleOutstandingUpdatesTimer(void);

    void                  HandleServiceUpdateResult(UpdateMetadata *aUpdate, Error aError);
    const UpdateMetadata *FindOutstandingUpdate(const Ip6::MessageInfo &aMessageInfo, uint16_t aDnsMessageId);

    Ip6::Udp::Socket                mSocket;
    otSrpServerServiceUpdateHandler mServiceUpdateHandler;
    void *                          mServiceUpdateHandlerContext;

    char *mDomain;

    LeaseConfig mLeaseConfig;

    LinkedList<Host> mHosts;
    TimerMilli       mLeaseTimer;

    TimerMilli                 mOutstandingUpdatesTimer;
    LinkedList<UpdateMetadata> mOutstandingUpdates;

    ServiceUpdateId mServiceUpdateId;
    bool            mEnabled : 1;
    bool            mHasRegisteredAnyService : 1;
};

} // namespace Srp
} // namespace ot

#endif // OPENTHREAD_CONFIG_SRP_SERVER_ENABLE
#endif // NET_SRP_SERVER_HPP_
