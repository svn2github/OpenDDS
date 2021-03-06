// -*- C++ -*-
//
// $Id$

#include "dds/DCPS/transport/framework/TransportImpl.h"
#include "dds/DCPS/transport/framework/EntryExit.h"


ACE_INLINE
OpenDDS::DCPS::SimpleUnreliableDgramDataLink::SimpleUnreliableDgramDataLink
                                        (const ACE_INET_Addr& remote_address,
                                         TransportImpl* transport_impl)
  : DataLink(transport_impl),
    remote_address_(remote_address),
    priority_( 0) // Default TRANSPORT_PRIORITY.value is 0.
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramDataLink","SimpleUnreliableDgramDataLink",6);
}


ACE_INLINE const ACE_INET_Addr&
OpenDDS::DCPS::SimpleUnreliableDgramDataLink::remote_address() const
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramDataLink","remote_address",6);
  return this->remote_address_;
}


ACE_INLINE int
OpenDDS::DCPS::SimpleUnreliableDgramDataLink::connect(TransportSendStrategy* send_strategy)
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramDataLink","connect",6);
  return this->start(send_strategy,0);
}

ACE_INLINE
CORBA::Long&
OpenDDS::DCPS::SimpleUnreliableDgramDataLink::priority()
{
  return this->priority_;
}

ACE_INLINE
CORBA::Long
OpenDDS::DCPS::SimpleUnreliableDgramDataLink::priority() const
{
  return this->priority_;
}

