/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "EntryExit.h"

ACE_INLINE
OpenDDS::DCPS::TransportSendElement::TransportSendElement(int initial_count,
  const DataSampleListElement* sample,
  TransportSendElementAllocator* allocator)
  : TransportQueueElement(initial_count),
    element_(sample),
    allocator_(allocator)
{
  DBG_ENTRY_LVL("TransportSendElement","TransportSendElement",6);
}

ACE_INLINE
bool 
OpenDDS::DCPS::TransportSendElement::owned_by_transport ()
{
  return false;
}
