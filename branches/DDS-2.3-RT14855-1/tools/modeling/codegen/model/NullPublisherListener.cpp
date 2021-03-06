// -*- C++ -*-
//
// $Id$
#include "NullPublisherListener.h"
#include <dds/DCPS/debug.h>

OpenDDS::Model::NullPublisherListener::NullPublisherListener()
{
  if( OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) NullPublisherListener::NullPublisherListener()\n")));
  }
}

OpenDDS::Model::NullPublisherListener::~NullPublisherListener()
{
  if( OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) NullPublisherListener::~NullPublisherListener()\n")));
  }
}

void
OpenDDS::Model::NullPublisherListener::on_offered_deadline_missed(
  DDS::DataWriter_ptr /* writer */,
  const DDS::OfferedDeadlineMissedStatus& /* status */
) ACE_THROW_SPEC((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) NullPublisherListener::on_offered_deadline_missed()\n")));
  }
}

void
OpenDDS::Model::NullPublisherListener::on_offered_incompatible_qos(
  DDS::DataWriter_ptr /* writer */,
  const DDS::OfferedIncompatibleQosStatus& /* status */
) ACE_THROW_SPEC((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) NullPublisherListener::on_offered_incompatible_qos()\n")));
  }
}

void
OpenDDS::Model::NullPublisherListener::on_liveliness_lost(
  DDS::DataWriter_ptr /* writer */,
  const DDS::LivelinessLostStatus& /* status */
) ACE_THROW_SPEC((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) NullPublisherListener::on_liveliness_lost()\n")));
  }
}

void
OpenDDS::Model::NullPublisherListener::on_publication_matched(
  DDS::DataWriter_ptr /* writer */,
  const DDS::PublicationMatchedStatus& /* status */
) ACE_THROW_SPEC((CORBA::SystemException))
{
  if( OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) NullPublisherListener::on_publication_matched()\n")));
  }
}
