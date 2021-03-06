// -*- C++ -*-
//
// $Id$

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "SendResponseListener.h"

#include "ace/Message_Block.h"

namespace OpenDDS { namespace DCPS {

SendResponseListener::~SendResponseListener()
{
}

void
SendResponseListener::data_delivered(DataSampleListElement* /* sample */)
{
}

void
SendResponseListener::data_dropped(
  DataSampleListElement* /* sample */,
  bool /* dropped_by_transport */
)
{
}

void
SendResponseListener::control_delivered(ACE_Message_Block* sample)
{
  if( sample != 0) sample->release();
}

void
SendResponseListener::control_dropped(
  ACE_Message_Block* sample,
  bool /* dropped_by_transport */
)
{
  if( sample != 0) sample->release();
}

}}  // namespace OpenDDS::DCPS

