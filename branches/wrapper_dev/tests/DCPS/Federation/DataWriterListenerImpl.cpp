// $Id$

#include "DataWriterListenerImpl.h"
#include "dds/DdsDcpsPublicationC.h"

DataWriterListenerImpl::DataWriterListenerImpl ()
  : condition_( this->lock_),
    complete_ (false)
{
}

DataWriterListenerImpl::~DataWriterListenerImpl ()
{
}

void
DataWriterListenerImpl::wait_for_completion()
{
  ACE_GUARD( ACE_SYNCH_MUTEX, guard, this->lock_);
  while( this->complete_ == false) {
    this->condition_.wait();
  }
}

void
DataWriterListenerImpl::on_offered_deadline_missed (
    ::DDS::DataWriter_ptr /* writer */,
    ::DDS::OfferedDeadlineMissedStatus const & /* status */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
}

void
DataWriterListenerImpl::on_offered_incompatible_qos (
    ::DDS::DataWriter_ptr /* writer */,
    ::DDS::OfferedIncompatibleQosStatus const & /* status */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
}

void
DataWriterListenerImpl::on_liveliness_lost (
    ::DDS::DataWriter_ptr /* writer */,
    ::DDS::LivelinessLostStatus const & /* status */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
}
  
void
DataWriterListenerImpl::on_publication_match (
    ::DDS::DataWriter_ptr /* writer */,
    ::DDS::PublicationMatchStatus const & /* status */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
}

void
DataWriterListenerImpl::on_publication_disconnected (
    ::DDS::DataWriter_ptr /* writer */,
    ::OpenDDS::DCPS::PublicationDisconnectedStatus const & /* status */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
}

void
DataWriterListenerImpl::on_publication_reconnected (
    ::DDS::DataWriter_ptr /* writer */,
    ::OpenDDS::DCPS::PublicationReconnectedStatus const & /* status */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
}

void
DataWriterListenerImpl::on_publication_lost (
    ::DDS::DataWriter_ptr /* writer */,
    ::OpenDDS::DCPS::PublicationLostStatus const & /* status */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) DataWriterListenerImpl::on_publication_lost()\n")
  ));

  ACE_GUARD( ACE_SYNCH_MUTEX, guard, this->lock_);
  this->complete_ = true;
  this->condition_.signal();
}

void
DataWriterListenerImpl::on_connection_deleted (
    ::DDS::DataWriter_ptr /* writer */)
  ACE_THROW_SPEC ((::CORBA::SystemException))
{
}
