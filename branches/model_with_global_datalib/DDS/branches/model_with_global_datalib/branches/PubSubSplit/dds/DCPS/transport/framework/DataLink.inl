// -*- C++ -*-
//
// $Id$

#include "TransportSendStrategy.h"
#include "TransportReceiveStrategy.h"
#include "ThreadPerConnectionSendTask.h"
#include "EntryExit.h"


ACE_INLINE void
TAO::DCPS::DataLink::send_start()
{
  DBG_ENTRY_LVL("DataLink","send_start",5);

  if (this->thr_per_con_send_task_ != 0)
  {
    this->thr_per_con_send_task_->add_request (SEND_START);
  }
  else
    this->send_start_i ();
}


ACE_INLINE void
TAO::DCPS::DataLink::send_start_i()
{
  DBG_ENTRY_LVL("DataLink","send_start_i",5);
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


//MJM: Here is where inlining will become critical.  This can be
//MJM: compiled completely away (if its not virtual, of course).
ACE_INLINE void
TAO::DCPS::DataLink::send(TransportQueueElement* element)
{
  DBG_ENTRY_LVL("DataLink","send",5);

  if (this->thr_per_con_send_task_ != 0)
  {
    this->thr_per_con_send_task_->add_request (SEND, element);
  }
  else
    this->send_i (element);
}


ACE_INLINE void
TAO::DCPS::DataLink::send_i(TransportQueueElement* element, bool relink)
{
  DBG_ENTRY_LVL("DataLink","send_i",5);
  // This one is easy.  Simply delegate to our TransportSendStrategy
  // data member.

  TransportSendStrategy_rch strategy;
  {
    GuardType guard(this->strategy_lock_);

    strategy = this->send_strategy_;
  }

  if (!strategy.is_nil()) {
    strategy->send (element, relink);
  }
}


ACE_INLINE void
TAO::DCPS::DataLink::send_stop()
{
  DBG_ENTRY_LVL("DataLink","send_stop",5);

  if (this->thr_per_con_send_task_ != 0)
  {
    this->thr_per_con_send_task_->add_request (SEND_STOP);
  }
  else
    this->send_stop_i ();
}


ACE_INLINE void
TAO::DCPS::DataLink::send_stop_i()
{
  DBG_ENTRY_LVL("DataLink","send_stop_i",5);
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
TAO::DCPS::DataLink::remove_sample(const DataSampleListElement* sample,
                                   bool dropped_by_transport)
{
  DBG_ENTRY_LVL("DataLink","remove_sample",5);
  ACE_UNUSED_ARG (dropped_by_transport);

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
   if (this->thr_per_con_send_task_ != 0)
   {
     status = this->thr_per_con_send_task_->remove_sample (sample);
   }

   if (status == 1)
   {
     VDBG((LM_DEBUG, "(%P|%t) DBG:   "
       "Removed sample from ThreadPerConnection queue.\n"));
   }
   else if (!strategy.is_nil()) // not exist on thread per connectio queue.
      return strategy->remove_sample(sample);

  return 0;
}


ACE_INLINE void
TAO::DCPS::DataLink::remove_all_control_msgs(RepoId pub_id)
{
  DBG_ENTRY_LVL("DataLink","remove_all_control_msgs",5);

  // This one is easy.  Simply delegate to our TransportSendStrategy
  // data member.

  TransportSendStrategy_rch strategy;
  {
    GuardType guard(this->strategy_lock_);

    strategy = this->send_strategy_;
  }

  if (!strategy.is_nil()) {
    strategy->remove_all_control_msgs(pub_id);
  }
}


/// We use our "this" pointer for our id.  Note that the "this" pointer
/// is a DataLink* as far as we are concerned.  This *is* different (due
/// to virtual tables) than the "this" pointer when in a DataLink subclass.
/// But since this is the only place where a DataLink provides its "id",
/// this should all work out (comparing ids for equality/inequality).
ACE_INLINE TAO::DCPS::DataLinkIdType
TAO::DCPS::DataLink::id() const
{
  DBG_ENTRY_LVL("DataLink","id",5);
  return id_;
}


ACE_INLINE int
TAO::DCPS::DataLink::start(TransportSendStrategy*    send_strategy,
                           TransportReceiveStrategy* receive_strategy)
{
  DBG_ENTRY_LVL("DataLink","start",5);

  // We assume that the send_strategy is not NULL, but the receive_strategy
  // is allowed to be NULL.

  // Keep a "copy" of the send_strategy.
  send_strategy->_add_ref(); //ciju: why do this? Cause the ptr was from a .in() and not a .retrn()
  TransportSendStrategy_rch ss = send_strategy;

  // Keep a "copy" of the receive_strategy (if there is one).
  TransportReceiveStrategy_rch rs;

  if (receive_strategy != 0)
    {
      receive_strategy->_add_ref(); //ciju: Why do this? Same reason as above
      rs = receive_strategy;
    }

  // Attempt to start the strategies, and if there is a start() failure,
  // make sure to stop() any strategy that was already start()'ed.
  if (ss->start() != 0)
    {
      // Failed to start the TransportSendStrategy.
      return -1;
    }

  if ((!rs.is_nil()) && (rs->start() != 0))
    {
      // Failed to start the TransportReceiveStrategy.

      // Remember to stop() the TransportSendStrategy since we did start it,
      // and now need to "undo" that action.
      ss->stop();

      return -1;
    }

  // We started both strategy objects.  Save them to data members since
  // we will now take ownership of them.
  {
    GuardType guard(this->strategy_lock_);

    this->send_strategy_    = ss._retn();
    this->receive_strategy_ = rs._retn();
  }
  return 0;
}


ACE_INLINE
const char*
TAO::DCPS::DataLink::connection_notice_as_str (enum ConnectionNotice notice)
{
  static const char* NoticeStr[] = { "DISCONNECTED",
                                     "RECONNECTED",
                                     "LOST"
                                   };

  return NoticeStr [notice];
}


ACE_INLINE
void
TAO::DCPS::DataLink::fully_associated ()
{
  //noop
}


ACE_INLINE
void 
TAO::DCPS::DataLink::terminate_send ()
{
  this->send_strategy_->terminate_send (false);
}


