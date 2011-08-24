/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_SUBSCRIBER_H
#define OPENDDS_DCPS_SUBSCRIBER_H

#include "dds/DdsDcpsSubscriptionExtS.h"
#include "dds/DdsDcpsDataReaderRemoteC.h"
#include "dds/DdsDcpsInfoC.h"
#include "EntityImpl.h"
#include "Definitions.h"
#include "DataCollector_T.h"
#include "DataReaderImpl.h"
#include "ace/Synch.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include <map>
#include <set>
#include <list>
#include <vector>

namespace OpenDDS {
namespace DCPS {

class DomainParticipantImpl;
class Monitor;

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
class MultiTopicImpl;
#endif

class OpenDDS_Dcps_Export SubscriberImpl
  : public virtual OpenDDS::DCPS::LocalObject<SubscriberExt>
  , public virtual EntityImpl {
public:

  SubscriberImpl(DDS::InstanceHandle_t handle,
                 const DDS::SubscriberQos& qos,
                 DDS::SubscriberListener_ptr a_listener,
                 const DDS::StatusMask& mask,
                 DomainParticipantImpl* participant);

  virtual ~SubscriberImpl();

  virtual DDS::InstanceHandle_t get_instance_handle()
  ACE_THROW_SPEC((CORBA::SystemException));

  bool contains_reader(DDS::InstanceHandle_t a_handle);

  virtual DDS::DataReader_ptr create_datareader(
    DDS::TopicDescription_ptr a_topic_desc,
    const DDS::DataReaderQos& qos,
    DDS::DataReaderListener_ptr a_listener,
    DDS::StatusMask mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::DataReader_ptr create_opendds_datareader(
    DDS::TopicDescription_ptr a_topic_desc,
    const DDS::DataReaderQos& qos,
    const DataReaderQosExt& ext_qos,
    DDS::DataReaderListener_ptr a_listener,
    DDS::StatusMask mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t delete_datareader(
    DDS::DataReader_ptr a_datareader)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t delete_contained_entities()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::DataReader_ptr lookup_datareader(
    const char* topic_name)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_datareaders(
    DDS::DataReaderSeq& readers,
    DDS::SampleStateMask sample_states,
    DDS::ViewStateMask view_states,
    DDS::InstanceStateMask instance_states)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t notify_datareaders()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_qos(
    const DDS::SubscriberQos& qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_qos(
    DDS::SubscriberQos& qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_listener(
    DDS::SubscriberListener_ptr a_listener,
    DDS::StatusMask mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::SubscriberListener_ptr get_listener()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t begin_access()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t end_access()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::DomainParticipant_ptr get_participant()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_default_datareader_qos(
    const DDS::DataReaderQos& qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_default_datareader_qos(
    DDS::DataReaderQos& qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual void get_default_datareader_qos_ext(
    DataReaderQosExt& qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t copy_from_topic_qos(
    DDS::DataReaderQos& a_datareader_qos,
    const DDS::TopicQos& a_topic_qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t enable()
  ACE_THROW_SPEC((CORBA::SystemException));

  /** This method is not defined in the IDL and is defined for
  *  internal use.
  *  Check if there is any datareader associated with it.
  */
  bool is_clean() const;

  // called by DataReaderImpl::data_received
  void data_received(DataReaderImpl* reader);

  DDS::ReturnCode_t reader_enabled(const char* topic_name,
                                   DataReaderImpl* reader);

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  DDS::ReturnCode_t multitopic_reader_enabled(DDS::DataReader_ptr reader);
#endif

  DDS::SubscriberListener* listener_for(DDS::StatusKind kind);

  /// @name Raw Latency Statistics Configuration Interfaces
  /// @{

  /// Configure the size of the raw data collection buffer.
  unsigned int& raw_latency_buffer_size();

  /// Configure the type of the raw data collection buffer.
  DataCollector<double>::OnFull& raw_latency_buffer_type();

  /// @}

  typedef std::vector<RepoId> SubscriptionIdVec;
  /// Populates a std::vector with the SubscriptionIds (GUIDs)
  /// of this Subscriber's Data Readers
  void get_subscription_ids(SubscriptionIdVec& subs);

  void update_ownership_strength (const PublicationId& pub_id,
                                  const CORBA::Long& ownership_strength);

  void coherent_change_received(RepoId& publisher_id,
                                DataReaderImpl* reader,
                                Coherent_State& group_state);

  virtual EntityImpl* parent() const;

private:

  // Keep track of all the DataReaders attached to this
  // Subscriber: key is the topic_name
  typedef std::multimap<std::string, DataReaderImpl*> DataReaderMap;

  // Keep track of DataReaders with data
  // std::set for now, want to encapsulate
  // this so we can switch between a set or
  // list depending on Presentation Qos.
  typedef std::set<DataReaderImpl*> DataReaderSet;

  // DataReader id to qos map.
  typedef std::map<RepoId, DDS::DataReaderQos, GUID_tKeyLessThan> DrIdToQosMap;

  DDS::InstanceHandle_t        handle_;

  DDS::SubscriberQos           qos_;
  DDS::DataReaderQos           default_datareader_qos_;

  DDS::StatusMask              listener_mask_;
  DDS::SubscriberListener_var  listener_;
  DDS::SubscriberListener*     fast_listener_;

  DataReaderMap                datareader_map_;
  DataReaderSet                datareader_set_;

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  std::map<std::string, DDS::DataReader_var> multitopic_reader_map_;
#endif

  DomainParticipantImpl*       participant_;
  DDS::DomainParticipant_var   participant_objref_;

  DDS::DomainId_t              domain_id_;

  /// Bound (or initial reservation) of raw latency buffers.
  unsigned int raw_latency_buffer_size_;

  /// Type of raw latency data buffers.
  DataCollector<double>::OnFull raw_latency_buffer_type_;

  /// this lock protects the data structures in this class.
  ACE_Recursive_Thread_Mutex   si_lock_;

  /// Monitor object for this entity
  Monitor* monitor_;

  int access_depth_;
};

} // namespace DCPS
} // namespace OpenDDS

#endif /* OPENDDS_DCPS_SUBSCRIBER_H  */
