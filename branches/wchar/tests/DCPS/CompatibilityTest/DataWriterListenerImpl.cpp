#include "DataWriterListenerImpl.h"
#include "common.h"
#include "../common/SampleInfo.h"
#include "dds/DdsDcpsPublicationC.h"
#include "dds/DCPS/Service_Participant.h"

DataWriterListenerImpl::DataWriterListenerImpl ()
: publication_matched_(false)
{
}

// Implementation skeleton destructor
DataWriterListenerImpl::~DataWriterListenerImpl ()
{
}

void DataWriterListenerImpl::on_offered_deadline_missed (
      ::DDS::DataWriter_ptr writer,
      const ::DDS::OfferedDeadlineMissedStatus & status
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
  ACE_UNUSED_ARG(status) ;
}

void DataWriterListenerImpl::on_offered_incompatible_qos (
      ::DDS::DataWriter_ptr writer,
      const ::DDS::OfferedIncompatibleQosStatus & status
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
  ACE_UNUSED_ARG(status) ;

  ACE_ERROR ((LM_DEBUG,
         ACE_TEXT("(%P|%t) DataWriterListenerImpl::on_offered_incompatible_qos ")
         ACE_TEXT("This should appear when the test is designed to be incompatible.\n")));
}

void DataWriterListenerImpl::on_liveliness_lost (
      ::DDS::DataWriter_ptr writer,
      const ::DDS::LivelinessLostStatus & status
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
  ACE_UNUSED_ARG(status) ;
}
  
void DataWriterListenerImpl::on_publication_match (
      ::DDS::DataWriter_ptr writer,
      const ::DDS::PublicationMatchStatus & status
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
  ACE_UNUSED_ARG(status) ;

  publication_matched_ = true;

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) DataReaderListenerImpl::on_publication_match \n")));
}

void DataWriterListenerImpl::on_publication_disconnected (
      ::DDS::DataWriter_ptr writer,
      const ::OpenDDS::DCPS::PublicationDisconnectedStatus & status
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
  ACE_UNUSED_ARG(status) ;
}

void DataWriterListenerImpl::on_publication_reconnected (
      ::DDS::DataWriter_ptr writer,
      const ::OpenDDS::DCPS::PublicationReconnectedStatus & status
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
  ACE_UNUSED_ARG(status) ;
}

void DataWriterListenerImpl::on_publication_lost (
      ::DDS::DataWriter_ptr writer,
      const ::OpenDDS::DCPS::PublicationLostStatus & status
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
  ACE_UNUSED_ARG(status) ;
}

void DataWriterListenerImpl::on_connection_deleted (
      ::DDS::DataWriter_ptr writer
    )
    ACE_THROW_SPEC ((
      ::CORBA::SystemException
    ))
{
  ACE_UNUSED_ARG(writer) ;
}
