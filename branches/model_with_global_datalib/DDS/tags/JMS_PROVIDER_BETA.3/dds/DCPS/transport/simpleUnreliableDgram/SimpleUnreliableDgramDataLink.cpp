// -*- C++ -*-
//
// $Id$

#include "SimpleUnreliableDgram_pch.h"
#include "SimpleUnreliableDgramDataLink.h"


#if !defined (__ACE_INLINE__)
#include "SimpleUnreliableDgramDataLink.inl"
#endif /* __ACE_INLINE__ */


OpenDDS::DCPS::SimpleUnreliableDgramDataLink::~SimpleUnreliableDgramDataLink()
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramDataLink","~SimpleUnreliableDgramDataLink",6);
}



void
OpenDDS::DCPS::SimpleUnreliableDgramDataLink::stop_i()
{
  DBG_ENTRY_LVL("SimpleUnreliableDgramDataLink","stop_i",6);

  // Nothing to do here.
}

