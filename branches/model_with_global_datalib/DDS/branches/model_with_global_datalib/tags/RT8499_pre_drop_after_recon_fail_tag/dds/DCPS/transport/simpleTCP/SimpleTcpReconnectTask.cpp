// -*- C++ -*-
//
// $Id$
#include  "DCPS/DdsDcps_pch.h"
#include  "SimpleTcpReconnectTask.h"
#include  "SimpleTcpTransport.h"
#include  "SimpleTcpConnection.h"
#include  "dds/DCPS/transport/framework/EntryExit.h"



TAO::DCPS::SimpleTcpReconnectTask::SimpleTcpReconnectTask(
  TAO::DCPS::SimpleTcpTransport* transport_impl)
  : work_available_(lock_),
    active_workers_(lock_),
    shutdown_initiated_(false),
    opened_(false),
    num_threads_(0)
{
  DBG_ENTRY("SimpleTcpReconnectTask","SimpleTcpReconnectTask");
  // Keep a reference for ourselves
  transport_impl->_add_ref();
  this->transport_ = transport_impl;
}


TAO::DCPS::SimpleTcpReconnectTask::~SimpleTcpReconnectTask()
{
  DBG_ENTRY("SimpleTcpReconnectTask","~SimpleTcpReconnectTas");
}


int
TAO::DCPS::SimpleTcpReconnectTask::add(COMMAND  command,
                                       SimpleTcpConnection_rch conn)
{
  DBG_ENTRY("SimpleTcpReconnectTask","add");
  GuardType guard(this->lock_);
  
  ConnectionInfo* info = new ConnectionInfo();
  info->command = command;
  info->connection = conn;

  int result = this->queue_.enqueue_tail (info);
  if (result == 0)
    this->work_available_.signal();
  else
    {
      ACE_ERROR((LM_ERROR, "(%P|%t)SimpleTcpReconnectTask::add %p\n", "enqueue_tail"));
    }
  return result;
}


int
TAO::DCPS::SimpleTcpReconnectTask::open(void*)
{
  DBG_ENTRY("SimpleTcpReconnectTask","open");

  GuardType guard(this->lock_);

  // We can assume that we are in the proper state to handle this open()
  // call as long as we haven't been open()'ed before.
  if (this->opened_)
    {
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) SimpleTcpReconnectTask failed to open.  "
                        "Task has previously been open()'ed.\n"),
                       -1);
    }

  // Activate this task object with one worker thread.
  if (this->activate(THR_NEW_LWP | THR_JOINABLE, 1) != 0)
    {
      // Assumes that when activate returns non-zero return code that
      // no threads were activated.
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) SimpleTcpReconnectTask failed to activate "
                        "the worker threads.\n"),
                       -1);
    }

  // Now we have past the point where we can say we've been open()'ed before.
  this->opened_ = true;

  // Now we wait until the thread has started.
  while (this->num_threads_ != 1)
    {
      this->active_workers_.wait();
    }

  return 0;
}


int
TAO::DCPS::SimpleTcpReconnectTask::svc()
{
   DBG_ENTRY("SimpleTcpReconnectTask","svc");
  // Account for this current worker thread having started the
  // execution of this svc() method.
  {
    GuardType guard(this->lock_);
    ++this->num_threads_;
    this->active_workers_.signal();
  }

  // Start the "GetWork-And-PerformWork" loop for the current worker thread.
  while (! this->shutdown_initiated_)
  {
    {
      GuardType guard(this->lock_);
      if (this->queue_.is_empty())
      {
        this->work_available_.wait ();
      }
    }

    if (this->shutdown_initiated_)
      break;

    ConnectionInfo* conn_info = 0;
    int result = -1;
    {
      GuardType guard(this->lock_);
      result = queue_.dequeue_head (conn_info);
    }

    if(result != 0)
    {
      ACE_ERROR ((LM_ERROR, "(%P|%t)ERROR: SimpleTcpReconnectTask::svc %p\n", 
        "dequeue_head"));
    }
    if (conn_info == 0)
    {
      ACE_ERROR ((LM_ERROR, "(%P|%t)ERROR: SimpleTcpReconnectTask::svc  "
        "dequeued a nil ConnectionInfo object\n"));
    }
    else
    {
        switch (conn_info->command)
        {
          case DO_RECONNECT:
            conn_info->connection->reconnect ();
          break;
          case NEW_CONNECTION_CHECK:
            transport_->fresh_link (conn_info->connection->get_remote_address(), conn_info->connection);
          break;
          default:
            ACE_ERROR ((LM_ERROR, "(%P|%t)ERROR: SimpleTcpReconnectTask::svc unknown command %d\n",
              conn_info->command));
          break;
        }

      delete conn_info;
    }
  }
  // This will never get executed.
  return 0;
}


int
TAO::DCPS::SimpleTcpReconnectTask::close(u_long)
{
  DBG_ENTRY("SimpleTcpReconnectTask","close");

  {
    GuardType guard(this->lock_);
    // Set the shutdown flag to true.
    this->shutdown_initiated_ = true;
    this->active_workers_.signal();
  }

  {
    GuardType guard(this->lock_);
    this->work_available_.signal();
  }

  // Do nothing if this task has never been open()'ed.
  if (!this->opened_)
  {
    return 0;
  }

  return 0;
}


