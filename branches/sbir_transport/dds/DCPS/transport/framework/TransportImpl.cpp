/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TransportImpl.h"
#include "DataLink.h"
#include "TransportExceptions.h"
#include "dds/DCPS/BuiltInTopicUtils.h"
#include "dds/DCPS/DataWriterImpl.h"
#include "dds/DCPS/DataReaderImpl.h"
#include "dds/DCPS/PublisherImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/RepoIdConverter.h"
#include "dds/DCPS/Util.h"
#include "dds/DCPS/MonitorFactory.h"
#include "dds/DCPS/Service_Participant.h"
#include "tao/debug.h"
#include <sstream>

#if !defined (__ACE_INLINE__)
#include "TransportImpl.inl"
#endif /* __ACE_INLINE__ */

namespace {

template <typename Container>
void clear(Container& c)
{
  Container copy;
  copy.swap(c);

  for (typename Container::iterator itr = copy.begin();
       itr != copy.end();
       ++itr) {
    itr->second->_remove_ref();
  }
}

} // namespace

OpenDDS::DCPS::TransportImpl::TransportImpl()
  : monitor_(0)
{
  DBG_ENTRY_LVL("TransportImpl","TransportImpl",6);
  if (TheServiceParticipant->monitor_factory_) {
    monitor_ = TheServiceParticipant->monitor_factory_->create_transport_monitor(this);
  }
}

OpenDDS::DCPS::TransportImpl::~TransportImpl()
{
  DBG_ENTRY_LVL("TransportImpl","~TransportImpl",6);
  PendingAssociationsMap::iterator penditer =
    pending_association_sub_map_.begin();

  while (penditer != pending_association_sub_map_.end()) {
    delete penditer->second;
    ++penditer;
  }
}

void
OpenDDS::DCPS::TransportImpl::shutdown()
{
  DBG_ENTRY_LVL("TransportImpl","shutdown",6);

  // Stop datalink clean task.
  this->dl_clean_task_.close(1);

  if (!this->reactor_task_.is_nil()) {
    this->reactor_task_->stop();
  }

  this->pre_shutdown_i();

  {
    GuardType guard(this->lock_);

    if (this->config_.is_nil()) {
      // This TransportImpl is already shutdown.
//MJM: So, I read here that config_i() actually "starts" us?
      return;
    }

    for (std::set<TransportClient*>::iterator it = clients_.begin();
         it != clients_.end(); ++it) {
      (*it)->transport_detached(this);
    }

    clients_.clear();

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

int
OpenDDS::DCPS::TransportImpl::configure(TransportInst* config)
{
  DBG_ENTRY_LVL("TransportImpl","configure",6);

  GuardType guard(this->lock_);

  if (config == 0) {
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: invalid configuration.\n"),
                     -1);
  }

  if (!this->config_.is_nil()) {
    // We are rejecting this configuration attempt since this
    // TransportImpl object has already been configured.
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl already configured.\n"),
                     -1);
  }

  config->_add_ref();
  this->config_ = config;

  // Let our subclass take a shot at the configuration object.
  if (this->configure_i(config) == -1) {
    if (OpenDDS::DCPS::Transport_debug_level > 0) {
      dump();
    }

    this->config_ = 0;

    // The subclass rejected the configuration attempt.
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl configuration failed.\n"),
                     -1);
  }

  // Open the DL Cleanup task
  // We depend upon the existing config logic to ensure the
  // DL Cleanup task is opened only once
  if (this->dl_clean_task_.open()) {
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: DL Cleanup task failed to open : %p\n",
                      ACE_TEXT("open")), -1);
  }

  // Success.
  if (this->monitor_) {
    this->monitor_->report();
  }

  if (OpenDDS::DCPS::Transport_debug_level > 0) {
    std::stringstream os;
    dump(os);

    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("\n(%P|%t) TransportImpl::configure()\n%C"),
               os.str().c_str()));
  }

  return 0;
}

void
OpenDDS::DCPS::TransportImpl::create_reactor_task()
{
  if (this->reactor_task_.in()) {
    return;
  }

  this->reactor_task_ = new TransportReactorTask;
  if (0 != this->reactor_task_->open(0)) {
    throw Transport::MiscProblem(); // error already logged by TRT::open()
  }
}

OpenDDS::DCPS::DataLink*
OpenDDS::DCPS::TransportImpl::reserve_datalink(
  RepoId                  local_id,
  const AssociationData*  remote_association,
  CORBA::Long             priority,
  TransportSendListener*  send_listener)
{
  DBG_ENTRY_LVL("TransportImpl","reserve_datalink",6);

  // Ask our concrete subclass to find or create a (concrete) DataLink
  // that matches the supplied criterea.

  // Note that we pass-in true as the third argument.  This means that
  // if a new DataLink needs to be created (ie, the find operation fails),
  // then the connection establishment logic will treat the local endpoint
  // as the publisher.  This knowledge dictates whether a passive or active
  // connection establishment procedure should be followed.
  DataLink_rch link =
    this->find_or_create_datalink(local_id,
                                  remote_association,
                                  priority,
                                  true);

  if (link.is_nil()) {
    OpenDDS::DCPS::RepoIdConverter pub_converter(local_id);
    OpenDDS::DCPS::RepoIdConverter sub_converter(remote_association->remote_id_);
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: TransportImpl::reserve_datalink: ")
                      ACE_TEXT("subclass was unable to find ")
                      ACE_TEXT("or create a DataLink for local publisher_id %C ")
                      ACE_TEXT("to remote subscriber_id %C.\n"),
                      std::string(pub_converter).c_str(),
                      std::string(sub_converter).c_str()),0);
  }

  link->make_reservation(remote_association->remote_id_,  // subscription_id
                         local_id,                        // publication_id
                         send_listener);

  return link._retn();
}

OpenDDS::DCPS::DataLink*
OpenDDS::DCPS::TransportImpl::reserve_datalink(
  RepoId                    local_id,
  const AssociationData*    remote_association,
  CORBA::Long               priority,
  TransportReceiveListener* receive_listener)
{
  DBG_ENTRY_LVL("TransportImpl","reserve_datalink",6);

  // Ask our concrete subclass to find or create a DataLink (actually, a
  // concrete subclass of DataLink) that matches the supplied criterea.
  // Since find_or_create() is pure virtual, the concrete subclass must
  // provide an implementation for us to use.

  // Note that we pass-in false as the third argument.  This means that
  // if a new DataLink needs to be created (ie, the find operation fails),
  // then the connection establishment logic will treat the local endpoint
  // as a subscriber.  This knowledge dictates whether a passive or active
  // connection establishment procedure should be followed.
  DataLink_rch link =
    this->find_or_create_datalink(local_id,
                                  remote_association,
                                  priority,
                                  false);

  if (link.is_nil()) {
    OpenDDS::DCPS::RepoIdConverter pub_converter(remote_association->remote_id_);
    OpenDDS::DCPS::RepoIdConverter sub_converter(local_id);
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: TransportImpl::reserve_datalink: ")
                      ACE_TEXT("subclass was unable to find ")
                      ACE_TEXT("or create a DataLink for local subscriber_id %C ")
                      ACE_TEXT("to remote publisher_id %C.\n"),
                      std::string(sub_converter).c_str(),
                      std::string(pub_converter).c_str()),0);
  }

  link->make_reservation(remote_association->remote_id_,  // publication_id
                         local_id,                        // subscription_id
                         receive_listener);

  // This is called on the subscriber side to let the concrete
  // datalink to do some necessary work such as Tcp will
  // send the FULLY_ASSOCIATED ack to the publisher.
  link->fully_associated();

  return link._retn();
}

void
OpenDDS::DCPS::TransportImpl::attach_client(TransportClient* client)
{
  DBG_ENTRY_LVL("TransportImpl", "attach_client", 6);

  GuardType guard(this->lock_);
  clients_.insert(client);
}

void
OpenDDS::DCPS::TransportImpl::detach_client(TransportClient* client)
{
  DBG_ENTRY_LVL("TransportImpl", "attach_client", 6);

  GuardType guard(this->lock_);
  clients_.erase(client);
}

int
OpenDDS::DCPS::TransportImpl::add_pending_association(
  RepoId                  local_id,
  const AssociationInfo&  info,
  TransportSendListener*  tsl)
{
  DBG_ENTRY_LVL("TransportImpl","add_pending_association",6);

  GuardType guard(this->lock_);

  // Cache the Association data so it can be used for the callback
  // to notify datawriter on_publication_matched.

  PendingAssociationsMap::iterator iter = pending_association_sub_map_.find(local_id);

  if (iter != pending_association_sub_map_.end())
    iter->second->push_back(info);

  else {
    AssociationInfoList* infos = new AssociationInfoList;
    infos->push_back(info);

    association_listeners_[local_id] = tsl;

    if (OpenDDS::DCPS::bind(pending_association_sub_map_, local_id, infos) == -1) {
      OpenDDS::DCPS::RepoIdConverter converter(local_id);
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: TransportImpl::add_pending_association: ")
                        ACE_TEXT("failed to add pending associations for pub %C\n"),
                        std::string(converter).c_str()),-1);
    }
  }

  // Acks for this new pending association may arrive at this time.
  // If check for individual association, it needs remove the association
  // from pending_association_sub_map_ so the fully_associated won't be
  // called multiple times. To simplify, check by pub id since the
  // check_fully_association overloaded function clean the pending list
  // after calling fully_associated.
  check_fully_association(local_id);

  return 0;
}

int
OpenDDS::DCPS::TransportImpl::demarshal_acks(ACE_Message_Block* acks, bool byte_order)
{
  DBG_ENTRY_LVL("TransportImpl","demarshal_acks",6);

  {
  GuardType guard(this->lock_);

  int status = this->acked_sub_map_.demarshal(acks, byte_order);

  if (status == -1)
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl::demarshal_acks failed\n"),
                     -1);
  }

  check_fully_association();
  return 0;
}

void OpenDDS::DCPS::TransportImpl::check_fully_association()
{
  DBG_ENTRY_LVL("TransportImpl","check_fully_association",6);

  GuardType guard(this->lock_);

  if (OpenDDS::DCPS::Transport_debug_level > 8) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) ack dump: \n")));

    acked_sub_map_.dump();
  }

  PendingAssociationsMap::iterator penditer =
    pending_association_sub_map_.begin();

  while (penditer != pending_association_sub_map_.end()) {
    PendingAssociationsMap::iterator cur = penditer;
    ++ penditer;

    check_fully_association(cur->first);
  }
}

void OpenDDS::DCPS::TransportImpl::check_fully_association(const RepoId pub_id)
{
  DBG_ENTRY_LVL("TransportImpl","check_fully_association",6);

  PendingAssociationsMap::iterator penditer =
    pending_association_sub_map_.find(pub_id);

  if (penditer != pending_association_sub_map_.end()) {
    AssociationInfoList& associations = *(penditer->second);

    AssociationInfoList::iterator iter = associations.begin();

    while (iter != associations.end()) {
      if (check_fully_association(penditer->first, *iter)) {
        iter = associations.erase(iter);
        association_listeners_.erase(pub_id);

      } else {
        ++ iter;
      }
    }

    if (associations.size() == 0) {
      delete penditer->second;
      pending_association_sub_map_.erase(penditer);
    }
  }
}

bool OpenDDS::DCPS::TransportImpl::check_fully_association(const RepoId pub_id,
                                                           AssociationInfo& associations)
{
  DBG_ENTRY_LVL("TransportImpl","check_fully_association",6);

  size_t num_acked = 0;

  TransportSendListener* tsl = association_listeners_[pub_id];

  for (size_t i = 0; i < associations.num_associations_; ++i) {
    RepoId sub_id = associations.association_data_[i].remote_id_;

    if (this->acked(pub_id, sub_id) && tsl) {
      ++num_acked;
    }
  }

  bool ret = (num_acked == associations.num_associations_);

  if (ret && tsl) {
    for (size_t i = 0; i < associations.num_associations_; ++i) {
      RepoId sub_id = associations.association_data_[i].remote_id_;
      this->remove_ack(pub_id, sub_id);
    }

    tsl->fully_associated(associations.num_associations_,
                          associations.association_data_);

    return true;

  } else if (ret && OpenDDS::DCPS::Transport_debug_level > 8) {
    std::stringstream buffer;
    buffer << " pub " << pub_id << " - sub " << associations.association_data_->remote_id_;
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) acked but DW is not registered:  %C \n"), buffer.str().c_str()));
  }

  return false;
}

bool
OpenDDS::DCPS::TransportImpl::acked(RepoId pub_id, RepoId sub_id)
{
  int ret = false;
  RepoIdSet_rch set = this->acked_sub_map_.find(pub_id);

  if (!set.is_nil()) {
    bool last = false;
    ret = set->exist(sub_id, last);
  }

  if (OpenDDS::DCPS::Transport_debug_level > 8) {
    std::stringstream buffer;
    buffer << " pub " << pub_id << " - sub " << sub_id;
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) %C %C \n"),
               ret ? "acked" : "pending", buffer.str().c_str()));
  }

  return ret;
}

bool
OpenDDS::DCPS::TransportImpl::release_link_resources(DataLink* link)
{
  DBG_ENTRY_LVL("TransportImpl", "release_link_resources",6);

  // Create a smart pointer without ownership (bumps up ref count)
  DataLink_rch dl(link, false);

  dl_clean_task_.add(dl);

  return true;
}

void
OpenDDS::DCPS::TransportImpl::remove_ack(RepoId pub_id, RepoId sub_id)
{
  this->acked_sub_map_.remove(pub_id, sub_id);
}

const OpenDDS::DCPS::FactoryIdType&
OpenDDS::DCPS::TransportImpl::get_factory_id()
{
  return this->factory_id_;
}

void
OpenDDS::DCPS::TransportImpl::set_factory_id(const FactoryIdType& fid)
{
  this->factory_id_ = fid;
}

void
OpenDDS::DCPS::TransportImpl::report()
{
  if (this->monitor_) {
    this->monitor_->report();
  }
}

void
OpenDDS::DCPS::TransportImpl::dump()
{
  std::stringstream os;
  dump(os);

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("\n(%P|%t) TransportImpl::dump() -\n%C"),
             os.str().c_str()));
}

void
OpenDDS::DCPS::TransportImpl::dump(ostream& os)
{
  os << TransportInst::formatNameForDump("name")
     << config()->name();

  if (this->config_.is_nil()) {
    os << " (not configured)" << std::endl;
  } else {
    os << std::endl;
    this->config_->dump(os);
  }
}


/// Note that this will return -1 if the TransportImpl has not been
/// configure()'d yet.
int
OpenDDS::DCPS::TransportImpl::swap_bytes() const
{
  DBG_ENTRY_LVL("TransportImpl","swap_bytes",6);

  GuardType guard(this->lock_);

  if (this->config_.is_nil()) {
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl cannot return swap_bytes "
                      "value - TransportImpl has not been configure()'d.\n"),
                     -1);
  }

  return this->config_->swap_bytes_;
}
