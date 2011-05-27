// -*- C++ -*-
//
// $Id$

#include "EntryExit.h"

ACE_INLINE
OpenDDS::DCPS::ThreadPerConRemoveVisitor::ThreadPerConRemoveVisitor
                                       (const ACE_Message_Block* sample)
  : sample_(sample),
    status_(0)
{
  DBG_ENTRY("ThreadPerConRemoveVisitor","ThreadPerConRemoveVisitor");
}


ACE_INLINE int
OpenDDS::DCPS::ThreadPerConRemoveVisitor::status() const
{
  DBG_ENTRY("ThreadPerConRemoveVisitor","status");
  return this->status_;
}



