// -*- C++ -*-
//
// $Id$

#include  "TransportSendListener.h"
#include  "TransportImpl.h"
#include  "DataLinkSet.h"
#include  "DataLinkSetMap.h"
#include  "DataLink.h"
#include  "dds/DCPS/AssociationData.h"
#include  "dds/DCPS/DataSampleList.h"
#include  "ace/Message_Block.h"
#include  "EntryExit.h"

ACE_INLINE
TAO::DCPS::TransportInterface::TransportInterface()
  : swap_bytes_(0)
{
  DBG_ENTRY("TransportInterface","TransportInterface");
  this->send_links_ = new DataLinkSet();
}



/// Return the connection_info_ that we retrieved from the TransportImpl
/// in our attach_transport() method.  Once we retrieve it from the
/// TransportImpl (in the attach_transport() method), we "cache" it in
/// a data member (this->connection_info_) so that we don't have to ask
/// the TransportImpl for it each time.
ACE_INLINE const TAO::DCPS::TransportInterfaceInfo&
TAO::DCPS::TransportInterface::connection_info() const
{
  DBG_ENTRY("TransportInterface","connection_info");
  return this->connection_info_;
}


ACE_INLINE int
TAO::DCPS::TransportInterface::swap_bytes() const
{
  DBG_ENTRY("TransportInterface","swap_bytes");
  return this->swap_bytes_;
}


ACE_INLINE TAO::DCPS::SendControlStatus
TAO::DCPS::TransportInterface::send_control(RepoId                 pub_id,
                                            TransportSendListener* listener,
                                            ACE_Message_Block*     msg)
{
  DBG_ENTRY("TransportInterface","send_control");

  DataLinkSet_rch pub_links = this->local_map_.find_set(pub_id);

  if (pub_links.is_nil())
    {
      // We get here if there aren't any remote subscribers that (currently)
      // have an interest in this publisher id.  Just like in the case of
      // the send() method, this is not an error.  A better way to understand
      // would be if this send_control() method were renamed to:
      //   send_control_to_all_interested_remote_subscribers(pub_id,...).
      //
      // And when we get to this spot in the logic, we have still fulfilled
      // our duties - we sent it to all interested remote subscribers - all
      // zero of them.
      listener->control_delivered(msg);
      return SEND_CONTROL_OK;
    }
  else
    {
      // Just have the DataLinkSet do the send_control for us, on each
      // DataLink in the set.
      return pub_links->send_control(pub_id, listener, msg);
    }
}


ACE_INLINE int
TAO::DCPS::TransportInterface::remove_sample
                                     (const DataSampleListElement* sample)
{
  DBG_ENTRY("TransportInterface","remove_sample");

  DataLinkSet_rch pub_links =
                        this->local_map_.find_set(sample->publication_id_);

  // We only need to do something here if the publication_id_ is associated
  // with at least one DataLink.  If it is not associated with any DataLinks
  // (ie, pub_links.is_nil() is true), then we don't have anything to do
  // here, and we don't consider this an error condition - just return 0
  // in the "no DataLinks associated with the sample->publication_id_" case.
  if (!pub_links.is_nil())
    {
      // Tell the DataLinkSet to tell each DataLink in the set to attempt
      // the remove_sample() operation.
      return pub_links->remove_sample(sample);
    }

  // The sample->publication_id_ isn't associated with any DataLinks, so
  // there are no samples to even attempt to remove.  This isn't considered
  // an error condition (which would require a -1 to be returned) - it just
  // means we do nothing except return 0.
//MJM: What is the use-case for this not being an error?  I am trying to
//MJM: think of one, but have been unsuccessful so far.
  return 0;
}


ACE_INLINE int
TAO::DCPS::TransportInterface::remove_all_control_msgs(RepoId pub_id)
{
  DBG_ENTRY("TransportInterface","remove_all_control_msgs");

  DataLinkSet_rch pub_links = this->local_map_.find_set(pub_id);

  if (!pub_links.is_nil())
    {
      return pub_links->remove_all_control_msgs(pub_id);
    }

//MJM: What is the use-case for this not being an error?  I am trying to
//MJM: think of one, but have been unsuccessful so far.
  return 0;
}


ACE_INLINE int
TAO::DCPS::TransportInterface::add_subscriptions
                                    (RepoId                 publisher_id,
                                     CORBA::Long            priority,
                                     ssize_t                size,
                                     const AssociationData* subscriptions)
{
  DBG_ENTRY("TransportInterface","add_subscriptions");
  // Delegate to generic add_associations operation
  return this->add_associations(publisher_id,
                                priority,
                                "publisher_id",
                                "subscriber_id",
                                size,
                                subscriptions);
}


ACE_INLINE int
TAO::DCPS::TransportInterface::add_publications
                                   (RepoId                    subscriber_id,
                                    TransportReceiveListener* receive_listener,
                                    CORBA::Long               priority,
                                    ssize_t                   size,
                                    const AssociationData*    publications)
{
  DBG_ENTRY("TransportInterface","add_publications");
  // Delegate to generic add_associations operation
  return this->add_associations(subscriber_id,
                                priority,
                                "subscriber_id",
                                "publisher_id",
                                size,
                                publications,
                                receive_listener);
}


ACE_INLINE void
TAO::DCPS::TransportInterface::send(const DataSampleList& samples)
{
  DBG_ENTRY("TransportInterface","send");

  DataSampleListElement* cur = samples.head_;

  while (cur)
    {
      // VERY IMPORTANT NOTE:
      //
      // We have to be very careful in how we deal with the current
      // DataSampleListElement.  The issue is that once we have invoked
      // data_delivered() on the send_listener_ object, or we have invoked
      // send() on the pub_links, we can no longer access the current
      // DataSampleListElement!  Thus, we need to get the next
      // DataSampleListElement (pointer) from the current element now,
      // while it is safe.
      DataSampleListElement* next_elem = cur->next_send_sample_;

      DataLinkSet_rch pub_links =
                          this->local_map_.find_set(cur->publication_id_);

      if (pub_links.is_nil())
        {
          // NOTE: This is the "local publisher id is not currently
          //       associated with any remote subscriber ids" case.

          VDBG((LM_DEBUG,"(%P|%t) DBG: "
               "TransportInterface::send no links for %d\n",
               cur->publication_id_));

          // We tell the send_listener_ that all of the remote subscriber ids
          // that wanted the data (all zero of them) have indeed received
          // the data.
          cur->send_listener_->data_delivered(cur);
        }
      else
        {
          // This will do several things, including adding to the membership
          // of the send_links_ set.  Any DataLinks added to the send_links_
          // set will be also told about the send_start() event.  Those
          // DataLinks (in the pub_links set) that are already in the
          // send_links_ set will not be told about the send_start() event
          // since they heard about it when they were inserted into the
          // send_links_ set.
          this->send_links_->send_start(pub_links.in());

          // Just have the DataLinkSet do the send for us, on each DataLink
          // in the set that is associated with the local publisher id.
          pub_links->send(cur);
        }

      // Move on to the next DataSampleListElement to send.
      cur = next_elem;
    }

  // This will inform each DataLink in the set about the stop_send() event.
  // It will then clear the send_links_ set.
  //
  // The reason that the send_links_ set is cleared is because we continually
  // reuse the same send_links_ object over and over for each call to this
  // send method.
  this->send_links_->send_stop();
}


ACE_INLINE TAO::DCPS::TransportImpl_rch
TAO::DCPS::TransportInterface::get_transport_impl ()
{
  return this->impl_;
}


