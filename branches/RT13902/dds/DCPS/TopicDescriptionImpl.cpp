/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TopicDescriptionImpl.h"
#include "DomainParticipantImpl.h"
#include "TopicImpl.h"
#include "Service_Participant.h"

namespace OpenDDS {
namespace DCPS {

// Implementation skeleton constructor
TopicDescriptionImpl::TopicDescriptionImpl(const char*            topic_name,
                                           const char*            type_name,
                                           TypeSupport_ptr        type_support,
                                           DomainParticipantImpl* participant)
  : topic_name_(topic_name),
    type_name_(type_name),
    participant_(participant),
    type_support_(type_support)
{
  // make sure the type_support doesn't get deleted while
  // were still using it.
  type_support_->_add_ref();
}

// Implementation skeleton destructor
TopicDescriptionImpl::~TopicDescriptionImpl()
{
  // were finished with the type_support
  type_support_->_remove_ref();
}

char *
TopicDescriptionImpl::get_type_name()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return CORBA::string_dup(type_name_.c_str());
}

char *
TopicDescriptionImpl::get_name()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return CORBA::string_dup(topic_name_.c_str());
}

DDS::DomainParticipant_ptr
TopicDescriptionImpl::get_participant()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return DDS::DomainParticipant::_duplicate(participant_);
}

OpenDDS::DCPS::TypeSupport_ptr
TopicDescriptionImpl::get_type_support()
{
  return type_support_;
}

} // namespace DCPS
} // namespace OpenDDS
