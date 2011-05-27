// -*- C++ -*-
//
// $Id$
#include "SimpleUnreliableDgram_pch.h"
#include "SimpleUnreliableDgramReceiveStrategy.h"
#include "dds/DCPS/transport/framework/EntryExit.h"

#if !defined (__ACE_INLINE__)
#include "SimpleUnreliableDgramReceiveStrategy.inl"
#endif /* __ACE_INLINE__ */


OpenDDS::DCPS::SimpleUnreliableDgramReceiveStrategy::~SimpleUnreliableDgramReceiveStrategy()
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramReceiveStrategy","~SimpleUnreliableDgramReceiveStrategy",6);
}


ssize_t
OpenDDS::DCPS::SimpleUnreliableDgramReceiveStrategy::receive_bytes(iovec          iov[],
                                                   int            n,
                                                   ACE_INET_Addr& remote_addr)
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramReceiveStrategy","receive_bytes",6);
  return this->socket_->receive_bytes(iov, n, remote_addr);
}


int
OpenDDS::DCPS::SimpleUnreliableDgramReceiveStrategy::start_i()
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramReceiveStrategy","start_i",6);
  return this->socket_->set_receive_strategy(this,this->task_.in());
}


void
OpenDDS::DCPS::SimpleUnreliableDgramReceiveStrategy::stop_i()
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramReceiveStrategy","stop_i",6);

  this->socket_->remove_receive_strategy();

  this->task_ = 0;
  this->socket_ = 0;
  this->transport_ = 0;
}


void
OpenDDS::DCPS::SimpleUnreliableDgramReceiveStrategy::deliver_sample
                                        (ReceivedDataSample&  sample,
                                         const ACE_INET_Addr& remote_address)
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramReceiveStrategy","deliver_sample",6);

  // Receive side does not honor the TRANSPORT_PRIORITY policy, assume
  // default 0 for reception.
  this->transport_->deliver_sample(sample, remote_address, 0);
}

