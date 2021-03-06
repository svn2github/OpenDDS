// -*- C++ -*-
//
// $Id$

#include "SimpleTcpTransport.h"
#include "SimpleTcpConnection.h"
#include "dds/DCPS/transport/framework/TransportSendStrategy.h"
#include "dds/DCPS/transport/framework/TransportReceiveStrategy.h"
#include "dds/DCPS/transport/framework/EntryExit.h"

ACE_INLINE const ACE_INET_Addr&
OpenDDS::DCPS::SimpleTcpDataLink::remote_address() const
{
  DBG_ENTRY_LVL("SimpleTcpDataLink","remote_address",6);
  return this->remote_address_;
}


ACE_INLINE OpenDDS::DCPS::SimpleTcpConnection_rch
OpenDDS::DCPS::SimpleTcpDataLink::get_connection ()
{
  return this->connection_;
}


ACE_INLINE OpenDDS::DCPS::SimpleTcpTransport_rch
OpenDDS::DCPS::SimpleTcpDataLink::get_transport_impl ()
{
  return this->transport_;
}

