// -*- C++ -*-
//
// $Id$
#ifndef OPENDDS_DCPS_TRANSPORTIMPL_H
#define OPENDDS_DCPS_TRANSPORTIMPL_H

#include "dds/DCPS/dcps_export.h"
#include "dds/DCPS/RcObject_T.h"
#include "dds/DdsDcpsInfoUtilsC.h"
#include "dds/DCPS/AssociationData.h"
#include "TransportDefs.h"
//borland #include "TransportConfiguration.h"
#include "TransportConfiguration_rch.h"
//borland #include "TransportReactorTask.h"
#include "TransportReactorTask_rch.h"
#include "RepoIdSetMap.h"
#include "DataLinkCleanupTask.h"
#include "ace/Synch.h"
#include "ace/Vector_T.h"
#include <map>

namespace OpenDDS
{
  namespace DCPS
  {

    class TransportInterface;
    class TransportReceiveListener;
    class ThreadSynchStrategy;
    class TransportImplFactory;
    class TransportFactory;
    class DataLink;
    class DataWriterImpl;
    class DataReaderImpl;

    struct AssociationInfo
    {
      ssize_t num_associations_;
      const AssociationData* association_data_;
      enum Association_Status status_;
    };

    typedef ACE_Vector<AssociationInfo> AssociationInfoList;

    /** The TransportImpl class includes the abstract methods that must be implemented 
    *   by any implementation to provide data delivery service to the DCPS implementation. 
    *   This includes methods to send data, received data, configure the operation, and 
    *   manage associations and datalinks between local and remote objects of the implementation.
    *   
    *   Notes about object ownership:
    *   1)Has longer lifetime than the publisher and subscriber objects. The publishers 
    *     and subscribers are owned by the DomainParticipant and transport factory shutdown 
    *     is always after DomainParticipant factory shutdown. 
    *   2)The concrete transport object owns the datalink objects. 
    *   3)Own  a DataLinkCleanup object. 
    *   4)Reference to TransportConfiguration object and TransportReactorTask object owned
    *     by TransportFactory.
    *   5)During transport shutdown, if this object does not have ownership of an object
    *     but has a references via smart pointer then the reference should be freed;
    *     if this object has ownership of task objects then the tasks should be closed.
    */
    class OpenDDS_Dcps_Export TransportImpl : public RcObject<ACE_SYNCH_MUTEX>
    {
      public:

        virtual ~TransportImpl();

        /// Called by the client application some time soon after this
        /// TransportImpl has been created.  This *must* be called
        /// prior to attempting to attach this TransportImpl to a
        /// TransportInterface object.
        int configure(TransportConfiguration* config);


        /// Called by the PublisherImpl to register datawriter upon
        /// datawriter creation.
        int register_publication (RepoId pub_id, DataWriterImpl* dw);

        /// Called by the PublisherImpl to unregister datawriter upon
        /// datawriter destruction.
        int unregister_publication (RepoId pub_id);

        /// Called by the DataLink to find the registered datawriter
        /// for the lost publication notification.
      /// If a safe copy (safe_cpy) is requested, the callee is responsible
      /// for bumping down the ref count.
        DataWriterImpl* find_publication (RepoId pub_id, bool safe_cpy = false);

        /// Called by the SubscriberImpl to register datareader upon
        /// datareader creation.
        int register_subscription (RepoId sub_id, DataReaderImpl* dr);

        /// Called by the SubscriberImpl to unregister datareader upon
        /// datareader destruction.
        int unregister_subscription (RepoId sub_id);

        /// Called by the DataLink to find the registered datareader
        /// for the lost subscription notification.
      /// If a safe copy (safe_cpy) is requested, the callee is responsible
      /// for bumping down the ref count.
        DataReaderImpl* find_subscription (RepoId sub_id, bool safe_cpy = false);

        /// Called when the receive strategy received the FULLY_ASSOCIATED
        /// message.
        int demarshal_acks (ACE_Message_Block* acks, bool byte_order);

        /// Return true if all the subscriptions to a datawriter are
        /// acknowledged, otherwise return false.
        virtual bool acked (RepoId pub_id);

      /// Callback from teh DataLink to clean up any associated resources.
      /// This usually is done when the DataLink is lost. The call is made with
      /// no transport/DCPS locks held.
      bool release_link_resources (DataLink* link);

      protected:

        TransportImpl();

        /// If connect_as_publisher == 1, then this find_or_create_datalink()
        /// call is being made on behalf of a local publisher id association
        /// with a remote subscriber id.  If connect_as_publisher == 0, then
        /// this find_or_create_datalink() call is being made on behalf of a
        /// local subscriber id association with a remote publisher id.
        /// Note that this "flag" is only used if the find operation fails,
        /// and a new DataLink must created and go through connection
        /// establishment.  This allows the connection establishment logic
        /// to determine whether an active or passive connection needs to
        /// be made.  If the find operation works, then we don't need to
        /// establish a connection since the existing DataLink is already
        /// connected.
        virtual DataLink* find_or_create_datalink
                    (const TransportInterfaceInfo& remote_info,
                     int                           connect_as_publisher) = 0;

        /// Concrete subclass gets a shot at the config object.  The subclass
        /// will likely downcast the TransportConfiguration object to a
        /// subclass type that it expects/requires.
        virtual int configure_i(TransportConfiguration* config) = 0;

        /// Called during the shutdown() method in order to give the
        /// concrete TransportImpl subclass a chance to do something when
        /// the shutdown "event" occurs.
        virtual void shutdown_i() = 0;

        /// Called before transport is shutdown to let the
        /// concrete transport to do anything necessary.
        virtual void pre_shutdown_i();

        /// Called by our connection_info() method to allow the concrete
        /// TransportImpl subclass to do the dirty work since it really
        /// is the one that knows how to populate the supplied
        /// TransportInterfaceInfo object.
        virtual int connection_info_i
                              (TransportInterfaceInfo& local_info) const = 0;

        /// Called by our release_datalink() method in order to give the
        /// concrete TransportImpl subclass a chance to do something when
        /// the release_datalink "event" occurs.
        virtual void release_datalink_i(DataLink* link) = 0;

        /// Accessor to obtain a "copy" of the reference to the reactor task.
        /// Caller is responsible for the "copy" of the reference that is
        /// returned.
        TransportReactorTask* reactor_task();


      private:

        /// We have a few friends in the transport framework so that they
        /// can access our private methods.  We do this to avoid pollution
        /// of our public interface with internal framework methods.
        friend class TransportInterface;
        friend class TransportImplFactory;
        friend class TransportFactory;
        friend class DataLink;
//MJM: blech.

        /// Our friend (and creator), the TransportImplFactory, will invoke
        /// this method in order to provide us with the TransportReactorTask.
        /// This happens immediately following the creation of this
        /// TransportImpl object.
        int set_reactor(TransportReactorTask* task);

        /// Called by the TransportFactory when this TransportImpl object
        /// is released while the TransportFactory is handling a release()
        /// "event".
        void shutdown();

        /// The DataLink itself calls this method when it thinks it is
        /// no longer used for any associations.  This occurs during
        /// a "remove associations" operation being performed by some
        /// TransportInterface that uses this TransportImpl.  The
        /// TransportInterface is known to have acquired our reservation_lock_,
        /// so there won't be any reserve_datalink() calls being made from
        /// any other threads while we perform this release.
        void release_datalink(DataLink* link);

        /// Called by our friend, the TransportInterface, to attach
        /// itself to this TransportImpl object.
        AttachStatus attach_interface(TransportInterface* interface);

        /// Called by our friend, the TransportInterface, to detach
        /// itself to this TransportImpl object.
        void detach_interface(TransportInterface* interface);

        /// Called by our friend, the TransportInterface, to reserve
        /// a DataLink for a remote subscription association
        /// (a local "publisher" to a remote "subscriber" association).
        DataLink* reserve_datalink
                      (const TransportInterfaceInfo& remote_subscriber_info,
                       RepoId                        subscriber_id,
                       RepoId                        publisher_id,
                       CORBA::Long                   priority);

        /// Called by our friend, the TransportInterface, to reserve
        /// a DataLink for a remote publication association
        /// (a local "subscriber" to a remote "publisher" association).
        DataLink* reserve_datalink
                      (const TransportInterfaceInfo& remote_publisher_info,
                       RepoId                        publisher_id,
                       RepoId                        subscriber_id,
                       TransportReceiveListener*     receive_listener,
                       CORBA::Long                   priority);
//MJM: You might be able to collapse these two methods by just providing
//MJM: a default NULL recieve listener (as the last arg) as long as this
//MJM: collapsing propogates down to the datalink as well.  The method
//MJM: implementations are remarkably similar.
protected:
        typedef ACE_SYNCH_MUTEX                ReservationLockType;
        typedef ACE_Guard<ReservationLockType> ReservationGuardType;
        /// Called by our friends, the TransportInterface, and the DataLink.
        /// Since this TransportImpl can be attached to many TransportInterface
        /// objects, and each TransportInterface object could be "running" in
        /// a separate thread, we need to protect all of the "reservation"
        /// methods with a lock.  The protocol is that a client of ours
        /// must "acquire" our reservation_lock() before it can proceed to
        /// call any methods that affect the DataLink reservations.  It
        /// should release the reservation_lock() as soon as it is done.
        ReservationLockType& reservation_lock();
        const ReservationLockType& reservation_lock() const;


        /// Called on publisher side as the last step of the add_associations().
        /// The pending publications are added to the pending_sub_map_
        /// and the association data are cached to pending_association_sub_map_.
        /// If the transport already received the FULLY_ASSOCIATED acks from
        /// subscribers then the transport will notify the datawriter fully
        /// association and the associations will be
        /// removed from the pending associations cache.
        int add_pending_association (RepoId  pub_id,
                                     size_t                  num_remote_associations,
                                     const AssociationData*  subs);


private:


        /// This method is called when the FULLY_ASSOCIATED ack of the pending
        /// associations is received. If the datawriter is registered, the
        /// datawriter will be notified, otherwise the status of the pending
        /// associations will be marked as FULLTY_ASSOCIATED.
        void fully_associated (RepoId pub_id);

        /// Called by our friend, the TransportInterface.
        /// Accessor for the TransportInterfaceInfo.  Accepts a reference
        /// to a TransportInterfaceInfo object that will be "populated"
        /// with this TransportImpl's connection information (ie, how
        /// another process would connect to this TransportImpl).
        int connection_info(TransportInterfaceInfo& local_info) const;

        /// Called by our friend, the TransportInterface.
        /// Accessor for the "swap bytes" flag that was supplied to this
        /// TransportImpl via the TransportConfiguration object supplied
        /// to our configure() method.
        int swap_bytes() const;


        typedef std::map<void*, TransportInterface*>         InterfaceMapType;

        typedef ACE_SYNCH_MUTEX     LockType;
        typedef ACE_Guard<LockType> GuardType;

        typedef std::map<RepoId, DataWriterImpl*>            PublicationObjectMap;

        typedef std::map<RepoId, DataReaderImpl*>            SubscriptionObjectMap;

        typedef std::map<RepoId, AssociationInfoList*>       PendingAssociationsMap;

        /// The collection of the DataWriterImpl objects that are created by
        /// the PublisherImpl currently "attached" to this TransportImpl.
        PublicationObjectMap  dw_map_;

        /// The collection of the DataWriterImpl objects that are created by
        /// the SubscriberImpl currently "attached" to this TransportImpl.
        SubscriptionObjectMap dr_map_;
        /// Our reservation lock.
        ReservationLockType reservation_lock_;

        /// Lock to protect the interfaces_, config_ and reactor_task_
        /// data members.
        mutable LockType lock_;

        /// The collection of TransportInterface objects that are currently
        /// "attached" to this TransportImpl.
        InterfaceMapType interfaces_;

        /// A reference (via a smart pointer) to the TransportConfiguration
        /// object that was supplied to us during our configure() method.
        TransportConfiguration_rch config_;
//MJM: I still don't understand why this is shareable.

        /// The reactor (task) object - may not even be used if the concrete
        /// subclass (of TransportImpl) doesn't require a reactor.
        TransportReactorTask_rch reactor_task_;


        /// These are used by the publisher side.

        /// pubid -> pending associations (remote sub id, association data,
        /// association status) map.
        PendingAssociationsMap pending_association_sub_map_;

        /// Fully association acknowledged map. pubid -> *subid
        RepoIdSetMap acked_sub_map_;

        /// Pending association map. pubid -> *subid
        RepoIdSetMap pending_sub_map_;

      /// smart ptr to the associated DL cleanup task
      DataLinkCleanupTask dl_clean_task_;

    };

  } /* namespace DCPS */

} /* namespace OpenDDS */

#if defined (__ACE_INLINE__)
#include "TransportImpl.inl"
#endif /* __ACE_INLINE__ */

#endif  /* OPENDDS_DCPS_TRANSPORTIMPL_H */
