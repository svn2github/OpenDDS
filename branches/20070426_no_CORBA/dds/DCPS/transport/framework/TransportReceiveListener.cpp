// -*- C++ -*-
//
// $Id$

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TransportReceiveListener.h"
#include "EntryExit.h"


#if !defined (__ACE_INLINE__)
#include "TransportReceiveListener.inl"
#endif /* __ACE_INLINE__ */

TAO::DCPS::TransportReceiveListener::TransportReceiveListener()
{
  DBG_ENTRY_LVL("TransportReceiveListener","TransportReceiveListener",5);
}


TAO::DCPS::TransportReceiveListener::~TransportReceiveListener()
{
  DBG_ENTRY_LVL("TransportReceiveListener","~TransportReceiveListener",5);
}

