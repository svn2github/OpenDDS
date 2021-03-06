// -*- C++ -*-
//
// $Id$
#include "PublisherListener.h"

extern int offered_incompatible_qos_called_on_pub;

// Implementation skeleton constructor
PublisherListenerImpl::PublisherListenerImpl (void)
  {
  }

// Implementation skeleton destructor
PublisherListenerImpl::~PublisherListenerImpl (void)
  {
  }

void PublisherListenerImpl::on_offered_deadline_missed (
    ::DDS::DataWriter_ptr writer,
    const ::DDS::OfferedDeadlineMissedStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(writer);
    ACE_UNUSED_ARG(status);
    // Add your implementation here
  }

void PublisherListenerImpl::on_offered_incompatible_qos (
    ::DDS::DataWriter_ptr writer,
    const ::DDS::OfferedIncompatibleQosStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(writer);
    ACE_UNUSED_ARG(status);
    offered_incompatible_qos_called_on_pub ++;
  }

void PublisherListenerImpl::on_liveliness_lost (
    ::DDS::DataWriter_ptr writer,
    const ::DDS::LivelinessLostStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(writer);
    ACE_UNUSED_ARG(status);
    // Add your implementation here
  }

void PublisherListenerImpl::on_publication_matched (
    ::DDS::DataWriter_ptr writer,
    const ::DDS::PublicationMatchedStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(writer);
    ACE_UNUSED_ARG(status);
    // Add your implementation here
  }
