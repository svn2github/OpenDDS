// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_SIMPLETCPTRANSPORT_H
#define TAO_DCPS_SIMPLETCPTRANSPORT_H

#include  "dds/DCPS/dcps_export.h"
#include  "dds/DCPS/transport/framework/TransportImpl.h"
#include  "SimpleTcpConfiguration_rch.h"
#include  "SimpleTcpDataLink_rch.h"
#include  "SimpleTcpAcceptor.h"
#include  "SimpleTcpConnection_rch.h"
#include  "SimpleTcpReconnectTask_rch.h"
#include  "dds/DCPS/transport/framework/TransportReactorTask_rch.h"
#include  "ace/INET_Addr.h"
#include  "ace/Hash_Map_Manager.h"
#include  "ace/Synch.h"


namespace TAO
{

  namespace DCPS
  {

    class TAO_DdsDcps_Export SimpleTcpTransport : public TransportImpl
    {
      public:

        SimpleTcpTransport();
        virtual ~SimpleTcpTransport();

        SimpleTcpConfiguration* get_configuration();

        int fresh_link (const ACE_INET_Addr&    remote_addr,
                        SimpleTcpConnection_rch connection);


      protected:

        /// Either find a suitable DataLink that already exists (and is
        /// connected), or create one, connect it, save it off for reuse,
        /// and return it.
        virtual DataLink* find_or_create_datalink
                          (const TransportInterfaceInfo& remote_info,
                           int                           connect_as_publisher);

        virtual int configure_i(TransportConfiguration* config);

        virtual void shutdown_i();

        virtual int connection_info_i
                                 (TransportInterfaceInfo& local_info) const;

        /// Called by the DataLink to release itself.
        virtual void release_datalink_i(DataLink* link);

      private:

        /// The SimpleTcpConnection is our friend.  It tells us when it
        /// has been created (by our acceptor_), and is seeking the
        /// DataLink that should be (or will be) expecting the passive
        /// connection.
        friend class SimpleTcpConnection;

        /// Called by the SimpleTcpConnection object when it has been
        /// created by the acceptor and needs to be attached to a DataLink.
        /// The DataLink may or may not already be created and waiting
        /// for this passive connection to appear.
        /// Note that the SimpleTcpConnection* "ownership" is given away
        /// to the passive_connection() call.
        void passive_connection(const ACE_INET_Addr& remote_address,
                                SimpleTcpConnection* connection);

        /// Called by find_or_create_datalink().
        int make_active_connection(const ACE_INET_Addr& remote_address,
                                   SimpleTcpDataLink*   link);

        /// Called by find_or_create_datalink().
        int make_passive_connection(const ACE_INET_Addr& remote_address,
                                    SimpleTcpDataLink*   link);

        /// Code common to make_active_connection() and
        /// make_passive_connection().
        int connect_datalink(SimpleTcpDataLink*   link,
                             SimpleTcpConnection* connection);


        /// Map Type: (key) ACE_INET_Addr to (value) SimpleTcpDataLink_rch
        typedef ACE_Hash_Map_Manager_Ex
                               <ACE_INET_Addr,
                                SimpleTcpDataLink_rch,
                                ACE_Hash<ACE_INET_Addr>,
                                ACE_Equal_To<ACE_INET_Addr>,
                                ACE_Null_Mutex>              AddrLinkMap;

        /// Map Type: (key) ACE_INET_Addr to (value) SimpleTcpConnection_rch
        typedef ACE_Hash_Map_Manager_Ex
                               <ACE_INET_Addr,
                                SimpleTcpConnection_rch,
                                ACE_Hash<ACE_INET_Addr>,
                                ACE_Equal_To<ACE_INET_Addr>,
                                ACE_Null_Mutex>              AddrConnectionMap;

        typedef ACE_SYNCH_MUTEX         LockType;
        typedef ACE_Guard<LockType>     GuardType;
        typedef ACE_Condition<LockType> ConditionType;

// TBD SOON - Something needs to protect the tcp_config_ reference
//            because it gets set in our configure() method, and
//            dropped in our shutdown_i() method.  Maybe we can just
//            assume that configure() can remain unlocked (why not lock it
//            though - it isn't in the send or receive path?) 
//            Step back and take a look at when the various locks in here
//            get used - and if not in the direct send or recieve path,
//            maybe we can simplify this at the expense of a little more
//            locking (ie, longer critical sections).  And the base
//            class has a lock that might work for us - check out if
//            our base class should do the locking, and then we can
//            assume it has been done for us (in various situations).

        /// Used to accept passive connections on our local_address_.
        SimpleTcpAcceptor acceptor_;

        /// Our configuration object, supplied to us in config_i().
        SimpleTcpConfiguration_rch tcp_config_;

        /// This is the map of connected DataLinks.
        AddrLinkMap links_;

        /// This lock is used to protect the links_ data member.
        LockType links_lock_;

        /// Map of passive connection objects that need to be paired
        /// with a DataLink.
        AddrConnectionMap connections_;

        /// Condition that will be signal()'ed whenever something has been
        /// inserted into connections_.
        ConditionType connections_updated_;

        /// This protects the connections_ and the connections_updated_
        /// data members.
        LockType connections_lock_;

        /// We need the reactor for our Acceptor.
        TransportReactorTask_rch   reactor_task_;

        SimpleTcpReconnectTask_rch reconnect_task_;
    };

  } /* namespace DCPS */

} /* namespace TAO */

#if defined (__ACE_INLINE__)
#include "SimpleTcpTransport.inl"
#endif /* __ACE_INLINE__ */


#endif  /* TAO_DCPS_SIMPLETCPTRANSPORT_H */
