/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "MulticastSession.h"

#include "ace/Log_Msg.h"
#include <cmath>
#include "dds/DCPS/async_debug.h"
#ifndef __ACE_INLINE__
# include "MulticastSession.inl"
#endif  /* __ACE_INLINE__ */

namespace OpenDDS {
namespace DCPS {

SynWatchdog::SynWatchdog(MulticastSession* session)
  : session_(session)
  , retries_(0)
{
}

ACE_Time_Value
SynWatchdog::next_interval()
{
  MulticastInst* config = this->session_->link()->config();
  ACE_Time_Value interval(config->syn_interval_);

  // Apply exponential backoff based on number of retries:
  if (this->retries_ > 0) {
    interval *= std::pow(config->syn_backoff_, double(this->retries_));
  }
  ++this->retries_;

  return interval;
}

void
SynWatchdog::on_interval(const void* /*arg*/)
{
  // Initiate handshake by sending a MULTICAST_SYN control
  // sample to the assigned remote peer:
  this->session_->send_syn();
}

ACE_Time_Value
SynWatchdog::next_timeout()
{
  MulticastInst* config = this->session_->link()->config();
  return config->syn_timeout_;
}

void
SynWatchdog::on_timeout(const void* /*arg*/)
{
  // There is no recourse if a link is unable to handshake;
  // log an error and return:
  ACE_ERROR((LM_WARNING,
             ACE_TEXT("(%P|%t) WARNING: ")
             ACE_TEXT("SynWatchdog[transport=%C]::on_timeout: ")
             ACE_TEXT("timed out waiting on remote peer: 0x%x local: 0x%x\n"),
             this->session_->link()->config()->name().c_str(),
             this->session_->remote_peer(),
             this->session_->link()->local_peer()));
}


MulticastSession::MulticastSession(MulticastDataLink* link,
                                   MulticastPeer remote_peer)
  : link_(link)
  , remote_peer_(remote_peer)
  , reverse_start_lock_(start_lock_)
  , started_(false)
  , active_(true)
  , acked_(false)
  , ack_cond_(ack_lock_)
  , syn_watchdog_(this)
{
}

MulticastSession::~MulticastSession()
{
}

bool
MulticastSession::acked()
{
  ACE_GUARD_RETURN(ACE_SYNCH_MUTEX, guard, this->ack_lock_, false);
  return this->acked_;
}

bool
MulticastSession::wait_for_ack()
{
  ACE_Time_Value abs_timeout = ACE_OS::gettimeofday() +
    this->link_->config()->syn_timeout_;
  ACE_GUARD_RETURN(ACE_SYNCH_MUTEX, guard, this->ack_lock_, false);
  while (!this->acked_) {
    if (this->ack_cond_.wait(&abs_timeout) == -1) {
      return false;
    }
  }
  return true;
}

bool
MulticastSession::start_syn(ACE_Reactor* reactor)
{
  return this->syn_watchdog_.schedule_now(reactor);
}

void
MulticastSession::send_control(char submessage_id, ACE_Message_Block* data)
{
  DataSampleHeader header;
  ACE_Message_Block* control =
    this->link_->create_control(submessage_id, header, data);
  if (control == 0) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("MulticastSession::send_control: ")
               ACE_TEXT("create_control failed!\n")));
    return;
  }

  int error = this->link_->send_control(header, control);
  if (error != SEND_CONTROL_OK) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("MulticastSession::send_control: ")
               ACE_TEXT("send_control failed: %d!\n"),
               error));
    return;
  }
}

bool
MulticastSession::control_received(char submessage_id,
                                   ACE_Message_Block* control)
{
  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastDataLink::control_received -> enter SESSION: %@ \n", this));


  // Record that we've gotten this message so we don't nak for it later.
  if (!this->acked()) {
    const TransportHeader& header =
      this->link_->receive_strategy()->received_header();
    if (this->remote_peer_ == header.source_) {
      check_header(header);
    }
  }

  switch (submessage_id) {
  case MULTICAST_SYN:
    syn_received(control);
    break;

  case MULTICAST_SYNACK:
    synack_received(control);
    break;

  default:
    return false;
  }

  return true;
}

void
MulticastSession::syn_received(ACE_Message_Block* control)
{
   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::syn_received --> enter\n"));

  if (this->active_) return; // pub send syn, then doesn't receive them.

  const TransportHeader& header =
    this->link_->receive_strategy()->received_header();

  // Not from the remote peer for this session.
  if (this->remote_peer_ != header.source_) return;

  Serializer serializer(control, header.swap_bytes());

  MulticastPeer local_peer;
  serializer >> local_peer; // sent as remote_peer

  // Ignore sample if not destined for us:
  if (local_peer != this->link_->local_peer()) return;

  VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastSession[%C]::syn_received "
                    "local 0x%x remote 0x%x\n",
                    this->link()->config()->name().c_str(),
                    this->link()->local_peer(), this->remote_peer_), 2);

  {
    ACE_GUARD(ACE_SYNCH_MUTEX, guard, this->ack_lock_);

    if (!this->acked_) {
      this->acked_ = true;
      this->ack_cond_.broadcast();

      syn_hook(header.sequence_);
    }
  }

  //### Debug statements to track where associate is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::syn_received --> about to send_synack\n"));

  // MULTICAST_SYN control samples are always positively
  // acknowledged by a matching remote peer:
  send_synack();

  //### Debug statements to track where associate is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::syn_received --> call passive_connection\n"));

  this->link_->transport()->passive_connection(this->link_->local_peer(), this->remote_peer_);

  //### Debug statements to track where associate is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::syn_received --> exit\n"));
}

void
MulticastSession::send_syn()
{
  size_t len = sizeof(this->remote_peer_);

  ACE_Message_Block* data;
  ACE_NEW(data, ACE_Message_Block(len));

  Serializer serializer(data);

  serializer << this->remote_peer_;

  VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastSession[%C]::send_syn "
                      "local 0x%x remote 0x%x\n",
                      this->link()->config()->name().c_str(),
                      this->link()->local_peer(), this->remote_peer_), 2);

  // Send control sample to remote peer:
  send_control(MULTICAST_SYN, data);
}

void
MulticastSession::synack_received(ACE_Message_Block* control)
{
  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> enter (SESSION:%@) \n", this));

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> check if active %d \n", this->active_));

  if (!this->active_) return; // sub send syn, then doesn't receive them.

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> check if acked %d \n", this->acked()));

  // Already received ack.
  if (this->acked()) return;

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> get transport header \n"));

  const TransportHeader& header =
    this->link_->receive_strategy()->received_header();

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> check if from remote peer for session \n"));

  // Not from the remote peer for this session.
  if (this->remote_peer_ != header.source_) return;

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> from correct peer for session \n"));

  Serializer serializer(control, header.swap_bytes());

  MulticastPeer local_peer;
  serializer >> local_peer; // sent as remote_peer

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> check if we should ignore sample \n"));

  // Ignore sample if not destined for us:
  if (local_peer != this->link_->local_peer()) return;

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> did not ignore sample\n"));

  VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastSession[%C]::synack_received "
                      "local 0x%x remote 0x%x\n",
                      this->link()->config()->name().c_str(),
                      this->link()->local_peer(), this->remote_peer_), 2);

  {
    //### Debug statements to track where connection is failing
    if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> trying to LOCK ack_lock_ \n"));
    ACE_GUARD(ACE_SYNCH_MUTEX, guard, this->ack_lock_);

    //### Debug statements to track where connection is failing
    if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> LOCKED ack_lock_ \n"));

    //### Debug statements to track where connection is failing
    if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> if already acked (%d) return \n", this->acked_));

    if (this->acked_) return; // already acked

    //### Debug statements to track where connection is failing
    if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> wasn't acked so cancel watchdog \n"));

    this->syn_watchdog_.cancel();
    //### Debug statements to track where connection is failing
    if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> set acked_ to true \n"));
    this->acked_ = true;
    //### Debug statements to track where connection is failing
    if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> broadcast ack_cond_ \n"));
    this->ack_cond_.broadcast();
    //### Debug statements to track where connection is failing
    if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:MulticastSession::synack_received -> RELEASING ack_lock_ \n"));
  }
}

void
MulticastSession::send_synack()
{
  // Send nakack before sending synack to
  // reduce naks from remote.
  SingleSendBuffer* send_buffer = this->link_->send_buffer();
  if (send_buffer && !send_buffer->empty()
      && send_buffer->low() > ++SequenceNumber()) {
    send_nakack(send_buffer->low());
  }

  size_t len = sizeof(this->remote_peer_);

  ACE_Message_Block* data;
  ACE_NEW(data, ACE_Message_Block(len));

  Serializer serializer(data);

  serializer << this->remote_peer_;

  VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastSession[%C]::send_synack "
                      "local 0x%x remote 0x%x active %d\n",
                      this->link()->config()->name().c_str(),
                      this->link()->local_peer(), this->remote_peer_,
                      this->active_ ? 1 : 0), 2);

  // Send control sample to remote peer:
  send_control(MULTICAST_SYNACK, data);
}

void
MulticastSession::stop()
{
  this->syn_watchdog_.cancel();
}

bool
MulticastSession::reassemble(ReceivedDataSample& data,
                             const TransportHeader& header)
{
  return this->reassembly_.reassemble(header.sequence_,
                                      header.first_fragment_,
                                      data);
}

} // namespace DCPS
} // namespace OpenDDS
