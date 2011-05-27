// -*- C++ -*-
//
// $Id$
#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "ThreadSynch.h"


#if !defined (__ACE_INLINE__)
#include "ThreadSynch.inl"
#endif /* __ACE_INLINE__ */


OpenDDS::DCPS::ThreadSynch::~ThreadSynch()
{
  DBG_ENTRY_LVL("ThreadSynch","~ThreadSynch",5);
  delete this->resource_;
}


int
OpenDDS::DCPS::ThreadSynch::register_worker_i()
{
  DBG_ENTRY_LVL("ThreadSynch","register_worker_i",5);
  // Default implementation is to do nothing here.  Subclass may override.
  return 0;
}


void
OpenDDS::DCPS::ThreadSynch::unregister_worker_i()
{
  DBG_ENTRY_LVL("ThreadSynch","unregister_worker_i",5);
  // Default implementation is to do nothing here.  Subclass may override.
}

