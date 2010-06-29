/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
#include "MultiTopicImpl.h"
#include "Registered_Data_Types.h"
#include "DomainParticipantImpl.h"

namespace OpenDDS {
namespace DCPS {

namespace {
  TypeSupport_ptr findTypeSupport(DomainParticipantImpl* participant,
    const char* type_name)
  {
    TypeSupport_ptr p = Registered_Data_Types->lookup(participant, type_name);
    if (!p) {
      throw std::runtime_error(std::string("Data type: ") + type_name +
        "is not registered.");
    }
    return p;
  }
}

MultiTopicImpl::MultiTopicImpl(const char* name,
  const char* type_name, const char* subscription_expression,
  const DDS::StringSeq& expression_parameters,
  DomainParticipantImpl* participant)
  : TopicDescriptionImpl(name, type_name,
      findTypeSupport(participant, type_name),
      participant)
  , subscription_expression_(subscription_expression)
  , expression_parameters_(expression_parameters)
{}

char* MultiTopicImpl::get_subscription_expression()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return CORBA::string_dup(subscription_expression_.c_str());
}

DDS::ReturnCode_t
MultiTopicImpl::get_expression_parameters(DDS::StringSeq& params)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lock_, false);
  params = expression_parameters_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
MultiTopicImpl::set_expression_parameters(const DDS::StringSeq& p)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lock_, false);
  expression_parameters_ = p;
  return DDS::RETCODE_OK;
}


} // namespace DCPS
} // namespace OpenDDS

#endif // OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE