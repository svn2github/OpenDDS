/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_CONTENTFILTEREDTOPICIMPL_H
#define OPENDDS_DCPS_CONTENTFILTEREDTOPICIMPL_H

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE

#include "dds/DCPS/TopicDescriptionImpl.h"
#include "dds/DCPS/FilterEvaluator.h"

#include <string>

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

namespace OpenDDS {
namespace DCPS {

class OpenDDS_Dcps_Export ContentFilteredTopicImpl
  : public virtual OpenDDS::DCPS::LocalObject<DDS::ContentFilteredTopic>
  , public virtual TopicDescriptionImpl {
public:
  ContentFilteredTopicImpl(const char* name, DDS::Topic_ptr related_topic,
    const char* filter_expression, const DDS::StringSeq& expression_parameters,
    DomainParticipantImpl* participant);

  virtual ~ContentFilteredTopicImpl() {}

  char* get_filter_expression()
    ACE_THROW_SPEC((CORBA::SystemException));

  DDS::ReturnCode_t get_expression_parameters(DDS::StringSeq& parameters)
    ACE_THROW_SPEC((CORBA::SystemException));

  DDS::ReturnCode_t set_expression_parameters(const DDS::StringSeq& parameters)
    ACE_THROW_SPEC((CORBA::SystemException));

  DDS::Topic_ptr get_related_topic()
    ACE_THROW_SPEC((CORBA::SystemException));

  template<typename Sample>
  bool filter(const Sample& s) const
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lock_, false);
    return filter_eval_.eval(s, expression_parameters_);
  }

private:
  std::string filter_expression_;
  FilterEvaluator filter_eval_;
  DDS::StringSeq expression_parameters_;
  DDS::Topic_var related_topic_;

  ///concurrent access to expression_parameters_
  mutable ACE_Recursive_Thread_Mutex lock_;
};

} // namespace DCPS
} // namespace OpenDDS

#endif // OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE

#endif
