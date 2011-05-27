// -*- C++ -*-
//
// $Id$

#include  "dds/DCPS/transport/framework/DataLink.h"
#include  "SimpleTcpConnection.h"
#include  "dds/DCPS/transport/framework/EntryExit.h"

ACE_INLINE
TAO::DCPS::SimpleTcpReceiveStrategy::SimpleTcpReceiveStrategy
                                        (DataLink*             link,
                                         SimpleTcpConnection*  connection,
                                         TransportReactorTask* task)
{
  DBG_ENTRY("SimpleTcpReceiveStrategy","SimpleTcpReceiveStrategy");

  // Keep a "copy" of the reference to the DataLink for ourselves.
  link->_add_ref();
  this->link_ = link;

  // Keep a "copy" of the reference to the TransportReactorTask for ourselves.
  task->_add_ref();
  this->reactor_task_ = task;

  // Keep a "copy" of the reference to the SimpleTcpConnection for ourselves.
  connection->_add_ref();
  this->connection_ = connection;
}

