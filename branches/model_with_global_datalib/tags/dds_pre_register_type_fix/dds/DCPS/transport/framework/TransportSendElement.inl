// -*- C++ -*-
//
// $Id$
#include  "EntryExit.h"

ACE_INLINE
TAO::DCPS::TransportSendElement::TransportSendElement
                                     (int                    initial_count,
                                      DataSampleListElement* sample,
                                      TransportSendElementAllocator* allocator)
  : TransportQueueElement(initial_count),
    element_(sample),
    allocator_(allocator)
{
  DBG_ENTRY("TransportSendElement","TransportSendElement");
}

