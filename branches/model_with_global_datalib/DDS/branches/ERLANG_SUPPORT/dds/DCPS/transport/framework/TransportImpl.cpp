// -*- C++ -*-
//
// $Id$

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TransportImpl.h"
#include "dds/DCPS/DataWriterImpl.h"
#include "dds/DCPS/DataReaderImpl.h"
#include "dds/DCPS/PublisherImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/Util.h"
#include "tao/debug.h"
#include <sstream>


#if !defined (__ACE_INLINE__)
#include "TransportImpl.inl"
#endif /* __ACE_INLINE__ */

namespace
{
  template <typename Container>
  void clear(Container& c)
  {
    Container copy;
    copy.swap(c);
    for (typename Container::iterator itr = copy.begin();
      itr != copy.end();
      ++itr)
    {
      itr->second->_remove_ref ();
    }
  }
}

OpenDDS::DCPS::TransportImpl::~TransportImpl()
{
  DBG_ENTRY_LVL("TransportImpl","~TransportImpl",6);
  PendingAssociationsMap::iterator penditer =
    pending_association_sub_map_.begin();

  while(penditer != pending_association_sub_map_.end())
  {
    delete (penditer->second);
    ++penditer;
  }

  clear(dw_map_);
  clear(dr_map_);
}


void
OpenDDS::DCPS::TransportImpl::shutdown()
{
  DBG_ENTRY_LVL("TransportImpl","shutdown",6);

  // Stop datalink clean task.
  this->dl_clean_task_.close (1);

  if (! this->reactor_task_.is_nil ())
  {
    this->reactor_task_->stop ();
  }

  this->pre_shutdown_i();

  {
    GuardType guard(this->lock_);

    if (this->config_.is_nil())
      {
        // This TransportImpl is already shutdown.
//MJM: So, I read here that config_i() actually "starts" us?
        return;
      }

    for (InterfaceMapType::iterator itr = interfaces_.begin();
         itr != interfaces_.end();
         ++itr)
      {
        itr->second->transport_detached();
      }

    // Clear our collection of TransportInterface pointers.
    interfaces_.clear();

//MJM: Won't you need to ACE_UNUSED_ARG here since you are depending on
//MJM: side effects here?

    // We can release our lock_ now.
  }

  // Tell our subclass about the "shutdown event".
  this->shutdown_i();

  {
    GuardType guard(this->lock_);
    this->reactor_task_ = 0;
    // The shutdown_i() path may access the configuration so remove configuration
    // reference after shutdown is performed.

    // Drop our references to the config_.
    this->config_ = 0;
  }
}


OpenDDS::DCPS::DataLink*
OpenDDS::DCPS::TransportImpl::reserve_datalink
                      (const TransportInterfaceInfo& remote_subscriber_info,
                       RepoId                        subscriber_id,
                       RepoId                        publisher_id,
                       CORBA::Long                   priority)
{
  DBG_ENTRY_LVL("TransportImpl","reserve_datalink",6);

  // Not used right now - not sure how it would apply either.
  ACE_UNUSED_ARG(priority);

  // Ask our concrete subclass to find or create a (concrete) DataLink
  // that matches the supplied criterea.

  // Note that we pass-in a 1 as the second argument.  This means that
  // if a new DataLink needs to be created (ie, the find operation fails),
  // then the connection establishment logic will treat the local endpoint
  // as a publisher.  This knowledge dictates whether a passive or active
  // connection establishment procedure should be followed.
  DataLink_rch link = this->find_or_create_datalink(remote_subscriber_info, 1);

  if (link.is_nil())
    {
      ::OpenDDS::DCPS::GuidConverter writerConverter( publisher_id);
      ::OpenDDS::DCPS::GuidConverter readerConverter( subscriber_id);
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: TransportImpl::reserve_datalink: ")
        ACE_TEXT("subclass was unable to find ")
        ACE_TEXT("or create a DataLink for local publisher_id %s ")
        ACE_TEXT("to remote subscriber_id %s.\n"),
        (const char*) writerConverter,
        (const char*) readerConverter
      ),0);
    }

  link->make_reservation(subscriber_id,publisher_id);

  return link._retn();
}


OpenDDS::DCPS::DataLink*
OpenDDS::DCPS::TransportImpl::reserve_datalink
                      (const TransportInterfaceInfo& remote_publisher_info,
                       RepoId                        publisher_id,
                       RepoId                        subscriber_id,
                       TransportReceiveListener*     receive_listener,
                       CORBA::Long                   priority)
{
  DBG_ENTRY_LVL("TransportImpl","reserve_datalink",6);

  // Not used right now - not sure how it would apply either.
  ACE_UNUSED_ARG(priority);

  // Ask our concrete subclass to find or create a DataLink (actually, a
  // concrete subclass of DataLink) that matches the supplied criterea.
  // Since find_or_create() is pure virtual, the concrete subclass must
  // provide an implementation for us to use.

  // Note that we pass-in a 0 as the second argument.  This means that
  // if a new DataLink needs to be created (ie, the find operation fails),
  // then the connection establishment logic will treat the local endpoint
  // as a subscriber.  This knowledge dictates whether a passive or active
  // connection establishment procedure should be followed.
  DataLink_rch link = this->find_or_create_datalink(remote_publisher_info,0);

  if (link.is_nil())
    {
      ::OpenDDS::DCPS::GuidConverter writerConverter( publisher_id);
      ::OpenDDS::DCPS::GuidConverter readerConverter( subscriber_id);
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: TransportImpl::reserve_datalink: ")
        ACE_TEXT("subclass was unable to find ")
        ACE_TEXT("or create a DataLink for local publisher_id %s ")
        ACE_TEXT("to remote subscriber_id %s.\n"),
        (const char*) writerConverter,
        (const char*) readerConverter
      ),0);
    }

  link->make_reservation(publisher_id,
                         subscriber_id,
                         receive_listener);

  // This is called on the subscriber side to let the concrete
  // datalink to do some necessary work such as SimpleTcp will
  // send the FULLY_ASSOCIATED ack to the publisher.
  link->fully_associated ();

  return link._retn();
}


/// This is called by a TransportInterface object when it is handling
/// its own request to attach_transport(TransportImpl*), and this
/// TransportImpl object is the one to which it should be attached.
OpenDDS::DCPS::AttachStatus
OpenDDS::DCPS::TransportImpl::attach_interface(TransportInterface* transport_interface)
{
  DBG_ENTRY_LVL("TransportImpl","attach_interface",6);

  GuardType guard(this->lock_);

  if (this->config_.is_nil())
    {
      // Can't attach to a TransportImpl that isn't currently configured.
      // This could mean that this TransportImpl object has never had its
      // configure() method called, or it could mean that this TransportImpl
      // object was shutdown() after the configure() method was called.
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) ERROR: Cannot attach_listener() to TransportImpl "
                        "object because TransportImpl is not configured, "
                        "or has been shutdown.\n"),
                       ATTACH_BAD_TRANSPORT);
    }

    if (bind(interfaces_, transport_interface, transport_interface) != 0)
    {
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) ERROR: Cannot attach_listener() to TransportImpl "
                        "object because TransportImpl thinks the "
                        "TransportInterface object is already attached.\n"),
                       ATTACH_BAD_TRANSPORT);
    }

  // Everything worked.  Return success code.
  return ATTACH_OK;
}


int
OpenDDS::DCPS::TransportImpl::register_publication (OpenDDS::DCPS::RepoId pub_id,
                                                OpenDDS::DCPS::DataWriterImpl* dw)
{
  DBG_ENTRY_LVL("TransportImpl","register_publication",6);
  GuardType guard(this->lock_);

  int ret =
    bind(dw_map_, pub_id, dw);

  if (ret != -1)
    {
      dw->_add_ref ();
    }

  // It's possiable this function is called after the
  // add_association is handled and also the FULLY_ASSOCIATED
  // ack is received by the publisher side, we need check the
  // map to see if it's the case. If it is,the datawriter will be
  // notified fully associated at this time.

  check_fully_association (pub_id);

  if (::OpenDDS::DCPS::Transport_debug_level > 8)
    {
      ::OpenDDS::DCPS::GuidConverter converter( pub_id);
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) transport %x added publication %s.\n"),
        this,
        (const char*) converter
      ));
    }

  return ret;
}


int
OpenDDS::DCPS::TransportImpl::unregister_publication (OpenDDS::DCPS::RepoId pub_id)
{
  DBG_ENTRY_LVL("TransportImpl","unregister_publication",6);
  GuardType guard(this->lock_);
  PublicationObjectMap::iterator iter = dw_map_.find(pub_id);
  int ret = -1;
  if (iter != dw_map_.end())
  {
    ret = 0;
    if (iter->second != 0)
      iter->second->_remove_ref ();
    dw_map_.erase(iter);
  }

  if (::OpenDDS::DCPS::Transport_debug_level > 8)
    {
      ::OpenDDS::DCPS::GuidConverter converter( pub_id);
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) transport %x released publication %s.\n"),
        this,
        (const char*) converter
      ));
    }

  return ret;
}


OpenDDS::DCPS::DataWriterImpl*
OpenDDS::DCPS::TransportImpl::find_publication (OpenDDS::DCPS::RepoId pub_id, bool safe_cpy)
{
  DBG_ENTRY_LVL("TransportImpl","find_publication",6);
  GuardType guard(this->lock_);
  PublicationObjectMap::iterator iter = dw_map_.find(pub_id);
  if (iter == dw_map_.end())
    {
      if (::OpenDDS::DCPS::Transport_debug_level > 8)
        {
          ::OpenDDS::DCPS::GuidConverter converter( pub_id);
          ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("(%P|%t)TransportImpl::find_publication: ")
            ACE_TEXT("publication %s not found\n"),
            (const char*) converter
          ));
        }
      return 0;
    }
  else if (safe_cpy && iter->second != 0) {
    iter->second->_add_ref ();
  }

  return iter->second;
}


int
OpenDDS::DCPS::TransportImpl::register_subscription (OpenDDS::DCPS::RepoId sub_id,
                                                 OpenDDS::DCPS::DataReaderImpl* dr)
{
  DBG_ENTRY_LVL("TransportImpl","register_subscription",6);
  GuardType guard(this->lock_);

  int ret =
    bind(dr_map_, sub_id, dr);

  if (ret != -1)
  {
    dr->_add_ref ();
  }

  if (::OpenDDS::DCPS::Transport_debug_level > 8)
    {
      ::OpenDDS::DCPS::GuidConverter converter( sub_id);
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) TransportImpl::register_subscription: ")
        ACE_TEXT("transport %x added subscription %s.\n"),
        this,
        (const char*) converter
      ));
    }

  return ret;
}


int
OpenDDS::DCPS::TransportImpl::unregister_subscription (OpenDDS::DCPS::RepoId sub_id)
{
  DBG_ENTRY_LVL("TransportImpl","unregister_subscription",6);
  GuardType guard(this->lock_);

  SubscriptionObjectMap::iterator iter = dr_map_.find(sub_id);
  if (iter != dr_map_.end())
  {
    if (iter->second != 0)
      iter->second->_remove_ref ();
    dr_map_.erase(iter);

  } else {
    ::OpenDDS::DCPS::GuidConverter converter( sub_id);
    ACE_ERROR((LM_WARNING,
      ACE_TEXT("(%P|%t) WARNING: TransportImpl::unregister_subscription: ")
      ACE_TEXT("subscription %s not found to unregister.\n"),
      (const char*) converter
    ));
  }

  if (::OpenDDS::DCPS::Transport_debug_level > 8)
    {
      ::OpenDDS::DCPS::GuidConverter converter( sub_id);
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) TransportImpl::unregister_subscription: ")
        ACE_TEXT("transport %x released subscription %s.\n"),
        this,
        (const char*) converter
      ));
    }

  // We can't fail here - at this point the subscription is _not_
  // registered with this transport.
  return 0;
}


OpenDDS::DCPS::DataReaderImpl*
OpenDDS::DCPS::TransportImpl::find_subscription (OpenDDS::DCPS::RepoId sub_id, bool safe_cpy)
{
  DBG_ENTRY_LVL("TransportImpl","find_subscription",6);
  GuardType guard(this->lock_);
  SubscriptionObjectMap::iterator iter = dr_map_.find(sub_id);
  if (iter == dr_map_.end())
    {
      if (::OpenDDS::DCPS::Transport_debug_level > 8)
        {
          ::OpenDDS::DCPS::GuidConverter converter( sub_id);
          ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("(%P|%t) TransportImpl::find_subscription: ")
            ACE_TEXT("subscription %s not found.\n"),
            (const char*) converter
          ));
        }
      return 0;
    }
  else if (safe_cpy && iter->second != 0) {
    iter->second->_add_ref ();
  }

  return iter->second;
}


int
OpenDDS::DCPS::TransportImpl::add_pending_association (RepoId  pub_id,
                                                   size_t                  num_remote_associations,
                                                   const AssociationData*  remote_associations)
{
  DBG_ENTRY_LVL("TransportImpl","add_pending_association",6);

  GuardType guard(this->lock_);

  AssociationInfo info;
  info.num_associations_ = num_remote_associations;
  info.association_data_ = remote_associations;

  // Cache the Association data so it can be used for the callback
  // to notify datawriter on_publication_match.

  PendingAssociationsMap::iterator iter = pending_association_sub_map_.find(pub_id);
  if (iter != pending_association_sub_map_.end())
    iter->second->push_back (info);
  else {
    AssociationInfoList* infos = new AssociationInfoList;
    infos->push_back (info);
   
    if (bind(pending_association_sub_map_, pub_id, infos) == -1)
    {
      ::OpenDDS::DCPS::GuidConverter converter( pub_id);
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: TransportImpl::add_pending_association: ")
        ACE_TEXT("failed to add pending associations for pub %s\n"),
        (const char*) converter
      ),-1);
    }
  }

  // Acks for this new pending association may arrive at this time. 
  // If check for individual association, it needs remove the association 
  // from pending_association_sub_map_ so the fully_associated won't be
  // called multiple times. To simplify, check by pub id since the 
  // check_fully_association overloaded function clean the pending list 
  // after calling fully_associated.
  check_fully_association (pub_id);

  return 0;
}


int
OpenDDS::DCPS::TransportImpl::demarshal_acks (ACE_Message_Block* acks, bool byte_order)
{
  DBG_ENTRY_LVL("TransportImpl","demarshal_acks",6);

  GuardType guard(this->lock_);
  int status = this->acked_sub_map_.demarshal (acks, byte_order);
  if (status == -1)
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl::demarshal_acks failed\n"),
                      -1);

  check_fully_association ();
  return 0;
}


void OpenDDS::DCPS::TransportImpl::check_fully_association ()
{
  DBG_ENTRY_LVL("TransportImpl","check_fully_association",6);

  if (::OpenDDS::DCPS::Transport_debug_level > 8)
  {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%P|%t)ack dump: \n")));
  
    acked_sub_map_.dump ();
  }

  PendingAssociationsMap::iterator penditer =
    pending_association_sub_map_.begin();

  while(penditer != pending_association_sub_map_.end())
  {
    PendingAssociationsMap::iterator cur = penditer;
    ++ penditer;

    check_fully_association (cur->first);
  }
}

void OpenDDS::DCPS::TransportImpl::check_fully_association (const RepoId pub_id)
{
  DBG_ENTRY_LVL("TransportImpl","check_fully_association",6);

  PendingAssociationsMap::iterator penditer =
    pending_association_sub_map_.find (pub_id);

  if (penditer != pending_association_sub_map_.end())
  {
    AssociationInfoList& associations = *(penditer->second);

    AssociationInfoList::iterator iter = associations.begin ();

    while (iter != associations.end ())
    {
      if (check_fully_association (penditer->first, *iter))
      {
        iter = associations.erase (iter);
      }
      else
      {
        ++ iter;
      }
    }

    if (associations.size () == 0)
    {
      pending_association_sub_map_.erase(penditer);
    }
  }
}


bool OpenDDS::DCPS::TransportImpl::check_fully_association (const RepoId pub_id, 
                                                            AssociationInfo& associations)
{
  DBG_ENTRY_LVL("TransportImpl","check_fully_association",6);

  int num_acked = 0;

  PublicationObjectMap::iterator pubiter = dw_map_.find(pub_id);

  for (ssize_t i=0; i < associations.num_associations_; ++i)
  {
    RepoId sub_id = associations.association_data_[i].remote_id_;
    if (this->acked (pub_id, sub_id) && pubiter != dw_map_.end())
    {
      ++ num_acked;
    }
  }

  bool ret = (num_acked == associations.num_associations_);
  if (ret && pubiter != dw_map_.end())
  {
    for (ssize_t i=0; i < associations.num_associations_; ++i)
    {
      RepoId sub_id = associations.association_data_[i].remote_id_;
      this->remove_ack (pub_id, sub_id);
    }
    
    pubiter->second->fully_associated (pub_id, 
                                       associations.num_associations_,
                                       associations.association_data_);

    return true;
  }
  else if (ret && ::OpenDDS::DCPS::Transport_debug_level > 8)
  {
    std::stringstream buffer;
    buffer << " pub " << pub_id << " - sub " << associations.association_data_->remote_id_; 
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t)acked but DW is not registered:  %s \n"), buffer.str().c_str()));
  }

  return false;
}


bool
OpenDDS::DCPS::TransportImpl::acked (RepoId pub_id, RepoId sub_id)
{
  int ret = false;
  RepoIdSet_rch set = this->acked_sub_map_.find (pub_id);
  if (! set.is_nil ())
  {
    bool last = false;
    ret = set->exist (sub_id, last);
  }

  if (::OpenDDS::DCPS::Transport_debug_level > 8)
  {
    std::stringstream buffer;
    buffer << " pub " << pub_id << " - sub " << sub_id; 
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t)%s %s \n"),
      ret ? "acked" : "pending", buffer.str().c_str()));
  }
  return ret;
}


bool
OpenDDS::DCPS::TransportImpl::release_link_resources (DataLink* link)
{
  DBG_ENTRY_LVL("TransportImpl", "release_link_resources",6);

  // Create a smart pointer without ownership (bumps up ref count)
  DataLink_rch dl (link, false);

  dl_clean_task_.add (dl);

  return true;
}

void
OpenDDS::DCPS::TransportImpl::remove_ack (RepoId pub_id, RepoId sub_id)
{
  this->acked_sub_map_.remove(pub_id, sub_id);
}

OpenDDS::DCPS::AttachStatus
OpenDDS::DCPS::TransportImpl::attach (DDS::Publisher_ptr pub)
{
  OpenDDS::DCPS::PublisherImpl* pub_impl = dynamic_cast<OpenDDS::DCPS::PublisherImpl*> (pub);
  if (0 == pub)
  {
    return ATTACH_ERROR;
  }
  return pub_impl->attach_transport(this);
}

OpenDDS::DCPS::AttachStatus
OpenDDS::DCPS::TransportImpl::attach (DDS::Subscriber_ptr sub)
{
  OpenDDS::DCPS::SubscriberImpl* sub_impl = dynamic_cast<OpenDDS::DCPS::SubscriberImpl*> (sub);
  if (0 == sub)
  {
    return ATTACH_ERROR;
  }
  return sub_impl->attach_transport(this);
}
