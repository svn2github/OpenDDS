// -*- C++ -*-
//
// $Id$
#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "ThreadPerConnectionSendTask.h"
#include "TransportQueueElement.h"
#include "DataLink.h"
#include "ThreadPerConRemoveVisitor.h"
#include "dds/DCPS/transport/framework/EntryExit.h"
#include "dds/DCPS/DataSampleList.h"

#include "ace/Auto_Ptr.h"

OpenDDS::DCPS::ThreadPerConnectionSendTask::ThreadPerConnectionSendTask(
  OpenDDS::DCPS::DataLink* link)
  : lock_ (),
    queue_ (1, 10),
    work_available_ (lock_),
    shutdown_initiated_ (false),
    opened_ (false),
    link_ (link)
{
  DBG_ENTRY_LVL("ThreadPerConnectionSendTask","ThreadPerConnectionSendTask",5);
}


OpenDDS::DCPS::ThreadPerConnectionSendTask::~ThreadPerConnectionSendTask()
{
  DBG_ENTRY_LVL("ThreadPerConnectionSendTask","~ThreadPerConnectionSendTask",5);
}


int OpenDDS::DCPS::ThreadPerConnectionSendTask::add_request(SendStrategyOpType op,
                                                        TransportQueueElement* element)
{
  DBG_ENTRY("ThreadPerConnectionSendTask","add");

  ACE_Auto_Ptr<SendRequest> req (new SendRequest);
  req->op_ = op;
  req->element_ = element;

  int result = -1;
  { // guard scope
    GuardType guard(this->lock_);

    if (this->shutdown_initiated_)
      return -1;

    result = this->queue_.put (req.get());
    if (result == 0)
      {
  this->work_available_.signal();
  req.release ();
      }
    else
      ACE_ERROR((LM_ERROR, "(%P|%t)ERROR: ThreadPerConnectionSendTask::add %p\n", "put"));
  }

  return result;
}


int OpenDDS::DCPS::ThreadPerConnectionSendTask::open(void*)
{
  DBG_ENTRY("ThreadPerConnectionSendTask","open");

  GuardType guard(this->lock_);

  // We can assume that we are in the proper state to handle this open()
  // call as long as we haven't been open()'ed before.
  if (this->opened_)
  {
    ACE_ERROR_RETURN((LM_ERROR,
      "(%P|%t) ThreadPerConnectionSendTask failed to open.  "
      "Task has previously been open()'ed.\n"),
      -1);
  }

  // Activate this task object with one worker thread.
  if (this->activate(THR_NEW_LWP | THR_JOINABLE, 1) != 0)
  {
    // Assumes that when activate returns non-zero return code that
    // no threads were activated.
    ACE_ERROR_RETURN((LM_ERROR,
      "(%P|%t) ThreadPerConnectionSendTask failed to activate "
      "the worker threads.\n"),
      -1);
  }

  // Now we have past the point where we can say we've been open()'ed before.
  this->opened_ = true;

  return 0;
}


int OpenDDS::DCPS::ThreadPerConnectionSendTask::svc()
{
  DBG_ENTRY_LVL("ThreadPerConnectionSendTask","svc", 5);

  this->thr_id_ = ACE_OS::thr_self ();

  // Start the "GetWork-And-PerformWork" loop for the current worker thread.
  while (! this->shutdown_initiated_)
  {
    SendRequest* req;
    {
      GuardType guard(this->lock_);
      if (this->queue_.size() == 0)
      {
        this->work_available_.wait ();
      }

      if (this->shutdown_initiated_)
        break;

      req = queue_.get ();
      if(req == 0)
      {
        //I'm not sure why this thread got more signals than actual signals
        //when using thread_per_connection and the user application thread
        //send requests without interval. We just need ignore the dequeue
        //failure.
        //ACE_ERROR ((LM_ERROR, "(%P|%t)ERROR: ThreadPerConnectionSendTask::svc  %p\n",
        //  "dequeue_head"));
        continue;
      }
    }

    this->execute(*req);
    delete req;
  }

  // This will never get executed.
  return 0;
}


int OpenDDS::DCPS::ThreadPerConnectionSendTask::close(u_long flag)
{
  DBG_ENTRY("ThreadPerConnectionSendTask","close");

  if (flag == 0)
    return 0;

  {
    GuardType guard(this->lock_);

    if (this->shutdown_initiated_)
      return 0;

    // Set the shutdown flag to true.
    this->shutdown_initiated_ = true;
    this->work_available_.signal();
  }

  if (this->opened_ && this->thr_id_ != ACE_OS::thr_self ())
    this->wait ();

  return 0;
}

int
OpenDDS::DCPS::ThreadPerConnectionSendTask::remove_sample ( const DataSampleListElement* sample )
{
  DBG_ENTRY("ThreadPerConnectionSendTask","remove_sample");

  GuardType guard(this->lock_);

  // Construct a PacketRemoveVisitor object.
  ThreadPerConRemoveVisitor vistor(sample->sample_);

  // Let it visit our elems_ collection as a "replace" visitor.
  this->queue_.accept_visitor(vistor);

  return vistor.status();
}

void OpenDDS::DCPS::ThreadPerConnectionSendTask::execute (SendRequest& req)
{
  DBG_ENTRY_LVL("ThreadPerConnectionSendTask","execute",5);

  switch (req.op_)
  {
  case SEND_START:
    this->link_->send_start_i ();
    break;
  case SEND:
    this->link_->send_i (req.element_);
    break;
  case SEND_STOP:
    this->link_->send_stop_i ();
    break;
  default:
    ACE_ERROR ((LM_ERROR, "(%P|%t)ERROR: ThreadPerConnectionSendTask::execute unknown command %d\n",
      req.op_));
    break;
  }
}
