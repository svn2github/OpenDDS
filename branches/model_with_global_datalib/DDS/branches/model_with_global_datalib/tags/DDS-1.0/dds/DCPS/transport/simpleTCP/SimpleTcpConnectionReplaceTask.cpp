// -*- C++ -*-
//
// $Id$
#include "SimpleTcp_pch.h"
#include "SimpleTcpConnectionReplaceTask.h"
#include "SimpleTcpTransport.h"
#include "SimpleTcpConnection.h"
#include "SimpleTcpSendStrategy.h"
#include "dds/DCPS/transport/framework/EntryExit.h"



OpenDDS::DCPS::SimpleTcpConnectionReplaceTask::SimpleTcpConnectionReplaceTask(
  SimpleTcpTransport* trans)
  : trans_ (trans)
{
  DBG_ENTRY_LVL("SimpleTcpConnectionReplaceTask","SimpleTcpConnectionReplaceTask",5);
}


OpenDDS::DCPS::SimpleTcpConnectionReplaceTask::~SimpleTcpConnectionReplaceTask()
{
  DBG_ENTRY_LVL("SimpleTcpConnectionReplaceTask","~SimpleTcpConnectionReplaceTask",5);
}



void OpenDDS::DCPS::SimpleTcpConnectionReplaceTask::execute (SimpleTcpConnection_rch& con)
{
  DBG_ENTRY_LVL("SimpleTcpConnectionReplaceTask","execute",5);

  this->trans_->fresh_link (con->get_remote_address(), con);
}




