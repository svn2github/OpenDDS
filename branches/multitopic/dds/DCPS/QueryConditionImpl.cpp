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
#include "QueryConditionImpl.h"
#include "DataReaderImpl.h"

namespace OpenDDS {
namespace DCPS {

char* QueryConditionImpl::get_query_expression()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return CORBA::string_dup(query_expression_);
}

DDS::ReturnCode_t
QueryConditionImpl::get_query_parameters(DDS::StringSeq& query_parameters)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lock_, false);
  query_parameters = query_parameters_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
QueryConditionImpl::set_query_parameters(const DDS::StringSeq& query_parameters)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lock_, false);
  query_parameters_ = query_parameters;
  return DDS::RETCODE_OK;
}

std::vector<std::string>
QueryConditionImpl::getOrderBys() const
{
  return evaluator_.getOrderBys();
}

bool
QueryConditionImpl::hasFilter() const
{
  return evaluator_.hasFilter();
}

CORBA::Boolean
QueryConditionImpl::get_trigger_value()
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (hasFilter()) {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lock_, false);
    return parent_->contains_sample_filtered(sample_states_, view_states_,
      instance_states_, evaluator_, query_parameters_);
  } else {
    return ReadConditionImpl::get_trigger_value();
  }
}

} // namespace DCPS
} // namespace OpenDDS

#endif // OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
