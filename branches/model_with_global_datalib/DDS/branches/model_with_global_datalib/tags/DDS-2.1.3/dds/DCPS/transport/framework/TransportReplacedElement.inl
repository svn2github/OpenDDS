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
OpenDDS::DCPS::TransportReplacedElement::TransportReplacedElement
(TransportQueueElement* orig_elem,
 TransportReplacedElementAllocator* allocator,
 MessageBlockAllocator* mb_allocator,
 DataBlockAllocator* db_allocator)
  : TransportQueueElement(1),
    allocator_(allocator),
    mb_allocator_ (mb_allocator),
    db_allocator_ (db_allocator),
    msg_(0)
{
  DBG_ENTRY_LVL("TransportReplacedElement","TransportReplacedElement",6);

  // Obtain the publisher id.
  this->publisher_id_ = orig_elem->publication_id();
  
  this->msg_ = TransportQueueElement::clone (orig_elem->msg(), 
                                            this->mb_allocator_, 
                                            this->db_allocator_);
}
