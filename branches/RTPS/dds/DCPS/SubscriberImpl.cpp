/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "debug.h"
#include "SubscriberImpl.h"
#include "DataReaderRemoteImpl.h"
#include "DomainParticipantImpl.h"
#include "Qos_Helper.h"
#include "RepoIdConverter.h"
#include "TopicImpl.h"
#include "MonitorFactory.h"
#include "DataReaderImpl.h"
#include "Service_Participant.h"
#include "dds/DdsDcpsTypeSupportExtC.h"
#include "TopicDescriptionImpl.h"
#include "Marked_Default_Qos.h"
#include "DataSampleList.h"
#include "Transient_Kludge.h"
#include "ContentFilteredTopicImpl.h"
#include "MultiTopicImpl.h"
#include "GroupRakeData.h"
#include "MultiTopicDataReaderBase.h"
#include "Util.h"
#include "dds/DCPS/transport/framework/TransportImpl.h"
#include "dds/DCPS/transport/framework/DataLinkSet.h"

#include "tao/debug.h"

#include "ace/Auto_Ptr.h"
#include "ace/Vector_T.h"

#include <stdexcept>

namespace OpenDDS {
namespace DCPS {

// Implementation skeleton constructor
SubscriberImpl::SubscriberImpl(DDS::InstanceHandle_t handle,
                               const DDS::SubscriberQos & qos,
                               DDS::SubscriberListener_ptr a_listener,
                               const DDS::StatusMask& mask,
                               DomainParticipantImpl* participant)
  : handle_(handle),
    qos_(qos),
    default_datareader_qos_(TheServiceParticipant->initial_DataReaderQos()),
    listener_mask_(mask),
    fast_listener_(0),
    participant_(participant),
    domain_id_(participant->get_domain_id()),
    raw_latency_buffer_size_(0),
    raw_latency_buffer_type_(DataCollector<double>::KeepOldest),
    monitor_(0),
    access_depth_ (0)
{
  //Note: OK to duplicate a nil.
  listener_ = DDS::SubscriberListener::_duplicate(a_listener);

  if (!CORBA::is_nil(a_listener)) {
    fast_listener_ = listener_.in();
  }

  monitor_ = TheServiceParticipant->monitor_factory_->create_subscriber_monitor(this);
}

// Implementation skeleton destructor
SubscriberImpl::~SubscriberImpl()
{
  //
  // The datareders should be deleted already before calling delete
  // subscriber.
  if (!is_clean()) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("SubscriberImpl::~SubscriberImpl, ")
               ACE_TEXT("some datareaders still exist.\n")));
  }
}

DDS::InstanceHandle_t
SubscriberImpl::get_instance_handle()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return handle_;
}

bool
SubscriberImpl::contains_reader(DDS::InstanceHandle_t a_handle)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   false);

  for (DataReaderSet::iterator it(datareader_set_.begin());
       it != datareader_set_.end(); ++it) {
    if (a_handle == (*it)->get_instance_handle())
      return true;
  }

  return false;
}

DDS::DataReader_ptr
SubscriberImpl::create_datareader(
  DDS::TopicDescription_ptr a_topic_desc,
  const DDS::DataReaderQos & qos,
  DDS::DataReaderListener_ptr a_listener,
  DDS::StatusMask mask)
ACE_THROW_SPEC((CORBA::SystemException))
{
  DataReaderQosExt ext_qos;
  get_default_datareader_qos_ext(ext_qos);
  return create_opendds_datareader(a_topic_desc, qos, ext_qos, a_listener, mask);
}

DDS::DataReader_ptr
SubscriberImpl::create_opendds_datareader(
  DDS::TopicDescription_ptr a_topic_desc,
  const DDS::DataReaderQos & qos,
  const DataReaderQosExt & ext_qos,
  DDS::DataReaderListener_ptr a_listener,
  DDS::StatusMask mask)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (CORBA::is_nil(a_topic_desc)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("SubscriberImpl::create_datareader, ")
               ACE_TEXT("topic desc is nil.\n")));
    return DDS::DataReader::_nil();
  }

  DDS::DataReaderQos dr_qos;

  TopicImpl* topic_servant = dynamic_cast<TopicImpl*>(a_topic_desc);

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  ContentFilteredTopicImpl* cft = 0;
  MultiTopicImpl* mt = 0;
  DDS::Topic_var related;
  if (!topic_servant) {
    cft = dynamic_cast<ContentFilteredTopicImpl*>(a_topic_desc);
    if (cft) {
      related = cft->get_related_topic();
      topic_servant = dynamic_cast<TopicImpl*>(related.in());
    } else {
      mt = dynamic_cast<MultiTopicImpl*>(a_topic_desc);
    }
  }
#endif

  if (qos == DATAREADER_QOS_DEFAULT) {
    this->get_default_datareader_qos(dr_qos);

  } else if (qos == DATAREADER_QOS_USE_TOPIC_QOS) {
#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
    if (mt) {
      if (DCPS_debug_level) {
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
          ACE_TEXT("SubscriberImpl::create_opendds_datareader, ")
          ACE_TEXT("DATAREADER_QOS_USE_TOPIC_QOS can not be used ")
          ACE_TEXT("to create a MultiTopic DataReader.\n")));
      }
      return DDS::DataReader::_nil();
    }
#endif
    DDS::TopicQos topic_qos;
    topic_servant->get_qos(topic_qos);

    this->get_default_datareader_qos(dr_qos);

    this->copy_from_topic_qos(dr_qos,
                              topic_qos);

  } else {
    dr_qos = qos;
  }

  if (!Qos_Helper::valid(dr_qos)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("SubscriberImpl::create_datareader, ")
               ACE_TEXT("invalid qos.\n")));
    return DDS::DataReader::_nil();
  }

  if (!Qos_Helper::consistent(dr_qos)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("SubscriberImpl::create_datareader, ")
               ACE_TEXT("inconsistent qos.\n")));
    return DDS::DataReader::_nil();
  }

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  if (mt) {
    try {
      DDS::DataReader_var dr =
        mt->get_type_support()->create_multitopic_datareader();
      MultiTopicDataReaderBase* mtdr =
        dynamic_cast<MultiTopicDataReaderBase*>(dr.in());
      mtdr->init(dr_qos, ext_qos, a_listener, mask, this, mt);
      if (enabled_.value() && qos_.entity_factory.autoenable_created_entities) {
        if (dr->enable() != DDS::RETCODE_OK) {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("(%P|%t) ERROR: ")
                     ACE_TEXT("SubscriberImpl::create_datareader, ")
                     ACE_TEXT("enable of MultiTopicDataReader failed.\n")));
          return DDS::DataReader::_nil();
        }
        multitopic_reader_enabled(dr);
      }
      return dr._retn();
    } catch (const std::exception& e) {
        ACE_ERROR((LM_ERROR,
                   ACE_TEXT("(%P|%t) ERROR: ")
                   ACE_TEXT("SubscriberImpl::create_datareader, ")
                   ACE_TEXT("creation of MultiTopicDataReader failed: %C.\n"),
                   e.what()));
    }
    return DDS::DataReader::_nil();
  }
#endif

  OpenDDS::DCPS::TypeSupport_ptr typesupport =
    topic_servant->get_type_support();

  if (0 == typesupport) {
    CORBA::String_var name = a_topic_desc->get_name();
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("SubscriberImpl::create_datareader, ")
               ACE_TEXT("typesupport(topic_name=%C) is nil.\n"),
               name.in()));
    return DDS::DataReader::_nil();
  }

  DDS::DataReader_var dr_obj = typesupport->create_datareader();

  DataReaderImpl* dr_servant =
    dynamic_cast<DataReaderImpl*>(dr_obj.in());

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  if (cft) {
    dr_servant->enable_filtering(cft);
  }
#endif

  DataReaderRemoteImpl* reader_remote_impl = 0;
  ACE_NEW_RETURN(reader_remote_impl,
                 DataReaderRemoteImpl(dr_servant),
                 DDS::DataReader::_nil());

  //this is taking ownership of the DataReaderRemoteImpl (server side) allocated above
  PortableServer::ServantBase_var reader_remote(reader_remote_impl);

  //this is the client reference to the DataReaderRemoteImpl
  OpenDDS::DCPS::DataReaderRemote_var dr_remote_obj =
    servant_to_remote_reference(reader_remote_impl);

  // Propagate the latency buffer data collection configuration.
  // @TODO: Determine whether we want to exclude the Builtin Topic
  //        readers from data gathering.
  dr_servant->raw_latency_buffer_size() = this->raw_latency_buffer_size_;
  dr_servant->raw_latency_buffer_type() = this->raw_latency_buffer_type_;

  dr_servant->init(topic_servant,
                   dr_qos,
                   ext_qos,
                   a_listener,
                   mask,
                   participant_,
                   this,
                   dr_obj.in(),
                   dr_remote_obj.in());

  if ((this->enabled_ == true)
      && (qos_.entity_factory.autoenable_created_entities == 1)) {
    DDS::ReturnCode_t ret
    = dr_servant->enable();

    if (ret != DDS::RETCODE_OK) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: ")
                 ACE_TEXT("SubscriberImpl::create_datareader, ")
                 ACE_TEXT("enable failed.\n")));
      return DDS::DataReader::_nil();
    }
  }

  // add created data reader to this' data reader container -
  // done in enable_reader
  return DDS::DataReader::_duplicate(dr_obj.in());
}

DDS::ReturnCode_t
SubscriberImpl::delete_datareader(::DDS::DataReader_ptr a_datareader)
ACE_THROW_SPEC((CORBA::SystemException))
{
  DBG_ENTRY_LVL("SubscriberImpl", "delete_datareader", 6);

  DataReaderImpl* dr_servant = dynamic_cast<DataReaderImpl*>(a_datareader);

  if (dr_servant) { // for MultiTopic this will be false
    DDS::Subscriber_var dr_subscriber(dr_servant->get_subscriber());

    if (dr_subscriber.in() != this) {
      RepoId id = dr_servant->get_subscription_id();
      RepoIdConverter converter(id);
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) SubscriberImpl::delete_datareader: ")
                 ACE_TEXT("data reader %C doesn't belong to this subscriber.\n"),
                 std::string(converter).c_str()));
      return DDS::RETCODE_PRECONDITION_NOT_MET;
    }

    int loans = dr_servant->num_zero_copies();

    if (0 != loans) {
      return DDS::RETCODE_PRECONDITION_NOT_MET;
    }
  }

  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     guard,
                     this->si_lock_,
                     DDS::RETCODE_ERROR);

    DataReaderMap::iterator it;

    for (it = datareader_map_.begin();
         it != datareader_map_.end();
         ++it) {
      if (it->second == dr_servant) {
        break;
      }
    }

    if (it == datareader_map_.end()) {
      DDS::TopicDescription_var td = a_datareader->get_topicdescription();
      CORBA::String_var topic_name = td->get_name();
#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
      std::map<std::string, DDS::DataReader_var>::iterator mt_iter =
        multitopic_reader_map_.find(topic_name.in());
      if (mt_iter != multitopic_reader_map_.end()) {
        DDS::DataReader_ptr ptr = mt_iter->second;
        dynamic_cast<MultiTopicDataReaderBase*>(ptr)->cleanup();
        multitopic_reader_map_.erase(mt_iter);
        return DDS::RETCODE_OK;
      }
#endif
      RepoId id = dr_servant->get_subscription_id();
      RepoIdConverter converter(id);
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("SubscriberImpl::delete_datareader: ")
                        ACE_TEXT("datareader(topic_name=%C) %C not found.\n"),
                        topic_name.in(),
                        std::string(converter).c_str()),::DDS::RETCODE_ERROR);
    }

    datareader_map_.erase(it);

    datareader_set_.erase(dr_servant);
  }

  if (this->monitor_) {
    this->monitor_->report();
  }

  RepoId subscription_id  = dr_servant->get_subscription_id();

  try {
    DCPSInfo_var repo = TheServiceParticipant->get_repository(this->domain_id_);
    repo->remove_subscription(this->domain_id_,
                              participant_->get_id(),
                              subscription_id) ;

  } catch (const CORBA::SystemException& sysex) {
    sysex._tao_print_exception(
      "ERROR: System Exception"
      " in SubscriberImpl::delete_datareader");
    return DDS::RETCODE_ERROR;

  } catch (const CORBA::UserException& userex) {
    userex._tao_print_exception(
      "ERROR: User Exception"
      " in SubscriberImpl::delete_datareader");
    return DDS::RETCODE_ERROR;
  }

  // Call remove association before unregistering the datareader from the transport,
  // otherwise some callbacks resulted from remove_association may lost.

  dr_servant->remove_all_associations();

  dr_servant->cleanup();

  // Decrease the ref count after the servant is removed
  // from the datareader map.
  dr_servant->_remove_ref();

  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
SubscriberImpl::delete_contained_entities()
ACE_THROW_SPEC((CORBA::SystemException))
{
  // mark that the entity is being deleted
  set_deleted(true);

  ACE_Vector<DDS::DataReader_ptr> drs ;

  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     guard,
                     this->si_lock_,
                     DDS::RETCODE_ERROR);

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
    for (std::map<std::string, DDS::DataReader_var>::iterator mt_iter =
           multitopic_reader_map_.begin();
         mt_iter != multitopic_reader_map_.end(); ++mt_iter) {
      drs.push_back(mt_iter->second);
    }
#endif

    DataReaderMap::iterator it;
    DataReaderMap::iterator itEnd = datareader_map_.end();

    for (it = datareader_map_.begin(); it != itEnd; ++it) {
      drs.push_back(it->second);
    }
  }

  size_t num_rds = drs.size();

  for (size_t i = 0; i < num_rds; ++i) {
    DDS::ReturnCode_t ret = delete_datareader(drs[i]);

    if (ret != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("SubscriberImpl::delete_contained_entities, ")
                        ACE_TEXT("failed to delete datareader\n")),
                       ret);
    }
  }

  // the subscriber can now start creating new publications
  set_deleted(false);

  return DDS::RETCODE_OK;
}

DDS::DataReader_ptr
SubscriberImpl::lookup_datareader(
  const char * topic_name)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   DDS::DataReader::_nil());

  // If multiple entries whose key is "topic_name" then which one is
  // returned ? Spec does not limit which one should give.
  DataReaderMap::iterator it = datareader_map_.find(topic_name);

  if (it == datareader_map_.end()) {
#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
    std::map<std::string, DDS::DataReader_var>::iterator mt_iter =
      multitopic_reader_map_.find(topic_name);
    if (mt_iter != multitopic_reader_map_.end()) {
      return DDS::DataReader::_duplicate(mt_iter->second);
    }
#endif

    if (DCPS_debug_level >= 2) {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) ")
                 ACE_TEXT("SubscriberImpl::lookup_datareader, ")
                 ACE_TEXT("The datareader(topic_name=%C) is not found\n"),
                 topic_name));
    }

    return DDS::DataReader::_nil();

  } else {
    return DDS::DataReader::_duplicate(it->second);
  }
}

DDS::ReturnCode_t
SubscriberImpl::get_datareaders(
  DDS::DataReaderSeq & readers,
  DDS::SampleStateMask sample_states,
  DDS::ViewStateMask view_states,
  DDS::InstanceStateMask instance_states)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   DDS::RETCODE_ERROR);

  // If access_scope is GROUP and ordered_access is true then return readers as
  // list which may contain same readers multiple times. Otherwise return readers
  // as set.
  if (this->qos_.presentation.access_scope == ::DDS::GROUP_PRESENTATION_QOS) {
    if (this->access_depth_ == 0 && this->qos_.presentation.coherent_access) {
      return ::DDS::RETCODE_PRECONDITION_NOT_MET;
    }

    if (this->qos_.presentation.ordered_access) {

      GroupRakeData data;
      for (DataReaderSet::const_iterator pos = datareader_set_.begin() ;
        pos != datareader_set_.end() ; ++pos) {
          (*pos)->get_ordered_data (data, sample_states, view_states, instance_states);
      }

      // Return list of readers in the order of the source timestamp of the received
      // samples from readers.
      data.get_datareaders (readers);

      return DDS::RETCODE_OK ;
    }
  }

  // Return set of datareaders.
  int count(0) ;
  readers.length(count);

  for (DataReaderSet::const_iterator pos = datareader_set_.begin() ;
       pos != datareader_set_.end() ; ++pos) {
    if ((*pos)->have_sample_states(sample_states) &&
        (*pos)->have_view_states(view_states) &&
        (*pos)->have_instance_states(instance_states)) {
      push_back(readers, (*pos)->get_dr_obj_ref());
      ++count;
    }
  }

  return DDS::RETCODE_OK ;
}

DDS::ReturnCode_t
SubscriberImpl::notify_datareaders()
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   DDS::RETCODE_ERROR);

  DataReaderMap::iterator it;

  for (it = datareader_map_.begin(); it != datareader_map_.end(); ++it) {
    if (it->second->have_sample_states(DDS::NOT_READ_SAMPLE_STATE)) {
      DDS::DataReaderListener_var listener = it->second->get_listener();
      if (!CORBA::is_nil (listener)) {
        listener->on_data_available(it->second);
      }

      it->second->set_status_changed_flag(DDS::DATA_AVAILABLE_STATUS, false);
    }
  }

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  for (std::map<std::string, DDS::DataReader_var>::iterator it =
         multitopic_reader_map_.begin(); it != multitopic_reader_map_.end();
       ++it) {
    MultiTopicDataReaderBase* dri =
      dynamic_cast<MultiTopicDataReaderBase*>(it->second.in());
    if (dri->have_sample_states(DDS::NOT_READ_SAMPLE_STATE)) {
      DDS::DataReaderListener_var listener = dri->get_listener();
      if (!CORBA::is_nil(listener)) {
        listener->on_data_available(dri);
      }
      dri->set_status_changed_flag(DDS::DATA_AVAILABLE_STATUS, false);
    }
  }
#endif

  return DDS::RETCODE_OK ;
}

DDS::ReturnCode_t
SubscriberImpl::set_qos(
  const DDS::SubscriberQos & qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    if (qos_ == qos)
      return DDS::RETCODE_OK;

    // for the not changeable qos, it can be changed before enable
    if (!Qos_Helper::changeable(qos_, qos) && enabled_ == true) {
      return DDS::RETCODE_IMMUTABLE_POLICY;

    } else {
      qos_ = qos;

      DrIdToQosMap idToQosMap;
      {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                         guard,
                         this->si_lock_,
                         DDS::RETCODE_ERROR);
        DataReaderMap::const_iterator endIter = datareader_map_.end();

        for (DataReaderMap::const_iterator iter = datareader_map_.begin() ;
             iter != endIter ; ++iter) {
          DataReaderImpl* reader = iter->second;
          reader->set_subscriber_qos (qos);
          DDS::DataReaderQos qos;
          reader->get_qos(qos);
          RepoId id = reader->get_subscription_id();
          std::pair<DrIdToQosMap::iterator, bool> pair
          = idToQosMap.insert(DrIdToQosMap::value_type(id, qos));

          if (pair.second == false) {
            RepoIdConverter converter(id);
            ACE_ERROR_RETURN((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::set_qos: ")
                              ACE_TEXT("insert %C to DrIdToQosMap failed.\n"),
                              std::string(converter).c_str()),::DDS::RETCODE_ERROR);
          }
        }
      }

      DrIdToQosMap::iterator iter = idToQosMap.begin();

      while (iter != idToQosMap.end()) {
        try {
          DCPSInfo_var repo = TheServiceParticipant->get_repository(this->domain_id_);
          CORBA::Boolean status
          = repo->update_subscription_qos(this->domain_id_,
                                          participant_->get_id(),
                                          iter->first,
                                          iter->second,
                                          this->qos_);

          if (status == 0) {
            ACE_ERROR_RETURN((LM_ERROR,
                              ACE_TEXT("(%P|%t) SubscriberImpl::set_qos, ")
                              ACE_TEXT("failed on compatiblity check. \n")),
                             DDS::RETCODE_ERROR);
          }

        } catch (const CORBA::SystemException& sysex) {
          sysex._tao_print_exception(
            "ERROR: System Exception"
            " in SubscriberImpl::set_qos");
          return DDS::RETCODE_ERROR;

        } catch (const CORBA::UserException& userex) {
          userex._tao_print_exception(
            "ERROR:  Exception"
            " in SubscriberImpl::set_qos");
          return DDS::RETCODE_ERROR;
        }

        ++iter;
      }
    }

    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
SubscriberImpl::get_qos(
  DDS::SubscriberQos & qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  qos = qos_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
SubscriberImpl::set_listener(
  DDS::SubscriberListener_ptr a_listener,
  DDS::StatusMask mask)
ACE_THROW_SPEC((CORBA::SystemException))
{
  listener_mask_ = mask;
  //note: OK to duplicate  a nil object ref
  listener_ = DDS::SubscriberListener::_duplicate(a_listener);
  fast_listener_ = listener_.in();
  return DDS::RETCODE_OK;
}

DDS::SubscriberListener_ptr
SubscriberImpl::get_listener()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return DDS::SubscriberListener::_duplicate(listener_.in());
}

DDS::ReturnCode_t
SubscriberImpl::begin_access()
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::begin_access:")
                      ACE_TEXT(" Subscriber is not enabled!\n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  if (qos_.presentation.access_scope != DDS::GROUP_PRESENTATION_QOS) {
    return DDS::RETCODE_OK;
  }

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   DDS::RETCODE_ERROR);

  ++this->access_depth_;

  // We should only notify subscription on the first
  // and last change to the current change set:
  if (this->access_depth_ == 1) {
    for (DataReaderSet::iterator it = this->datareader_set_.begin();
         it != this->datareader_set_.end(); ++it) {
      (*it)->begin_access();
    }
  }

  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
SubscriberImpl::end_access()
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::end_access:")
                      ACE_TEXT(" Publisher is not enabled!\n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  if (qos_.presentation.access_scope != DDS::GROUP_PRESENTATION_QOS) {
    return DDS::RETCODE_OK;
  }

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   DDS::RETCODE_ERROR);

  if (this->access_depth_ == 0) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::end_access:")
                      ACE_TEXT(" No matching call to begin_coherent_changes!\n")),
                     DDS::RETCODE_PRECONDITION_NOT_MET);
  }

  --this->access_depth_;

  // We should only notify subscription on the first
  // and last change to the current change set:
  if (this->access_depth_ == 0) {
    for (DataReaderSet::iterator it = this->datareader_set_.begin();
      it != this->datareader_set_.end(); ++it) {
        (*it)->end_access();
    }
  }

  return DDS::RETCODE_OK;
}

DDS::DomainParticipant_ptr
SubscriberImpl::get_participant()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return DDS::DomainParticipant::_duplicate(participant_);
}

DDS::ReturnCode_t
SubscriberImpl::set_default_datareader_qos(
  const DDS::DataReaderQos & qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    default_datareader_qos_ = qos;
    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
SubscriberImpl::get_default_datareader_qos(
  DDS::DataReaderQos & qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  qos = default_datareader_qos_;
  return DDS::RETCODE_OK;
}

void
SubscriberImpl::get_default_datareader_qos_ext(
  DataReaderQosExt & qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  qos.durability.always_get_history = false;
}

DDS::ReturnCode_t
SubscriberImpl::copy_from_topic_qos(
  DDS::DataReaderQos & a_datareader_qos,
  const DDS::TopicQos & a_topic_qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (Qos_Helper::valid(a_topic_qos) &&
      Qos_Helper::consistent(a_topic_qos)) {
    // the caller can get the default before calling this
    // method if it wants to.
    //a_datareader_qos = this->default_datareader_qos_;
    a_datareader_qos.durability = a_topic_qos.durability;
    a_datareader_qos.deadline = a_topic_qos.deadline;
    a_datareader_qos.latency_budget = a_topic_qos.latency_budget;
    a_datareader_qos.liveliness = a_topic_qos.liveliness;
    a_datareader_qos.reliability = a_topic_qos.reliability;
    a_datareader_qos.destination_order = a_topic_qos.destination_order;
    a_datareader_qos.history = a_topic_qos.history;
    a_datareader_qos.resource_limits = a_topic_qos.resource_limits;
    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
SubscriberImpl::enable()
ACE_THROW_SPEC((CORBA::SystemException))
{
  //According spec:
  // - Calling enable on an already enabled Entity returns OK and has no
  // effect.
  // - Calling enable on an Entity whose factory is not enabled will fail
  // and return PRECONDITION_NOT_MET.

  if (this->is_enabled()) {
    return DDS::RETCODE_OK;
  }

  if (this->participant_->is_enabled() == false) {
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  if (this->monitor_) {
    this->monitor_->report();
  }

  this->set_enabled();
  return DDS::RETCODE_OK;
}

bool
SubscriberImpl::is_clean() const
{
  bool sub_is_clean = datareader_map_.empty();

  if (!sub_is_clean && !TheTransientKludge->is_enabled()) {
    // Four BIT datareaders.
    return datareader_map_.size() == 4;
  }

  return sub_is_clean;
}

void
SubscriberImpl::data_received(DataReaderImpl* reader)
{
  ACE_GUARD(ACE_Recursive_Thread_Mutex,
            guard,
            this->si_lock_);
  datareader_set_.insert(reader);
}

DDS::ReturnCode_t
SubscriberImpl::reader_enabled(const char* topic_name,
                               DataReaderImpl* reader)
{
  if (DCPS_debug_level >= 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) SubscriberImpl::reader_enabled, ")
               ACE_TEXT("datareader(topic_name=%C) enabled\n"),
               topic_name));
  }

  this->datareader_map_.insert(DataReaderMap::value_type(topic_name, reader));

  // Increase the ref count when the servant is referenced
  // by the datareader map.
  reader->_add_ref();

  if (this->monitor_) {
    this->monitor_->report();
  }

  return DDS::RETCODE_OK;
}

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
DDS::ReturnCode_t
SubscriberImpl::multitopic_reader_enabled(DDS::DataReader_ptr reader)
{
  DDS::TopicDescription_var td = reader->get_topicdescription();
  CORBA::String_var topic = td->get_name();
  multitopic_reader_map_[topic.in()] = DDS::DataReader::_duplicate(reader);
  return DDS::RETCODE_OK;
}
#endif

DDS::SubscriberListener*
SubscriberImpl::listener_for(::DDS::StatusKind kind)
{
  // per 2.1.4.3.1 Listener Access to Plain Communication Status
  // use this entities factory if listener is mask not enabled
  // for this kind.
  if (fast_listener_ == 0 || (listener_mask_ & kind) == 0) {
    return participant_->listener_for(kind);

  } else {
    return fast_listener_;
  }
}

unsigned int&
SubscriberImpl::raw_latency_buffer_size()
{
  return this->raw_latency_buffer_size_;
}

DataCollector<double>::OnFull&
SubscriberImpl::raw_latency_buffer_type()
{
  return this->raw_latency_buffer_type_;
}

void
SubscriberImpl::get_subscription_ids(SubscriptionIdVec& subs)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   );

  subs.reserve(datareader_map_.size());
  for (DataReaderMap::iterator iter = datareader_map_.begin();
       iter != datareader_map_.end();
       ++iter) {
    subs.push_back(iter->second->get_subscription_id());
  }
}

void
SubscriberImpl::update_ownership_strength (const PublicationId& pub_id,
                                  const CORBA::Long& ownership_strength)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->si_lock_,
                   );

  for (DataReaderMap::iterator iter = datareader_map_.begin();
       iter != datareader_map_.end();
       ++iter) {
    if (!iter->second->is_bit()) {
      iter->second->update_ownership_strength(pub_id, ownership_strength);
    }
  }
}


void
SubscriberImpl::coherent_change_received (RepoId& publisher_id,
                                          DataReaderImpl* reader,
                                          Coherent_State& group_state)
{
  // Verify if all readers complete the coherent changes. The result
  // is either COMPLETED or REJECTED.
  group_state = COMPLETED;
  DataReaderSet::const_iterator endIter = datareader_set_.end();
  for (DataReaderSet::const_iterator iter = datareader_set_.begin() ;
    iter != endIter ; ++iter) {

    Coherent_State state = COMPLETED;
    (*iter)->coherent_change_received (publisher_id, state);
    if (state == NOT_COMPLETED_YET) {
      group_state = state;
      return;
    }
    else if (state == REJECTED) {
      group_state = REJECTED;
    }
  }

  PublicationId writerId = GUID_UNKNOWN;
  for (DataReaderSet::const_iterator iter = datareader_set_.begin() ;
    iter != endIter ; ++iter) {
      if (group_state == COMPLETED) {
        (*iter)->accept_coherent (writerId, publisher_id);
      }
      else { //REJECTED
        (*iter)->reject_coherent (writerId, publisher_id);
      }
  }

  if (group_state == COMPLETED) {
    for (DataReaderSet::const_iterator iter = datareader_set_.begin() ;
      iter != endIter ; ++iter) {
      (*iter)->coherent_changes_completed (reader);
      (*iter)->reset_coherent_info (writerId, publisher_id);
    }
  }
}

EntityImpl*
SubscriberImpl::parent() const
{
  return this->participant_;
}

} // namespace DCPS
} // namespace OpenDDS

