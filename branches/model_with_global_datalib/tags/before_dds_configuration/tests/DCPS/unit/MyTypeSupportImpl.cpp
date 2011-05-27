// -*- C++ -*-
//
// $Id$

#include "MyTypeSupportImpl.h"
#include "dds/DCPS/Registered_Data_Types.h"
#include "dds/DdsDcpsDomainC.h"
#include "dds/DCPS/Service_Participant.h"

#include "dds/DCPS/DataWriterImpl.h"
#include "dds/DCPS/DataReaderImpl.h"

// Implementation skeleton constructor
MyTypeSupportImpl::MyTypeSupportImpl (void)
  {
  }
  
// Implementation skeleton destructor
MyTypeSupportImpl::~MyTypeSupportImpl (void)
  {
  }
  
::DDS::ReturnCode_t MyTypeSupportImpl::register_type (
    ::DDS::DomainParticipant_ptr participant,
    const char * type_name
    ACE_ENV_ARG_DECL
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ::DDS::DomainId_t domain = 0;
    domain = participant->get_domain_id();

    ::DDS::ReturnCode_t registered = 
      ::TAO::DCPS::Registered_Data_Types->register_type(domain,
                                                        type_name,
                                                        this);

    return registered;
  }
  
::TAO::DCPS::DataWriterRemote_ptr MyTypeSupportImpl::create_datawriter (
    ACE_ENV_SINGLE_ARG_DECL
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    MyDataWriterImpl* writer_impl;
    ACE_NEW_RETURN(writer_impl, 
                   MyDataWriterImpl(), 
                   ::TAO::DCPS::DataWriterRemote::_nil());


    ::TAO::DCPS::DataWriterRemote_ptr writer_obj 
        = ::TAO::DCPS::servant_to_reference<TAO::DCPS::DataWriterRemote, 
                                            MyDataWriterImpl, 
                                            TAO::DCPS::DataWriterRemote_ptr> 
              (writer_impl ACE_ENV_ARG_PARAMETER);
    ACE_CHECK_RETURN (::TAO::DCPS::DataWriterRemote::_nil());

    return writer_obj;
  }
  
::TAO::DCPS::DataReaderRemote_ptr MyTypeSupportImpl::create_datareader (
    ACE_ENV_SINGLE_ARG_DECL
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    MyDataReaderImpl* reader_impl;
    ACE_NEW_RETURN(reader_impl, 
                   MyDataReaderImpl(), 
                   ::TAO::DCPS::DataReaderRemote::_nil());


    ::TAO::DCPS::DataReaderRemote_ptr reader_obj 
        = ::TAO::DCPS::servant_to_reference<TAO::DCPS::DataReaderRemote, 
                                            MyDataReaderImpl, 
                                            TAO::DCPS::DataReaderRemote_ptr> 
              (reader_impl ACE_ENV_ARG_PARAMETER);
    ACE_CHECK_RETURN (::TAO::DCPS::DataReaderRemote::_nil());

    return reader_obj;
  }
  

