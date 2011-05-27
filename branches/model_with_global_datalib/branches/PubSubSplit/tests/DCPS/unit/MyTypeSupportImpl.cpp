// -*- C++ -*-
//
// $Id$

#include "MyTypeSupportImpl.h"
#include "dds/DCPS/Registered_Data_Types.h"
#include "dds/DdsDcpsDomainC.h"
#include "dds/DCPS/Service_Participant.h"

#include "dds/DCPS/Publication/DataWriterImpl.h"
#include "dds/DCPS/Subscription/DataReaderImpl.h"

// Implementation skeleton constructor
MyTypeSupportImpl::MyTypeSupportImpl (void)
  {
  }

// Implementation skeleton destructor
MyTypeSupportImpl::~MyTypeSupportImpl (void)
  {
  }

DDS::ReturnCode_t
MyTypeSupportImpl::register_type (
    ::DDS::DomainParticipant_ptr participant,
    const char * type_name
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
{
  CORBA::String_var tn;
  if (type_name == 0 || type_name[0] == '\0')
     tn = this->get_type_name ();
  else
     tn = CORBA::string_dup (type_name);

  return ::TAO::DCPS::Registered_Data_Types->register_type(participant,
                                                           tn.in (),
                                                           this);
}


char *
MyTypeSupportImpl::get_type_name (
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
{
  return CORBA::string_dup (this->_interface_repository_id());
}


::TAO::DCPS::DataWriterRemote_ptr MyTypeSupportImpl::create_datawriter (
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
      = ::TAO::DCPS::servant_to_reference (writer_impl);

    return writer_obj;
  }

::TAO::DCPS::DataReaderRemote_ptr MyTypeSupportImpl::create_datareader (
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
      = ::TAO::DCPS::servant_to_reference (reader_impl);

    return reader_obj;
  }


