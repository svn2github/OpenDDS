/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "EntryExit.h"

ACE_INLINE
OpenDDS::DCPS::QueueRemoveVisitor::QueueRemoveVisitor
(const TransportQueueElement& sample)
  : sample_(sample),
    status_(0),
    removed_bytes_(0)
{
  DBG_ENTRY_LVL("QueueRemoveVisitor","QueueRemoveVisitor",6);
}

ACE_INLINE int
OpenDDS::DCPS::QueueRemoveVisitor::status() const
{
  DBG_ENTRY_LVL("QueueRemoveVisitor","status",6);
  return this->status_;
}

ACE_INLINE int
OpenDDS::DCPS::QueueRemoveVisitor::removed_bytes() const
{
  DBG_ENTRY_LVL("QueueRemoveVisitor","removed_bytes",6);
  return this->removed_bytes_;
}
