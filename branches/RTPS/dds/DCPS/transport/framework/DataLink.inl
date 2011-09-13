/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "TransportSendStrategy.h"
#include "TransportStrategy.h"
#include "ThreadPerConnectionSendTask.h"
#include "EntryExit.h"

ACE_INLINE
CORBA::Long&
OpenDDS::DCPS::DataLink::transport_priority()
{
  return this->transport_priority_;
}

ACE_INLINE
CORBA::Long
OpenDDS::DCPS::DataLink::transport_priority() const
{
  return this->transport_priority_;
}


ACE_INLINE
bool& OpenDDS::DCPS::DataLink::is_loopback()
{
  return this->is_loopback_;
}


ACE_INLINE
bool  OpenDDS::DCPS::DataLink::is_loopback() const
{
  return this->is_loopback_;
}


ACE_INLINE
bool& OpenDDS::DCPS::DataLink::is_active()
{
  return this->is_active_;
}


ACE_INLINE
bool  OpenDDS::DCPS::DataLink::is_active() const
{
  return this->is_active_;
}


ACE_INLINE void
OpenDDS::DCPS::DataLink::send_start()
{
  DBG_ENTRY_LVL("DataLink","send_start",6);

  if (this->thr_per_con_send_task_ != 0) {
    this->thr_per_con_send_task_->add_request(SEND_START);

  } else
    this->send_start_i();
}

ACE_INLINE void
OpenDDS::DCPS::DataLink::send_start_i()
{
  DBG_ENTRY_LVL("DataLink","send_start_i",6);
  // This one is easy.  Simply delegate to our TransportSendStrategy
  // data member.

  TransportSendStrategy_rch strategy;
  {
    GuardType guard(this->strategy_lock_);

    strategy = this->send_strategy_;
  }

  if (!strategy.is_nil()) {
    strategy->send_start();
  }
}

ACE_INLINE void
OpenDDS::DCPS::DataLink::send(TransportQueueElement* element)
{
  DBG_ENTRY_LVL("DataLink","send",6);

  if (this->thr_per_con_send_task_ != 0) {
    this->thr_per_con_send_task_->add_request(SEND, element);

  } else {
    this->send_i(element);

  }
}

ACE_INLINE void
OpenDDS::DCPS::DataLink::send_i(TransportQueueElement* element, bool relink)
{
  DBG_ENTRY_LVL("DataLink","send_i",6);
  // This one is easy.  Simply delegate to our TransportSendStrategy
  // data member.

  TransportSendStrategy_rch strategy;
  {
    GuardType guard(this->strategy_lock_);

    strategy = this->send_strategy_;
  }

  if (!strategy.is_nil()) {
    strategy->send(element, relink);
  }
}

ACE_INLINE void
OpenDDS::DCPS::DataLink::send_stop()
{
  DBG_ENTRY_LVL("DataLink","send_stop",6);

  if (this->thr_per_con_send_task_ != 0) {
    this->thr_per_con_send_task_->add_request(SEND_STOP);

  } else
    this->send_stop_i();
}

ACE_INLINE void
OpenDDS::DCPS::DataLink::send_stop_i()
{
  DBG_ENTRY_LVL("DataLink","send_stop_i",6);
  // This one is easy.  Simply delegate to our TransportSendStrategy
  // data member.

  TransportSendStrategy_rch strategy = 0;
  {
    GuardType guard(this->strategy_lock_);

    strategy = this->send_strategy_;
  }

  if (!strategy.is_nil()) {
    strategy->send_stop();
  }
}

ACE_INLINE int
OpenDDS::DCPS::DataLink::remove_sample(TransportSendElement& element,
                                       bool dropped_by_transport)
{
  DBG_ENTRY_LVL("DataLink","remove_sample",6);
  ACE_UNUSED_ARG(dropped_by_transport);

  // This one is easy.  Simply delegate to our TransportSendStrategy
  // data member.

  TransportSendStrategy_rch strategy;
  {
    GuardType guard(this->strategy_lock_);

    strategy = this->send_strategy_;
  }

  int status = -1;

  // Remove the sample from thread per connection queue and then
  // delegate to send strategy.
  if (this->thr_per_con_send_task_ != 0) {
    status = this->thr_per_con_send_task_->remove_sample(element);
  }

  if (status == 1) {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
          "Removed sample from ThreadPerConnection queue.\n"));

  } else if (!strategy.is_nil()) // not exist on thread per connectio queue.
    return strategy->remove_sample(element);

  return 0;
}

ACE_INLINE void
OpenDDS::DCPS::DataLink::remove_all_msgs(RepoId pub_id)
{
  DBG_ENTRY_LVL("DataLink","remove_all_msgs",6);

  // This one is easy.  Simply delegate to our TransportSendStrategy
  // data member.

  TransportSendStrategy_rch strategy;
  {
    GuardType guard(this->strategy_lock_);

    strategy = this->send_strategy_;
  }

  if (!strategy.is_nil()) {
    strategy->remove_all_msgs(pub_id);
  }
}

ACE_INLINE OpenDDS::DCPS::DataLinkIdType
OpenDDS::DCPS::DataLink::id() const
{
  DBG_ENTRY_LVL("DataLink","id",6);
  return id_;
}

ACE_INLINE int
OpenDDS::DCPS::DataLink::start(const TransportSendStrategy_rch& send_strategy,
                               const TransportStrategy_rch& receive_strategy)
{
  DBG_ENTRY_LVL("DataLink","start",6);

  // We assume that the send_strategy is not NULL, but the receive_strategy
  // is allowed to be NULL.

  // Attempt to start the strategies, and if there is a start() failure,
  // make sure to stop() any strategy that was already start()'ed.
  if (send_strategy->start() != 0) {
    // Failed to start the TransportSendStrategy.
    return -1;
  }

  if ((!receive_strategy.is_nil()) && (receive_strategy->start() != 0)) {
    // Failed to start the TransportReceiveStrategy.

    // Remember to stop() the TransportSendStrategy since we did start it,
    // and now need to "undo" that action.
    send_strategy->stop();

    return -1;
  }

  // We started both strategy objects.  Save them to data members since
  // we will now take ownership of them.
  {
    GuardType guard(this->strategy_lock_);

    this->send_strategy_    = send_strategy;
    this->receive_strategy_ = receive_strategy;

    this->strategy_condition_.broadcast();
  }
  return 0;
}

ACE_INLINE
void
OpenDDS::DCPS::DataLink::unblock_wait_for_start()
{
  GuardType guard(this->strategy_lock_);
  this->start_failed_ = true;
  this->strategy_condition_.broadcast();
}

ACE_INLINE
const char*
OpenDDS::DCPS::DataLink::connection_notice_as_str(ConnectionNotice notice)
{
  static const char* NoticeStr[] = { "DISCONNECTED",
                                     "RECONNECTED",
                                     "LOST"
                                   };

  return NoticeStr [notice];
}

ACE_INLINE
void
OpenDDS::DCPS::DataLink::terminate_send()
{
  this->send_strategy_->terminate_send(false);
}

ACE_INLINE
void
OpenDDS::DCPS::DataLink::remove_listener(const OpenDDS::DCPS::RepoId& local_id)
{
  {
    GuardType guard(this->pub_map_lock_);
    if (this->send_listeners_.erase(local_id)) {
      return;
    }
  }
  GuardType guard(this->sub_map_lock_);
  this->recv_listeners_.erase(local_id);
}

ACE_INLINE
OpenDDS::DCPS::TransportSendListener*
OpenDDS::DCPS::DataLink::send_listener_for(const RepoId& pub_id) const
{
  // pub_map_ (and send_listeners_) are already locked when entering this
  // private method.
  IdToSendListenerMap::const_iterator found =
    this->send_listeners_.find(pub_id);
  if (found == this->send_listeners_.end()) {
    return 0;
  }
  return found->second;
}

ACE_INLINE
OpenDDS::DCPS::TransportReceiveListener*
OpenDDS::DCPS::DataLink::recv_listener_for(const RepoId& sub_id) const
{
  // sub_map_ (and recv_listeners_) are already locked when entering this
  // private method.
  IdToRecvListenerMap::const_iterator found =
    this->recv_listeners_.find(sub_id);
  if (found == this->recv_listeners_.end()) {
    return 0;
  }
  return found->second;
}
