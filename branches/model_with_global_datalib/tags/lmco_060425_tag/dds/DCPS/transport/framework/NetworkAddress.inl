// -*- C++ -*-
//
// $Id$
#include  "EntryExit.h"


ACE_INLINE
TAO::DCPS::NetworkAddress::NetworkAddress()
  : ip_(0),
    port_(0)
{
  DBG_SUB_ENTRY("NetworkAddress","NetworkAddress",1);
}


ACE_INLINE
TAO::DCPS::NetworkAddress::NetworkAddress(const ACE_INET_Addr& addr)
{
  DBG_SUB_ENTRY("NetworkAddress","NetworkAddress",2);
  this->ip_   = htonl(addr.get_ip_address());
  this->port_ = htons(addr.get_port_number());
}


ACE_INLINE
void
TAO::DCPS::NetworkAddress::to_addr(ACE_INET_Addr& addr)
{
  DBG_ENTRY("NetworkAddress","to_addr");
  addr.set(this->port_, this->ip_, 0);
}
