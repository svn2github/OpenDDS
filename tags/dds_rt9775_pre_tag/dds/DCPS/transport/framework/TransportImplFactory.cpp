// -*- C++ -*-
//
// $Id$
#include  "DCPS/DdsDcps_pch.h"
#include  "TransportImplFactory.h"


#if !defined (__ACE_INLINE__)
#include "TransportImplFactory.inl"
#endif /* __ACE_INLINE__ */


TAO::DCPS::TransportImplFactory::~TransportImplFactory()
{
  DBG_ENTRY_LVL("TransportImplFactory","~TransportImplFactory",5);
}


int
TAO::DCPS::TransportImplFactory::requires_reactor() const
{
  DBG_ENTRY_LVL("TransportImplFactory","requires_reactor",5);
  // Return "false" (aka 0).
  return 0;
}

