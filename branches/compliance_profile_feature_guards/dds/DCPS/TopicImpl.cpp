/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TopicImpl.h"
#include "Qos_Helper.h"
#include "Definitions.h"
#include "Service_Participant.h"
#include "DomainParticipantImpl.h"
#include "MonitorFactory.h"
#include "dds/DCPS/transport/framework/TransportExceptions.h"

namespace OpenDDS {
namespace DCPS {

TopicImpl::TopicImpl(const RepoId                   topic_id,
                     const char*                    topic_name,
                     const char*                    type_name,
                     OpenDDS::DCPS::TypeSupport_ptr type_support,
                     const DDS::TopicQos &          qos,
                     DDS::TopicListener_ptr         a_listener,
                     const DDS::StatusMask &        mask,
                     DomainParticipantImpl*         participant)
  : TopicDescriptionImpl(topic_name,
                         type_name,
                         type_support,
                         participant),
    qos_(qos),
    listener_mask_(mask),
    listener_(DDS::TopicListener::_duplicate(a_listener)),
    fast_listener_(0),
    id_(topic_id),
    entity_refs_(0),
    monitor_(0)
{
  inconsistent_topic_status_.total_count = 0;
  inconsistent_topic_status_.total_count_change = 0;
  monitor_ =
    TheServiceParticipant->monitor_factory_->create_topic_monitor(this);
}

TopicImpl::~TopicImpl()
{
}

DDS::ReturnCode_t
TopicImpl::set_qos(const DDS::TopicQos & qos)
{
#ifdef OPENDDS_NO_OWNERSHIP
  if (qos.ownership_strength != TheServiceParticipant->initial_OwnershipStrengthQosPolicy()) {
    return DDS::RETCODE_NOT_SUPPORTED;
  }
  if (qos.ownership.kind == ::DDS::EXCLUSIVE_OWNERSHIP_QOS) {
    return DDS::RETCODE_NOT_SUPPORTED;
  }
#endif
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    if (qos_ == qos)
      return DDS::RETCODE_OK;

    // for the not changeable qos, it can be changed before enable
    if (!Qos_Helper::changeable(qos_, qos) && enabled_ == true) {
      return DDS::RETCODE_IMMUTABLE_POLICY;

    } else {
      qos_ = qos;
      DomainParticipantImpl* part =
        dynamic_cast<DomainParticipantImpl*>(this->participant_);

      Discovery_rch disco =
        TheServiceParticipant->get_discovery(part->get_domain_id());
      const bool status =
        disco->update_topic_qos(this->id_, part->get_domain_id(),
                               part->get_id(), qos_);

      if (!status) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("(%P|%t) TopicImpl::set_qos, ")
                          ACE_TEXT("failed on compatiblity check. \n")),
                         DDS::RETCODE_ERROR);
      }
    }

    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
TopicImpl::get_qos(DDS::TopicQos& qos)
{
  qos = qos_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
TopicImpl::set_listener(DDS::TopicListener_ptr a_listener, DDS::StatusMask mask)
{
  listener_mask_ = mask;
  //note: OK to duplicate  a nil object ref
  listener_ = DDS::TopicListener::_duplicate(a_listener);
  fast_listener_ = listener_.in();
  return DDS::RETCODE_OK;
}

DDS::TopicListener_ptr
TopicImpl::get_listener()
{
  return DDS::TopicListener::_duplicate(listener_.in());
}

DDS::ReturnCode_t
TopicImpl::get_inconsistent_topic_status(DDS::InconsistentTopicStatus& a_status)
{
  a_status = inconsistent_topic_status_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
TopicImpl::enable()
{
  //According spec:
  // - Calling enable on an already enabled Entity returns OK and has no
  // effect.
  // - Calling enable on an Entity whose factory is not enabled will fail
  // and return PRECONDITION_NOT_MET.

  if (this->is_enabled()) {
    return DDS::RETCODE_OK;
  }

  DomainParticipantImpl* part =
    dynamic_cast<DomainParticipantImpl*>(this->participant_);

  if (part->is_enabled() == false) {
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  if (this->monitor_) {
    monitor_->report();
  }
  return this->set_enabled();
}

RepoId
TopicImpl::get_id() const
{
  return id_;
}

DDS::InstanceHandle_t
TopicImpl::get_instance_handle()
{
  return this->participant_->get_handle(this->id_);
}

const char*
TopicImpl::type_name() const
{
  return this->type_name_.c_str();
}

void
TopicImpl::transport_config(const TransportConfig_rch&)
{
  throw Transport::MiscProblem();
}

} // namespace DCPS
} // namespace OpenDDS
