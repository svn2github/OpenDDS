/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "DataWriterImpl.h"
#include "DomainParticipantImpl.h"
#include "PublisherImpl.h"
#include "Service_Participant.h"
#include "GuidConverter.h"
#include "TopicImpl.h"
#include "PublicationInstance.h"
#include "Serializer.h"
#include "Transient_Kludge.h"
#include "DataDurabilityCache.h"
#include "OfferedDeadlineWatchdog.h"
#include "MonitorFactory.h"
#include "CoherentChangeControl.h"
#include "DataWriterRemoteImpl.h"
#include "AssociationData.h"
#include "dds/DdsDcpsInfrastructureTypeSupportImpl.h"

#if !defined (DDS_HAS_MINIMUM_BIT)
#include "BuiltInTopicUtils.h"
#endif // !defined (DDS_HAS_MINIMUM_BIT)

#include "Util.h"
#include "dds/DCPS/transport/framework/EntryExit.h"
#include "dds/DCPS/transport/framework/TransportExceptions.h"

#include "tao/ORB_Core.h"
#include "ace/Reactor.h"
#include "ace/Auto_Ptr.h"

#include <sstream>
#include <stdexcept>

namespace OpenDDS {
namespace DCPS {

//TBD - add check for enabled in most methods.
//      currently this is not needed because auto_enable_created_entities
//      cannot be false.

DataWriterImpl::DataWriterImpl()
  : data_dropped_count_(0),
    data_delivered_count_(0),
    control_dropped_count_(0),
    control_delivered_count_(0),
    n_chunks_(TheServiceParticipant->n_chunks()),
    association_chunk_multiplier_(TheServiceParticipant->association_chunk_multiplier()),
    qos_(TheServiceParticipant->initial_DataWriterQos()),
    participant_servant_(0),
    topic_id_(GUID_UNKNOWN),
    topic_servant_(0),
    listener_mask_(DEFAULT_STATUS_MASK),
    fast_listener_(0),
    domain_id_(0),
    publisher_servant_(0),
    publication_id_(GUID_UNKNOWN),
    sequence_number_(SequenceNumber::SEQUENCENUMBER_UNKNOWN()),
    coherent_(false),
    coherent_samples_(0),
    data_container_(0),
    liveliness_lost_(false),
    mb_allocator_(0),
    db_allocator_(0),
    header_allocator_(0),
    reactor_(0),
    liveliness_check_interval_(ACE_Time_Value::zero),
    last_liveliness_activity_time_(ACE_Time_Value::zero),
    last_deadline_missed_total_count_(0),
    watchdog_(),
    cancel_timer_(false),
    is_bit_(false),
    initialized_(false),
    wfaCondition_(this->wfaLock_),
    monitor_(0),
    periodic_monitor_(0)
{
  liveliness_lost_status_.total_count = 0;
  liveliness_lost_status_.total_count_change = 0;

  offered_deadline_missed_status_.total_count = 0;
  offered_deadline_missed_status_.total_count_change = 0;
  offered_deadline_missed_status_.last_instance_handle = DDS::HANDLE_NIL;

  offered_incompatible_qos_status_.total_count = 0;
  offered_incompatible_qos_status_.total_count_change = 0;
  offered_incompatible_qos_status_.last_policy_id = 0;
  offered_incompatible_qos_status_.policies.length(0);

  publication_match_status_.total_count = 0;
  publication_match_status_.total_count_change = 0;
  publication_match_status_.current_count = 0;
  publication_match_status_.current_count_change = 0;
  publication_match_status_.last_subscription_handle = DDS::HANDLE_NIL;

  monitor_ =
    TheServiceParticipant->monitor_factory_->create_data_writer_monitor(this);
  periodic_monitor_ =
    TheServiceParticipant->monitor_factory_->create_data_writer_periodic_monitor(this);
}

// This method is called when there are no longer any reference to the
// the servant.
DataWriterImpl::~DataWriterImpl()
{
  DBG_ENTRY_LVL("DataWriterImpl","~DataWriterImpl",6);

  if (initialized_) {
    delete data_container_;
    delete mb_allocator_;
    delete db_allocator_;
    delete header_allocator_;
  }
}

// this method is called when delete_datawriter is called.
void
DataWriterImpl::cleanup()
{
  if (cancel_timer_) {
    // The cancel_timer will call handle_close to
    // remove_ref.
    (void) reactor_->cancel_timer(this, 0);
    cancel_timer_ = false;
  }

  // release our Topic_var
  topic_objref_ = DDS::Topic::_nil();
  topic_servant_->remove_entity_ref();
  topic_servant_->_remove_ref();
  topic_servant_ = 0;

  dw_local_objref_ = DDS::DataWriter::_nil();

  DataWriterRemoteImpl* dwr =
    remote_reference_to_servant<DataWriterRemoteImpl>(dw_remote_objref_.in());
  dwr->detach_parent();
  deactivate_remote_object(dw_remote_objref_.in());
  dw_remote_objref_ = DataWriterRemote::_nil();
}

void
DataWriterImpl::init(
  DDS::Topic_ptr                       topic,
  TopicImpl *                            topic_servant,
  const DDS::DataWriterQos &           qos,
  DDS::DataWriterListener_ptr          a_listener,
  const DDS::StatusMask &              mask,
  OpenDDS::DCPS::DomainParticipantImpl * participant_servant,
  OpenDDS::DCPS::PublisherImpl *         publisher_servant,
  DDS::DataWriter_ptr                  dw_local,
  OpenDDS::DCPS::DataWriterRemote_ptr    dw_remote)
ACE_THROW_SPEC((CORBA::SystemException))
{
  DBG_ENTRY_LVL("DataWriterImpl","init",6);
  topic_objref_ = DDS::Topic::_duplicate(topic);
  topic_servant_ = topic_servant;
  topic_servant_->_add_ref();
  topic_servant_->add_entity_ref();
  topic_name_    = topic_servant_->get_name();
  topic_id_      = topic_servant_->get_id();
  type_name_     = topic_servant_->get_type_name();

#if !defined (DDS_HAS_MINIMUM_BIT)
  is_bit_ = ACE_OS::strcmp(topic_name_.in(), BUILT_IN_PARTICIPANT_TOPIC) == 0
            || ACE_OS::strcmp(topic_name_.in(), BUILT_IN_TOPIC_TOPIC) == 0
            || ACE_OS::strcmp(topic_name_.in(), BUILT_IN_SUBSCRIPTION_TOPIC) == 0
            || ACE_OS::strcmp(topic_name_.in(), BUILT_IN_PUBLICATION_TOPIC) == 0;
#endif // !defined (DDS_HAS_MINIMUM_BIT)

  qos_ = qos;

  //Note: OK to _duplicate(nil).
  listener_ = DDS::DataWriterListener::_duplicate(a_listener);

  if (!CORBA::is_nil(listener_.in())) {
    fast_listener_ = listener_.in();
  }

  listener_mask_ = mask;

  // Only store the participant pointer, since it is our "grand"
  // parent, we will exist as long as it does.
  participant_servant_ = participant_servant;
  domain_id_ = participant_servant_->get_domain_id();

  // Only store the publisher pointer, since it is our parent, we will
  // exist as long as it does.
  publisher_servant_ = publisher_servant;
  dw_local_objref_   = DDS::DataWriter::_duplicate(dw_local);
  dw_remote_objref_  = OpenDDS::DCPS::DataWriterRemote::_duplicate(dw_remote);

  CORBA::ORB_var orb = TheServiceParticipant->get_ORB();
  this->reactor_ = orb->orb_core()->reactor();

  initialized_ = true;
}

DDS::InstanceHandle_t
DataWriterImpl::get_instance_handle()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return this->participant_servant_->get_handle(publication_id_);
}

DDS::InstanceHandle_t
DataWriterImpl::get_next_handle()
{
  return this->participant_servant_->get_handle();
}

void
DataWriterImpl::add_association(const RepoId& yourId,
                                const ReaderAssociation& reader,
                                bool active)
{
  DBG_ENTRY_LVL("DataWriterImpl", "add_association", 6);

  if (DCPS_debug_level >= 1) {
    GuidConverter writer_converter(yourId);
    GuidConverter reader_converter(reader.readerId);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DataWriterImpl::add_association - ")
               ACE_TEXT("bit %d local %C remote %C\n"),
               is_bit_,
               std::string(writer_converter).c_str(),
               std::string(reader_converter).c_str()));
  }

  if (entity_deleted_ == true) {
    if (DCPS_debug_level >= 1)
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DataWriterImpl::add_association")
                 ACE_TEXT(" This is a deleted datawriter, ignoring add.\n")));

    return;
  }

  if (GUID_UNKNOWN == publication_id_) {
    publication_id_ = yourId;
  }

  {
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, this->lock_);
    // Add to pending_readers_

    if (OpenDDS::DCPS::insert(pending_readers_, reader.readerId) == -1) {
      GuidConverter converter(reader.readerId);
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::add_association: ")
                 ACE_TEXT("failed to mark %C as pending.\n"),
                 std::string(converter).c_str()));

    } else {
      if (DCPS_debug_level > 0) {
        GuidConverter converter(reader.readerId);
        ACE_DEBUG((LM_DEBUG,
                   ACE_TEXT("(%P|%t) DataWriterImpl::add_association: ")
                   ACE_TEXT("marked %C as pending.\n"),
                   std::string(converter).c_str()));
      }
      reader_info_.insert(std::make_pair(reader.readerId,
        ReaderInfo(TheServiceParticipant->publisher_content_filter() ? reader.filterExpression : "",
          reader.exprParams, participant_servant_)));
    }
  }

  if (DCPS_debug_level > 4) {
    GuidConverter converter(get_publication_id());
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DataWriterImpl::add_association(): ")
               ACE_TEXT("adding subscription to publication %C with priority %d.\n"),
               std::string(converter).c_str(),
               qos_.transport_priority.value));
  }

  AssociationData data;
  data.remote_id_ = reader.readerId;
  data.remote_data_ = reader.readerTransInfo;
  data.remote_reliable_ =
    (reader.readerQos.reliability.kind == DDS::RELIABLE_RELIABILITY_QOS);

  if (!this->associate(data, active)) {
    //FUTURE: inform inforepo and try again as passive peer
    if (DCPS_debug_level) {
      ACE_DEBUG((LM_ERROR,
                ACE_TEXT("(%P|%t) DataWriterImpl::add_association: ")
                ACE_TEXT("ERROR: transport layer failed to associate.\n")));
    }
    return;
  }

  if (!active) {
    // In the current implementation, DataWriter is always active, so this
    // code will not be applicable.
    DCPSInfo_var repo = TheServiceParticipant->get_repository(this->domain_id_);
    try {
      repo->association_complete(this->domain_id_,
        this->participant_servant_->get_id(),
        this->publication_id_, reader.readerId);
    } catch (const CORBA::Exception& e) {
      e._tao_print_exception("ERROR: Exception from DCPSInfo::"
        "association_complete");
    }
  }
}


DataWriterImpl::ReaderInfo::ReaderInfo(const char* filter,
                                       const DDS::StringSeq& params,
                                       DomainParticipantImpl* participant)
#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  : participant_(participant)
  , expression_params_(params)
  , filter_(filter)
  , eval_(*filter ? participant->get_filter_eval(filter) : 0)
  , expected_sequence_(SequenceNumber::SEQUENCENUMBER_UNKNOWN())
{}
#else
  : expected_sequence_(SequenceNumber::SEQUENCENUMBER_UNKNOWN())
{
  ACE_UNUSED_ARG(filter);
  ACE_UNUSED_ARG(params);
  ACE_UNUSED_ARG(participant);
}
#endif // OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE

DataWriterImpl::ReaderInfo::~ReaderInfo()
{
#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  eval_ = RcHandle<FilterEvaluator>();
  if (!filter_.empty()) {
    participant_->deref_filter_eval(filter_.c_str());
  }
#endif // OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
}


void
DataWriterImpl::association_complete(const RepoId& remote_id)
{
  DBG_ENTRY_LVL("DataWriterImpl", "association_complete", 6);

  if (DCPS_debug_level >= 1) {
    GuidConverter writer_converter(this->publication_id_);
    GuidConverter reader_converter(remote_id);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DataWriterImpl::association_complete - ")
               ACE_TEXT("bit %d local %C remote %C\n"),
               is_bit_,
               std::string(writer_converter).c_str(),
               std::string(reader_converter).c_str()));
  }

  {
    // protect readers_
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, this->lock_);

    // If the reader is not in pending association list, which indicates it's already
    // removed by remove_association. In other words, the remove_association()
    // is called before assocation_complete() call.
    if (OpenDDS::DCPS::remove(pending_readers_, remote_id) == -1) {
      GuidConverter converter(remote_id);
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DataWriterImpl::association_complete: ")
                 ACE_TEXT("reader %C is not in pending list ")
                 ACE_TEXT("because remove_association is already called.\n"),
                 std::string(converter).c_str()));
      return;
    }

    // The reader is in the pending reader, now add it to fully associated reader
    // list.

    if (OpenDDS::DCPS::insert(readers_, remote_id) == -1) {
      GuidConverter converter(remote_id);
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::association_complete: ")
                 ACE_TEXT("insert %C from pending failed.\n"),
                 std::string(converter).c_str()));
    }
  }

  if (this->monitor_) {
    this->monitor_->report();
  }

  if (!is_bit_) {

    DDS::InstanceHandle_t handle =
      this->participant_servant_->get_handle(remote_id);

    {
      // protect publication_match_status_ and status changed flags.
      ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, this->lock_);

      // update the publication_match_status_
      ++publication_match_status_.total_count;
      ++publication_match_status_.total_count_change;
      ++publication_match_status_.current_count;
      ++publication_match_status_.current_count_change;

      if (OpenDDS::DCPS::bind(id_to_handle_map_, remote_id, handle) != 0) {
        GuidConverter converter(remote_id);
        ACE_DEBUG((LM_WARNING,
                   ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::association_complete: ")
                   ACE_TEXT("id_to_handle_map_%C = 0x%x failed.\n"),
                   std::string(converter).c_str(),
                   handle));
        return;

      } else if (DCPS_debug_level > 4) {
        GuidConverter converter(remote_id);
        ACE_DEBUG((LM_WARNING,
                   ACE_TEXT("(%P|%t) DataWriterImpl::association_complete: ")
                   ACE_TEXT("id_to_handle_map_%C = 0x%x.\n"),
                   std::string(converter).c_str(),
                   handle));
      }

      publication_match_status_.last_subscription_handle = handle;

      set_status_changed_flag(::DDS::PUBLICATION_MATCHED_STATUS, true);
    }

    DDS::DataWriterListener* listener =
      listener_for(::DDS::PUBLICATION_MATCHED_STATUS);

    if (listener != 0) {
      listener->on_publication_matched(dw_local_objref_.in(),
                                       publication_match_status_);

      // TBD - why does the spec say to change this but not
      // change the ChangeFlagStatus after a listener call?
      publication_match_status_.total_count_change = 0;
      publication_match_status_.current_count_change = 0;
    }

    notify_status_condition();
  }

  // Support DURABILITY QoS
  if (this->qos_.durability.kind > DDS::VOLATILE_DURABILITY_QOS) {
    // Tell the WriteDataContainer to resend all sending/sent
    // samples.
    ReaderIdSeq rd_ids(1);
    rd_ids.length(1);
    rd_ids[0] = remote_id;
    this->data_container_->reenqueue_all(rd_ids, this->qos_.lifespan);

    // Acquire the data writer container lock to avoid deadlock. The
    // thread calling association_complete() has to acquire lock in the
    // same order as the write()/register() operation.

    // Since the thread calling association_complete() is the ORB
    // thread, it may have some performance penalty. If the
    // performance is an issue, we may need a new thread to handle the
    // data_available() calls.
    ACE_GUARD(ACE_Recursive_Thread_Mutex,
              guard,
              this->get_lock());

    DataSampleList list = data_container_->get_resend_data();

    // Assign new sequence numbers to the durable samples
    for (DataSampleListElement* list_el = list.head_;
         list_el != NULL;
         list_el = list_el->next_send_sample_) {
      // Durable data gets a new sequence number
      if (this->sequence_number_ == SequenceNumber::SEQUENCENUMBER_UNKNOWN()) {
        this->sequence_number_ = SequenceNumber();
      } else {
        ++this->sequence_number_;
      }
      // Reassign list element header sequence
      list_el->originalSequence_ = list_el->header_.sequence_;
      list_el->header_.sequence_ = sequence_number_;
      list_el->header_.historic_sample_ = true;
      // Reform sequence number in blob
      list_el->sample_->wr_ptr(list_el->sample_->base());
      if (!(*list_el->sample_ << list_el->header_)) {
        ACE_DEBUG((LM_ERROR,
              ACE_TEXT("(%P|%t) DataWriterImpl::association_complete - ")
              ACE_TEXT(" failed to serialize header for historic sample\n")));
      }
    }

    // Update the reader's expected sequence
    reader_info_.find(remote_id)->second.expected_sequence_ = sequence_number_;

    if (this->publisher_servant_->is_suspended()) {
      this->available_data_list_.enqueue_tail_next_send_sample(list);
    } else {
      this->send(list);
    }
  }
}

void
DataWriterImpl::remove_associations(const ReaderIdSeq & readers,
                                    CORBA::Boolean notify_lost)
{
  if (DCPS_debug_level >= 1) {
    GuidConverter writer_converter(publication_id_);
    GuidConverter reader_converter(readers[0]);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DataWriterImpl::remove_associations: ")
               ACE_TEXT("bit %d local %C remote %C num remotes %d\n"),
               is_bit_,
               std::string(writer_converter).c_str(),
               std::string(reader_converter).c_str(),
               readers.length()));
  }

  ReaderIdSeq fully_associated_readers;
  CORBA::ULong fully_associated_len = 0;
  ReaderIdSeq rds;
  CORBA::ULong rds_len = 0;
  DDS::InstanceHandleSeq handles;

  {
    // Ensure the same acquisition order as in wait_for_acknowledgments().
    ACE_GUARD(ACE_SYNCH_MUTEX, wfaGuard, this->wfaLock_);
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, this->lock_);

    //Remove the readers from fully associated reader list.
    //If the supplied reader is not in the cached reader list then it is
    //already removed. We just need remove the readers in the list that have
    //not been removed.

    CORBA::ULong len = readers.length();

    for (CORBA::ULong i = 0; i < len; ++i) {
      //Remove the readers from fully associated reader list. If it's not
      //in there, the association_complete() is not called yet and remove it
      //from pending list.

      if (OpenDDS::DCPS::remove(readers_, readers[i]) == 0) {
        ++ fully_associated_len;
        fully_associated_readers.length(fully_associated_len);
        fully_associated_readers [fully_associated_len - 1] = readers[i];

        // Remove this reader from the ACK sequence map if its there.
        // This is where we need to be holding the wfaLock_ obtained
        // above.
        RepoIdToSequenceMap::iterator where
        = this->idToSequence_.find(readers[i]);

        if (where != this->idToSequence_.end()) {
          this->idToSequence_.erase(where);

          // It is possible that this subscription was causing the wait
          // to continue, so give the opportunity to find out.
          this->wfaCondition_.broadcast();
        }

        ++ rds_len;
        rds.length(rds_len);
        rds [rds_len - 1] = readers[i];

      } else if (OpenDDS::DCPS::remove(pending_readers_, readers[i]) == 0) {
        ++ rds_len;
        rds.length(rds_len);
        rds [rds_len - 1] = readers[i];

        GuidConverter converter(readers[i]);
        ACE_DEBUG((LM_WARNING,
                  ACE_TEXT("(%P|%t) WARNING: DataWriterImpl::remove_associations: ")
                  ACE_TEXT("removing reader %C before association_complete() call.\n"),
                  std::string(converter).c_str()));
      }
      reader_info_.erase(readers[i]);
      //else reader is already removed which indicates remove_association()
      //is called multiple times.
    }

    if (fully_associated_len > 0 && !is_bit_) {
      // The reader should be in the id_to_handle map at this time so
      // log with error.
      if (this->lookup_instance_handles(fully_associated_readers, handles) == false) {
        ACE_ERROR((LM_ERROR, "(%P|%t) ERROR: DataWriterImpl::remove_associations: "
                  "lookup_instance_handles failed, notify %d \n", notify_lost));
        return;
      }

      for (CORBA::ULong i = 0; i < fully_associated_len; ++i) {
        id_to_handle_map_.erase(fully_associated_readers[i]);
      }
    }

    wfaGuard.release();

    // Mirror the PUBLICATION_MATCHED_STATUS processing from
    // association_complete() here.
    if (!this->is_bit_) {

      // Derive the change in the number of subscriptions reading this writer.
      int matchedSubscriptions =
        static_cast<int>(this->id_to_handle_map_.size());
      this->publication_match_status_.current_count_change =
        matchedSubscriptions - this->publication_match_status_.current_count;

      // Only process status if the number of subscriptions has changed.
      if (this->publication_match_status_.current_count_change != 0) {
        this->publication_match_status_.current_count = matchedSubscriptions;

        /// Section 7.1.4.1: total_count will not decrement.

        /// @TODO: Reconcile this with the verbiage in section 7.1.4.1
        this->publication_match_status_.last_subscription_handle =
          handles[rds_len - 1];

        set_status_changed_flag(::DDS::PUBLICATION_MATCHED_STATUS, true);

        DDS::DataWriterListener* listener =
         this->listener_for(::DDS::SUBSCRIPTION_MATCHED_STATUS);

        if (listener != 0) {
          listener->on_publication_matched(
            this->dw_local_objref_.in(),
            this->publication_match_status_);

          // Listener consumes the change.
          this->publication_match_status_.total_count_change = 0;
          this->publication_match_status_.current_count_change = 0;
        }

        this->notify_status_condition();
      }
    }
  }

  for (CORBA::ULong i = 0; i < rds.length(); ++i) {
    this->disassociate(rds[i]);
  }

  // If this remove_association is invoked when the InfoRepo
  // detects a lost reader then make a callback to notify
  // subscription lost.
  if (notify_lost && handles.length() > 0) {
    this->notify_publication_lost(handles);
  }
}

void DataWriterImpl::remove_all_associations()
{
  OpenDDS::DCPS::ReaderIdSeq readers;
  CORBA::ULong size;
  CORBA::ULong num_pending_readers;
  {
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, lock_);

    num_pending_readers = static_cast<CORBA::ULong>(pending_readers_.size());
    size = static_cast<CORBA::ULong>(readers_.size()) + num_pending_readers;
    readers.length(size);

    IdSet::iterator itEnd = readers_.end();
    int i = 0;

    for (IdSet::iterator it = readers_.begin(); it != itEnd; ++it) {
      readers[i ++] = *it;
    }

    itEnd = pending_readers_.end();
    for (IdSet::iterator it = pending_readers_.begin(); it != itEnd; ++it) {
      readers[i ++] = *it;
    }

    if (num_pending_readers > 0) {
      ACE_DEBUG((LM_WARNING,
                 ACE_TEXT("(%P|%t) WARNING: DataWriterImpl::remove_all_associations() - ")
                 ACE_TEXT("%d subscribers were pending and never fully associated.\n"),
                 num_pending_readers));
    }
  }

  try {
    if (0 < size) {
      CORBA::Boolean dont_notify_lost = false;
      this->remove_associations(readers, dont_notify_lost);
    }

  } catch (const CORBA::Exception&) {
  }
}

void
DataWriterImpl::update_incompatible_qos(const IncompatibleQosStatus& status)
{
  DDS::DataWriterListener* listener =
    listener_for(::DDS::OFFERED_INCOMPATIBLE_QOS_STATUS);

  ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, this->lock_);

#if 0

  if (this->offered_incompatible_qos_status_.total_count == status.total_count) {
    // This test should make the method idempotent.
    return;
  }

#endif

  set_status_changed_flag(::DDS::OFFERED_INCOMPATIBLE_QOS_STATUS, true);

  // copy status and increment change
  offered_incompatible_qos_status_.total_count = status.total_count;
  offered_incompatible_qos_status_.total_count_change +=
    status.count_since_last_send;
  offered_incompatible_qos_status_.last_policy_id = status.last_policy_id;
  offered_incompatible_qos_status_.policies = status.policies;

  if (listener != 0) {
    listener->on_offered_incompatible_qos(dw_local_objref_.in(),
                                          offered_incompatible_qos_status_);

    // TBD - Why does the spec say to change this but not change the
    //       ChangeFlagStatus after a listener call?
    offered_incompatible_qos_status_.total_count_change = 0;
  }

  notify_status_condition();
}

void
DataWriterImpl::update_subscription_params(const RepoId& readerId,
                                           const DDS::StringSeq& params)
{
#ifdef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  ACE_UNUSED_ARG(readerId);
  ACE_UNUSED_ARG(params);
#else
  ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, this->lock_);
  RepoIdToReaderInfoMap::iterator iter = reader_info_.find(readerId);
  if (iter != reader_info_.end()) {
    iter->second.expression_params_ = params;
  } else if (DCPS_debug_level > 4 &&
             TheServiceParticipant->publisher_content_filter()) {
    GuidConverter pubConv(this->publication_id_), subConv(readerId);
    ACE_DEBUG((LM_WARNING,
      ACE_TEXT("(%P|%t) WARNING: DataWriterImpl::update_subscription_params()")
      ACE_TEXT(" - writer: %C has no info about reader: %C\n"),
      std::string(pubConv).c_str(), std::string(subConv).c_str()));
  }
#endif
}

DDS::ReturnCode_t
DataWriterImpl::set_qos(const DDS::DataWriterQos & qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    if (qos_ == qos)
      return DDS::RETCODE_OK;

    if (!Qos_Helper::changeable(qos_, qos) && enabled_ == true) {
      return DDS::RETCODE_IMMUTABLE_POLICY;

    } else {
      try {
        DCPSInfo_var repo = TheServiceParticipant->get_repository(domain_id_);
        DDS::PublisherQos publisherQos;
        this->publisher_servant_->get_qos(publisherQos);
        CORBA::Boolean status
        = repo->update_publication_qos(this->participant_servant_->get_domain_id(),
                                       this->participant_servant_->get_id(),
                                       this->publication_id_,
                                       qos,
                                       publisherQos);

        if (status == 0) {
          ACE_ERROR_RETURN((LM_ERROR,
                            ACE_TEXT("(%P|%t) DataWriterImpl::set_qos, ")
                            ACE_TEXT("qos is not compatible. \n")),
                           DDS::RETCODE_ERROR);
        }

      } catch (const CORBA::SystemException& sysex) {
        sysex._tao_print_exception(
          "ERROR: System Exception"
          " in DataWriterImpl::set_qos");
        return DDS::RETCODE_ERROR;

      } catch (const CORBA::UserException& userex) {
        userex._tao_print_exception(
          "ERROR:  Exception"
          " in DataWriterImpl::set_qos");
        return DDS::RETCODE_ERROR;
      }
    }

    if (!(qos_ == qos)) {
      // Reset the deadline timer if the period has changed.
      if (qos_.deadline.period.sec != qos.deadline.period.sec
          || qos_.deadline.period.nanosec != qos.deadline.period.nanosec) {
        if (qos_.deadline.period.sec == DDS::DURATION_INFINITE_SEC
            && qos_.deadline.period.nanosec == DDS::DURATION_INFINITE_NSEC) {
          ACE_auto_ptr_reset(this->watchdog_,
                             new OfferedDeadlineWatchdog(
                               this->reactor_,
                               this->lock_,
                               qos.deadline,
                               this,
                               this->dw_local_objref_.in(),
                               this->offered_deadline_missed_status_,
                               this->last_deadline_missed_total_count_));

        } else if (qos.deadline.period.sec == DDS::DURATION_INFINITE_SEC
                   && qos.deadline.period.nanosec == DDS::DURATION_INFINITE_NSEC) {
          this->watchdog_->cancel_all();
          this->watchdog_.reset();

        } else {
          this->watchdog_->reset_interval(
            duration_to_time_value(qos.deadline.period));
        }
      }

      qos_ = qos;
    }

    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
DataWriterImpl::get_qos(::DDS::DataWriterQos & qos)
ACE_THROW_SPEC((CORBA::SystemException))
{
  qos = qos_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DataWriterImpl::set_listener(::DDS::DataWriterListener_ptr a_listener,
                             DDS::StatusMask mask)
ACE_THROW_SPEC((CORBA::SystemException))
{
  listener_mask_ = mask;
  //note: OK to duplicate  a nil object ref
  listener_ = DDS::DataWriterListener::_duplicate(a_listener);
  fast_listener_ = listener_.in();
  return DDS::RETCODE_OK;
}

DDS::DataWriterListener_ptr
DataWriterImpl::get_listener()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return DDS::DataWriterListener::_duplicate(listener_.in());
}

DDS::Topic_ptr
DataWriterImpl::get_topic()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return DDS::Topic::_duplicate(topic_objref_.in());
}

bool
DataWriterImpl::should_ack() const
{
  // N.B. It may be worthwhile to investigate a more efficient
  // heuristic for determining if a writer should send SAMPLE_ACK
  // control samples. Perhaps based on a sequence number delta?
  return this->readers_.size() != 0;
}

DataWriterImpl::AckToken
DataWriterImpl::create_ack_token(DDS::Duration_t max_wait) const
{
  return AckToken(max_wait, this->sequence_number_);
}

SequenceNumber
DataWriterImpl::AckToken::expected(const RepoId& subscriber) const
{
  RepoIdToSequenceMap::const_iterator found = this->custom_.find(subscriber);
  return (found == this->custom_.end()) ? this->sequence_ : found->second;
}

bool
DataWriterImpl::AckToken::marshal(ACE_Message_Block*& mblock, bool swap) const
{
  size_t dataSize = 0, padding = 0;
  gen_find_size(sequence_, dataSize, padding);
  gen_find_size(max_wait_, dataSize, padding);

  ACE_NEW_RETURN(mblock, ACE_Message_Block(dataSize), false);

  Serializer ser(mblock, swap);
  ser << sequence_;
  ser << max_wait_;
  return ser.good_bit();
}

DDS::ReturnCode_t
DataWriterImpl::send_ack_requests(DataWriterImpl::AckToken& token)
{
  AckCustomization ac(token);
  for (RepoIdToReaderInfoMap::iterator iter = reader_info_.begin(),
       end = reader_info_.end(); iter != end; ++iter) {
    if (iter->second.expected_sequence_ != token.sequence_) {
      push_back(ac.customized_, iter->first);
      token.custom_[iter->first] = iter->second.expected_sequence_;
    }
  }

  ACE_Message_Block* data;
  if (!token.marshal(data, this->swap_bytes())) {
    return DDS::RETCODE_OUT_OF_RESOURCES;
  }

  if (DCPS_debug_level > 0) {
    GuidConverter converter(this->publication_id_);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DataWriterImpl::send_ack_requests() - ")
               ACE_TEXT("%C sending REQUEST_ACK message for sequence %q ")
               ACE_TEXT("to %d subscriptions.\n"),
               std::string(converter).c_str(),
               token.sequence_.getValue(),
               this->readers_.size()));
  }

  DataSampleHeader header;
  ACE_Message_Block* ack_request =
    this->create_control_message(REQUEST_ACK, header, data, token.timestamp());

  const SendControlStatus status =
    this->send_control(header, ack_request, ac.customized_.length() ? &ac : 0);

  if (status == SEND_CONTROL_ERROR) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::send_ack_requests() - ")
               ACE_TEXT("failed to send REQUEST_ACK message. \n")));
    return DDS::RETCODE_ERROR;
  }

  return DDS::RETCODE_OK;
}

SendControlStatus
DataWriterImpl::send_control_customized(const DataLinkSet_rch&  links,
                                        const DataSampleHeader& header,
                                        ACE_Message_Block*      msg,
                                        void*                   extra)
{
  AckCustomization& ac = *static_cast<AckCustomization*>(extra);
  const bool swap = this->swap_bytes();

  // set of DataLinks that have no targets that need custom control messages
  DataLinkSet notCustomized;
  bool ok = true;

  typedef std::map<SequenceNumber, GUIDSeq> GUIDSeqMap;

  DataLinkSet::GuardType guard(links->lock());

  // For each data link in "links"...
  for (DataLinkSet::MapType::iterator iter = links->map().begin(),
       end = links->map().end(); iter != end; ++iter) {

    const DataLink_rch& datalink = iter->second;
    GUIDSeq_var intersect = datalink->target_intersection(ac.customized_);
    // "intersect" is the list of GUIDs for the subs on this link that will
    // need to get a customized REQUEST_ACK message.

    if (intersect.ptr() == 0 || intersect->length() == 0) {
      // This link requires no customization, add to the notCustomized list
      // so we can handle it after the big for loop.
      notCustomized.insert_link(datalink.in());

    } else {
      DataLinkSet thisLink; // need a "set" with only the current link in it
      thisLink.insert_link(datalink.in());
      RepoIdSet_rch allTargets = datalink->get_targets();
      RepoIdSet::MapType noCustTargets = allTargets->map(); // copied

      // Each sequence number may go to > 1 target on this link, build the
      // STL map "gsm" keyed on the sequence number.
      GUIDSeqMap gsm;
      const CORBA::ULong len = intersect->length();
      for (CORBA::ULong i = 0; i < len; ++i) {
        const GUID_t& target = intersect[i];
        push_back(gsm[ac.token_.custom_[target]], target);
        noCustTargets.erase(target);
      }

      // Some targets don't require customization, but do need to see the
      // original sequence number from the AckToken
      for (RepoIdSet::MapType::iterator nct_iter(noCustTargets.begin()),
           nct_end(noCustTargets.end()); nct_iter != nct_end; ++nct_iter) {
        push_back(gsm[ac.token_.sequence_], nct_iter->first);
      }

      // For each sequence number that needs to go to this link:
      for (GUIDSeqMap::iterator it2 = gsm.begin(), it2_end = gsm.end();
           it2 != it2_end; ++it2) {

        const SequenceNumber& seq = it2->first;
        const GUIDSeq& targets = it2->second;

        // Deep copy the payload and chain it to a shallow-copied header
        ACE_Message_Block* data_orig = msg->cont();
        msg->cont(0); // temporarily unlink so we don't duplicate the data
        ACE_Message_Block* header_mb = msg->duplicate();
        ACE_Message_Block* data_modified = data_orig->clone();
        header_mb->cont(data_modified);
        msg->cont(data_orig);                 // undo the temporary unlink

        // Modify the payload to contain the customized sequence #
        char* wr = data_modified->wr_ptr();
        data_modified->wr_ptr(data_modified->base());   // rewind
        Serializer ser(data_modified, swap);
        ser << seq;                                     // overwrite Seq#
        data_modified->wr_ptr(wr);                      // wind

        // If other targets are listening on the DataLink, filter those out
        // using the CONTENT_FILTER_FLAG and its optional header.
        if (allTargets->size() != targets.length()) {
          RepoIdSet::MapType filterTargets = allTargets->map(); // copied
          for (CORBA::ULong i = 0, leng = targets.length(); i < leng; ++i) {
            filterTargets.erase(targets[i]);
          }
          GUIDSeq ftseq(static_cast<CORBA::ULong>(filterTargets.size()));
          for (RepoIdSet::MapType::iterator ris_iter(filterTargets.begin()),
               ris_end(filterTargets.end()); ris_iter != ris_end; ++ris_iter) {
            push_back(ftseq, ris_iter->first);
          }

          // Now changing the header, need to copy its data contents
          size_t len = header_mb->length();
          header_mb->data_block(header_mb->data_block()->clone());
          header_mb->wr_ptr(len);
          DataSampleHeader::set_flag(CONTENT_FILTER_FLAG, header_mb);
          DataSampleHeader::add_cfentries(&ftseq, header_mb);
        }

        if (DCPS_debug_level > 4) {
          ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("(%P|%t) DataWriterImpl::send_control_customized() - ")
            ACE_TEXT("sending REQUEST_ACK %q to single data link with%C ")
            ACE_TEXT("content filtering.\n"), seq.getValue(),
            (allTargets->size() == targets.length()) ? "out" : ""));
        }

        ok &= (thisLink.send_control(this->publication_id_, this, header, header_mb)
               == SEND_CONTROL_OK);
      }
    }
  }

  if (notCustomized.empty()) {
    msg->release();
  } else {
    if (DCPS_debug_level > 4) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DataWriterImpl::send_control_customized() - ")
        ACE_TEXT("sending REQUEST_ACK to %d non-customized data links.\n"),
        notCustomized.map().size()));
    }

    ok &= (notCustomized.send_control(this->publication_id_, this, header, msg)
           == SEND_CONTROL_OK);
  }

  return ok ? SEND_CONTROL_OK : SEND_CONTROL_ERROR;
}

DDS::ReturnCode_t
DataWriterImpl::wait_for_ack_responses(const DataWriterImpl::AckToken& token)
{
  ACE_Time_Value deadline(token.deadline());

  // Protect the wait-fer-acks blocking.
  ACE_GUARD_RETURN(
    ACE_SYNCH_MUTEX,
    wfaGuard,
    this->wfaLock_,
    DDS::RETCODE_ERROR);

  do {
    // We use the values from the set of readers to index into the sequence
    // map.  Any readers_ that have not responded will have the default,
    // smallest, value which will cause the wait to continue.
    bool done = true;

    // Protect the readers_ set.
    {
      ACE_GUARD_RETURN(
        ACE_Recursive_Thread_Mutex,
        readersGuard,
        this->lock_,
        DDS::RETCODE_ERROR);

      for (IdSet::const_iterator current = this->readers_.begin();
           current != this->readers_.end();
           ++current) {
        if (this->idToSequence_[*current] < token.expected(*current)) {
          done = false;
          if (DCPS_debug_level > 0) {
            GuidConverter conv(*current);
            ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t) DataWriterImpl::wait_for_ack_responses() - ")
              ACE_TEXT("waiting for seq %q from sub %C, got %q\n"),
              token.expected(*current).getValue(), std::string(conv).c_str(),
              this->idToSequence_[*current].getValue()));
          }
          break;
        }
      }
    }

    if (done) {
      if (DCPS_debug_level > 0) {
        GuidConverter converter(this->publication_id_);
        ACE_DEBUG((LM_DEBUG,
                   ACE_TEXT("(%P|%t) DataWriterImpl::wait_for_ack_responses() - ")
                   ACE_TEXT("%C unblocking for sequence %q.\n"),
                   std::string(converter).c_str(),
                   token.sequence_.getValue()));
      }

      return DDS::RETCODE_OK;
    }

  } while (0 == this->wfaCondition_.wait(&deadline));

  GuidConverter converter(this->publication_id_);
  ACE_DEBUG((LM_WARNING,
             ACE_TEXT("(%P|%t) WARNING: DataWriterImpl::wait_for_ack_responses() - ")
             ACE_TEXT("%C timed out waiting for sequence %q to be acknowledged ")
             ACE_TEXT("from %d subscriptions.\n"),
             std::string(converter).c_str(),
             token.sequence_.getValue(),
             this->readers_.size()));
  return DDS::RETCODE_TIMEOUT;
}

DDS::ReturnCode_t
DataWriterImpl::wait_for_acknowledgments(const DDS::Duration_t& max_wait)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (!should_ack()) {
    if (DCPS_debug_level > 0) {
      GuidConverter converter(this->publication_id_);
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DataWriterImpl::wait_for_acknowledgments() - ")
                 ACE_TEXT("%C not blocking due to no associated subscriptions.\n"),
                 std::string(converter).c_str()));
    }

    return DDS::RETCODE_OK;
  }

  AckToken token(create_ack_token(max_wait));

  DDS::ReturnCode_t error;

  if ((error = send_ack_requests(token)) != DDS::RETCODE_OK) return error;

  if ((error = wait_for_ack_responses(token)) != DDS::RETCODE_OK) return error;

  return DDS::RETCODE_OK;
}

DDS::Publisher_ptr
DataWriterImpl::get_publisher()
ACE_THROW_SPEC((CORBA::SystemException))
{
  return DDS::Publisher::_duplicate(publisher_servant_);
}

DDS::ReturnCode_t
DataWriterImpl::get_liveliness_lost_status(
  DDS::LivelinessLostStatus & status)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->lock_,
                   DDS::RETCODE_ERROR);
  set_status_changed_flag(::DDS::LIVELINESS_LOST_STATUS, false);
  status = liveliness_lost_status_;
  liveliness_lost_status_.total_count_change = 0;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DataWriterImpl::get_offered_deadline_missed_status(
  DDS::OfferedDeadlineMissedStatus & status)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->lock_,
                   DDS::RETCODE_ERROR);

  set_status_changed_flag(::DDS::OFFERED_DEADLINE_MISSED_STATUS, false);

  this->offered_deadline_missed_status_.total_count_change =
    this->offered_deadline_missed_status_.total_count
    - this->last_deadline_missed_total_count_;

  // Update for next status check.
  this->last_deadline_missed_total_count_ =
    this->offered_deadline_missed_status_.total_count;

  status = offered_deadline_missed_status_;

  this->offered_deadline_missed_status_.total_count_change = 0;

  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DataWriterImpl::get_offered_incompatible_qos_status(
  DDS::OfferedIncompatibleQosStatus & status)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->lock_,
                   DDS::RETCODE_ERROR);
  set_status_changed_flag(::DDS::OFFERED_INCOMPATIBLE_QOS_STATUS, false);
  status = offered_incompatible_qos_status_;
  offered_incompatible_qos_status_.total_count_change = 0;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DataWriterImpl::get_publication_matched_status(
  DDS::PublicationMatchedStatus & status)
ACE_THROW_SPEC((CORBA::SystemException))
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->lock_,
                   DDS::RETCODE_ERROR);
  set_status_changed_flag(::DDS::PUBLICATION_MATCHED_STATUS, false);
  status = publication_match_status_;
  publication_match_status_.total_count_change = 0;
  publication_match_status_.current_count_change = 0;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DataWriterImpl::assert_liveliness()
ACE_THROW_SPEC((CORBA::SystemException))
{
  // This operation need only be used if the LIVELINESS setting
  // is either MANUAL_BY_PARTICIPANT or MANUAL_BY_TOPIC.
  // Otherwise, it has no effect.

  if (this->qos_.liveliness.kind == DDS::AUTOMATIC_LIVELINESS_QOS) {
    return DDS::RETCODE_OK;
  }

  ACE_Time_Value now = ACE_OS::gettimeofday();

  if (last_liveliness_activity_time_ == ACE_Time_Value::zero
      || now - last_liveliness_activity_time_ >= liveliness_check_interval_) {
    //Not recent enough then send liveliness message.
    if (DCPS_debug_level > 9) {
      GuidConverter converter(publication_id_);
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DataWriterImpl::assert_liveliness: ")
                 ACE_TEXT("%C sending LIVELINESS message.\n"),
                 std::string(converter).c_str()));
    }

    if (this->send_liveliness(now) == false)
      return DDS::RETCODE_ERROR;
  }

  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DataWriterImpl::assert_liveliness_by_participant()
{
  // This operation is called by participant.

  if (this->qos_.liveliness.kind != DDS::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS) {
    return DDS::RETCODE_OK;
  }

  return this->assert_liveliness();
}

DDS::ReturnCode_t
DataWriterImpl::get_matched_subscriptions(
  DDS::InstanceHandleSeq & subscription_handles)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DataWriterImpl::get_matched_subscriptions: ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->lock_,
                   DDS::RETCODE_ERROR);

  // Copy out the handles for the current set of subscriptions.
  int index = 0;
  subscription_handles.length(
    static_cast<CORBA::ULong>(this->id_to_handle_map_.size()));

  for (RepoIdToHandleMap::iterator
       current = this->id_to_handle_map_.begin();
       current != this->id_to_handle_map_.end();
       ++current, ++index) {
    subscription_handles[index] = current->second;
  }

  return DDS::RETCODE_OK;
}

#if !defined (DDS_HAS_MINIMUM_BIT)
DDS::ReturnCode_t
DataWriterImpl::get_matched_subscription_data(
  DDS::SubscriptionBuiltinTopicData & subscription_data,
  DDS::InstanceHandle_t subscription_handle)
ACE_THROW_SPEC((CORBA::SystemException))
{
  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::")
                      ACE_TEXT("get_matched_subscription_data: ")
                      ACE_TEXT("Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  BIT_Helper_1 < DDS::SubscriptionBuiltinTopicDataDataReader,
  DDS::SubscriptionBuiltinTopicDataDataReader_var,
  DDS::SubscriptionBuiltinTopicDataSeq > hh;

  DDS::SubscriptionBuiltinTopicDataSeq data;

  DDS::ReturnCode_t ret =
    hh.instance_handle_to_bit_data(participant_servant_,
                                   BUILT_IN_SUBSCRIPTION_TOPIC,
                                   subscription_handle,
                                   data);

  if (ret == DDS::RETCODE_OK) {
    subscription_data = data[0];
  }

  return ret;
}
#endif // !defined (DDS_HAS_MINIMUM_BIT)

DDS::ReturnCode_t
DataWriterImpl::enable()
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

  if (this->publisher_servant_->is_enabled() == false) {
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  // Note: do configuration based on QoS in enable() because
  //       before enable is called the QoS can be changed -- even
  //       for Changeable=NO

  // Configure WriteDataContainer constructor parameters from qos.

  const bool reliable = qos_.reliability.kind == DDS::RELIABLE_RELIABILITY_QOS,
    should_block = reliable && qos_.history.kind == DDS::KEEP_ALL_HISTORY_QOS;

  CORBA::Long const depth =
    get_instance_sample_list_depth(
      qos_.history.kind,
      qos_.history.depth,
      qos_.resource_limits.max_samples_per_instance);

  if (qos_.resource_limits.max_samples != DDS::LENGTH_UNLIMITED) {
    n_chunks_ = qos_.resource_limits.max_samples;
  }

  //else using value from Service_Participant

  // enable the type specific part of this DataWriter
  this->enable_specific();

  // Get data durability cache if DataWriter QoS requires durable
  // samples.  Publisher servant retains ownership of the cache.
  DataDurabilityCache* const durability_cache =
    TheServiceParticipant->get_data_durability_cache(qos_.durability);

  //Note: the QoS used to set n_chunks_ is Changable=No so
  // it is OK that we cannot change the size of our allocators.
  data_container_ = new WriteDataContainer(this,
                                           depth,
                                           should_block,
                                           qos_.reliability.max_blocking_time,
                                           n_chunks_,
                                           domain_id_,
                                           get_topic_name(),
                                           get_type_name(),
                                           durability_cache,
                                           qos_.durability_service,
                                           this->watchdog_);

  // +1 because we might allocate one before releasing another
  // TBD - see if this +1 can be removed.
  mb_allocator_ = new MessageBlockAllocator(n_chunks_ * association_chunk_multiplier_);
  db_allocator_ = new DataBlockAllocator(n_chunks_+1);
  header_allocator_ = new DataSampleHeaderAllocator(n_chunks_+1);

  if (DCPS_debug_level >= 2) {
    ACE_DEBUG((LM_DEBUG,
               "(%P|%t) DataWriterImpl::enable-mb"
               " Cached_Allocator_With_Overflow %x with %d chunks\n",
               mb_allocator_,
               n_chunks_));

    ACE_DEBUG((LM_DEBUG,
               "(%P|%t) DataWriterImpl::enable-db"
               " Cached_Allocator_With_Overflow %x with %d chunks\n",
               db_allocator_,
               n_chunks_));

    ACE_DEBUG((LM_DEBUG,
               "(%P|%t) DataWriterImpl::enable-header"
               " Cached_Allocator_With_Overflow %x with %d chunks\n",
               header_allocator_,
               n_chunks_));
  }

  if (qos_.liveliness.lease_duration.sec != DDS::DURATION_INFINITE_SEC
      && qos_.liveliness.lease_duration.nanosec != DDS::DURATION_INFINITE_NSEC) {
    liveliness_check_interval_ =
      duration_to_time_value(qos_.liveliness.lease_duration);
    liveliness_check_interval_ *=
      TheServiceParticipant->liveliness_factor()/100.0;

    if (reactor_->schedule_timer(this,
                                 0,
                                 liveliness_check_interval_,
                                 liveliness_check_interval_) == -1) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::enable: %p.\n"),
                 ACE_TEXT("schedule_timer")));

    } else {
      cancel_timer_ = true;
      this->_add_ref();
    }
  }

  // Setup the offered deadline watchdog if the configured deadline
  // period is not the default (infinite).
  DDS::Duration_t const deadline_period = this->qos_.deadline.period;

  if (deadline_period.sec != DDS::DURATION_INFINITE_SEC
      || deadline_period.nanosec != DDS::DURATION_INFINITE_NSEC) {
    ACE_auto_ptr_reset(this->watchdog_,
                       new OfferedDeadlineWatchdog(
                         this->reactor_,
                         this->lock_,
                         this->qos_.deadline,
                         this,
                         this->dw_local_objref_.in(),
                         this->offered_deadline_missed_status_,
                         this->last_deadline_missed_total_count_));
  }

  this->set_enabled();

  try {
    this->enable_transport(reliable);

    const TransportLocatorSeq& trans_conf_info = connection_info();

    DDS::PublisherQos pub_qos;
    this->publisher_servant_->get_qos(pub_qos);

    DCPSInfo_var repo = TheServiceParticipant->get_repository(this->domain_id_);
    this->publication_id_ =
      repo->add_publication(this->domain_id_,
                            this->participant_servant_->get_id(),
                            this->topic_servant_->get_id(),
                            this->dw_remote_objref_,
                            this->qos_,
                            trans_conf_info,
                            pub_qos);

    if (this->publication_id_ == GUID_UNKNOWN) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::enable, ")
                 ACE_TEXT("add_publication returned invalid id. \n")));
      return DDS::RETCODE_ERROR;
    }

    this->data_container_->publication_id_ = this->publication_id_;

  } catch (const Transport::Exception&) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::enable, ")
               ACE_TEXT("Transport Exception.\n")));
    return DDS::RETCODE_ERROR;

  } catch (const CORBA::SystemException& sysex) {
    sysex._tao_print_exception(
      "ERROR: System Exception"
      " in DataWriterImpl::enable");
    return DDS::RETCODE_ERROR;

  } catch (const CORBA::UserException& userex) {
    userex._tao_print_exception(
      "ERROR:  Exception"
      " in DataWriterImpl::enable");
    return DDS::RETCODE_ERROR;
  }

  const DDS::ReturnCode_t writer_enabled_result =
    publisher_servant_->writer_enabled(topic_name_.in(), this);

  if (this->monitor_) {
    this->monitor_->report();
  }

  // Move cached data from the durability cache to the unsent data
  // queue.
  if (durability_cache != 0) {
    if (!durability_cache->get_data(this->domain_id_,
                                    get_topic_name(),
                                    get_type_name(),
                                    this,
                                    this->mb_allocator_,
                                    this->db_allocator_,
                                    this->qos_.lifespan)) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::enable: ")
                 ACE_TEXT("unable to retrieve durable data\n")));
    }
  }

  return writer_enabled_result;
}

DDS::ReturnCode_t
DataWriterImpl::register_instance_i(::DDS::InstanceHandle_t& handle,
                                    DataSample* data,
                                    const DDS::Time_t & source_timestamp)
ACE_THROW_SPEC((CORBA::SystemException))
{
  DBG_ENTRY_LVL("DataWriterImpl","register_instance_i",6);

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DataWriterImpl::register_instance_i: ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  DDS::ReturnCode_t ret =
    this->data_container_->register_instance(handle, data);

  if (ret != DDS::RETCODE_OK) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::register_instance_i: ")
                      ACE_TEXT("register instance with container failed.\n")),
                     ret);
  }

  if (this->monitor_) {
    this->monitor_->report();
  }

  // Add header with the registration sample data.
  DataSampleHeader header;
  ACE_Message_Block* registered_sample =
    this->create_control_message(INSTANCE_REGISTRATION,
                                 header,
                                 data,
                                 source_timestamp);

  if (this->send_control(header, registered_sample) == SEND_CONTROL_ERROR) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DataWriterImpl::register_instance_i: ")
                      ACE_TEXT("send_control failed.\n")),
                     DDS::RETCODE_ERROR);
  }

  return ret;
}

DDS::ReturnCode_t
DataWriterImpl::unregister_instance_i(::DDS::InstanceHandle_t handle,
                                      const DDS::Time_t & source_timestamp)
ACE_THROW_SPEC((CORBA::SystemException))
{
  DBG_ENTRY_LVL("DataWriterImpl","unregister_instance_i",6);

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::unregister_instance_i: ")
                      ACE_TEXT(" Entity is not enabled.\n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  // According to spec 1.2, autodispose_unregistered_instances true causes
  // dispose on the instance prior to calling unregister operation.
  if (this->qos_.writer_data_lifecycle.autodispose_unregistered_instances) {
    this->dispose(handle, source_timestamp);
  }

  DataSample* unregistered_sample_data;

  DDS::ReturnCode_t ret =
    this->data_container_->unregister(handle,
                                      unregistered_sample_data);

  if (ret != DDS::RETCODE_OK) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DataWriterImpl::unregister_instance_i: ")
                      ACE_TEXT(" unregister with container failed. \n")),
                     ret);
  }

  DataSampleHeader header;
  ACE_Message_Block* message =
    this->create_control_message(UNREGISTER_INSTANCE,
                                 header,
                                 unregistered_sample_data,
                                 source_timestamp);

  if (this->send_control(header, message) == SEND_CONTROL_ERROR) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::unregister_instance_i: ")
                      ACE_TEXT(" send_control failed. \n")),
                     DDS::RETCODE_ERROR);
  }

  return ret;
}

void
DataWriterImpl::unregister_instances(const DDS::Time_t& source_timestamp)
{
  PublicationInstanceMapType::iterator it =
    this->data_container_->instances_.begin();

  while (it != this->data_container_->instances_.end()) {
    DDS::InstanceHandle_t handle = it->first;
    ++it; // avoid mangling the iterator

    this->unregister_instance_i(handle, source_timestamp);
  }
}

DDS::ReturnCode_t
DataWriterImpl::write(DataSample* data,
                      DDS::InstanceHandle_t handle,
                      const DDS::Time_t& source_timestamp,
                      GUIDSeq* filter_out)
{
  DBG_ENTRY_LVL("DataWriterImpl","write",6);

  // take ownership of sequence allocated in FooDWImpl::write_w_timestamp()
  GUIDSeq_var filter_out_var(filter_out);

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::write: ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  DataSampleListElement* element;
  DDS::ReturnCode_t ret = this->data_container_->obtain_buffer(element, handle);

  if (ret == DDS::RETCODE_TIMEOUT) {
    return ret; // silent for timeout
  }
  else if (ret != DDS::RETCODE_OK) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DataWriterImpl::write: ")
                      ACE_TEXT("obtain_buffer returned %d.\n"),
                      ret),
                     ret);
  }

  ret = create_sample_data_message(data,
                                   handle,
                                   element->header_,
                                   element->sample_,
                                   source_timestamp,
                                   (filter_out != 0));
  if (ret != DDS::RETCODE_OK) {
    return ret;
  }

  element->filter_out_ = filter_out_var._retn(); // ownership passed to element

  ret = this->data_container_->enqueue(element, handle);

  if (ret != DDS::RETCODE_OK) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DataWriterImpl::write: ")
                      ACE_TEXT("enqueue failed.\n")),
                     ret);
  }

  DataSampleList list = this->get_unsent_data();

  if (this->publisher_servant_->is_suspended()) {
    this->available_data_list_.enqueue_tail_next_send_sample(list);
  } else {
    this->send(list);
  }

  this->last_liveliness_activity_time_ = ACE_OS::gettimeofday();

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
  // Track individual expected sequence numbers in ReaderInfo
  std::set<GUID_t, GUID_tKeyLessThan> excluded;
  if (filter_out && reader_info_.size()) {
    const GUID_t* buf = filter_out->get_buffer();
    excluded.insert(buf, buf + filter_out->length());
  }
  for (RepoIdToReaderInfoMap::iterator iter = reader_info_.begin(),
       end = reader_info_.end(); iter != end; ++iter) {
    // If not excluding this reader, update expected sequence
    if (excluded.count(iter->first) == 0) {
      iter->second.expected_sequence_ = sequence_number_;
    }
  }
#else
  for (RepoIdToReaderInfoMap::iterator iter = reader_info_.begin(),
       end = reader_info_.end(); iter != end; ++iter) {
    iter->second.expected_sequence_ = sequence_number_;
  }
#endif // OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE

  if (this->coherent_) {
    ++this->coherent_samples_;
  }

  return DDS::RETCODE_OK;
}

void
DataWriterImpl::send_suspended_data()
{
  this->send(this->available_data_list_);
  this->available_data_list_.reset();
}

DDS::ReturnCode_t
DataWriterImpl::dispose(::DDS::InstanceHandle_t handle,
                        const DDS::Time_t & source_timestamp)
ACE_THROW_SPEC((CORBA::SystemException))
{
  DBG_ENTRY_LVL("DataWriterImpl","dispose",6);

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::dispose: ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  DataSample* registered_sample_data;
  DDS::ReturnCode_t ret =
    this->data_container_->dispose(handle,
                                   registered_sample_data);

  if (ret != DDS::RETCODE_OK) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DataWriterImpl::dispose: ")
                      ACE_TEXT("dispose failed.\n")),
                     ret);
  }

  DataSampleHeader header;
  ACE_Message_Block* message =
    this->create_control_message(DISPOSE_INSTANCE,
                                 header,
                                 registered_sample_data,
                                 source_timestamp);

  if (this->send_control(header, message) == SEND_CONTROL_ERROR) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::dispose: ")
                      ACE_TEXT(" send_control failed. \n")),
                     DDS::RETCODE_ERROR);
  }

  return ret;
}

DDS::ReturnCode_t
DataWriterImpl::num_samples(::DDS::InstanceHandle_t handle,
                            size_t&                 size)
{
  return data_container_->num_samples(handle, size);
}

DataSampleList
DataWriterImpl::get_unsent_data()
{
  return data_container_->get_unsent_data();
}

DataSampleList
DataWriterImpl::get_resend_data()
{
  return data_container_->get_resend_data();
}

void
DataWriterImpl::unregister_all()
{
  if (cancel_timer_) {
    // The cancel_timer will call handle_close to remove_ref.
    (void) reactor_->cancel_timer(this, 0);
    cancel_timer_ = false;
  }

  data_container_->unregister_all();
}

RepoId
DataWriterImpl::get_publication_id()
{
  return publication_id_;
}

RepoId
DataWriterImpl::get_dp_id()
{
  return participant_servant_->get_id();
}

const char*
DataWriterImpl::get_topic_name()
{
  return topic_name_.in();
}

char const *
DataWriterImpl::get_type_name() const
{
  return type_name_.in();
}

ACE_Message_Block*
DataWriterImpl::create_control_message(MessageId message_id,
                                       DataSampleHeader& header_data,
                                       ACE_Message_Block* data,
                                       const DDS::Time_t& source_timestamp)
{
  header_data.message_id_ = message_id;
  header_data.byte_order_ =
    this->swap_bytes() ? !ACE_CDR_BYTE_ORDER : ACE_CDR_BYTE_ORDER;
  header_data.coherent_change_ = 0;

  if (data) {
    header_data.message_length_ = static_cast<ACE_UINT32>(data->total_length());
    if (header_data.message_length_ == 0) {
      data->release();
    }
  }

  header_data.sequence_ = SequenceNumber::SEQUENCENUMBER_UNKNOWN();
  header_data.sequence_repair_ = false; // set below
  header_data.source_timestamp_sec_ = source_timestamp.sec;
  header_data.source_timestamp_nanosec_ = source_timestamp.nanosec;
  header_data.publication_id_ = publication_id_;
  header_data.publisher_id_ = this->publisher_servant_->publisher_id_;

  if (message_id == INSTANCE_REGISTRATION
   || message_id == DISPOSE_INSTANCE
   || message_id == UNREGISTER_INSTANCE
   || message_id == DISPOSE_UNREGISTER_INSTANCE) {

    header_data.sequence_repair_ = need_sequence_repair();
    // Use the sequence number here for the sake of RTPS (where these
    // control messages map onto the Data Submessage).
    if (this->sequence_number_ == SequenceNumber::SEQUENCENUMBER_UNKNOWN()) {
      this->sequence_number_ = SequenceNumber();
    } else {
      ++this->sequence_number_;
    }
    header_data.sequence_ = this->sequence_number_;
    header_data.key_fields_only_ = true;
  }

  ACE_Message_Block* message;
  ACE_NEW_MALLOC_RETURN(message,
                        static_cast<ACE_Message_Block*>(
                          mb_allocator_->malloc(sizeof(ACE_Message_Block))),
                        ACE_Message_Block(
                          DataSampleHeader::max_marshaled_size(),
                          ACE_Message_Block::MB_DATA,
                          header_data.message_length_ ? data : 0, //cont
                          0, //data
                          0, //allocator_strategy
                          0, //locking_strategy
                          ACE_DEFAULT_MESSAGE_BLOCK_PRIORITY,
                          ACE_Time_Value::zero,
                          ACE_Time_Value::max_time,
                          db_allocator_,
                          mb_allocator_),
                        0);

  *message << header_data;
  // If we incremented sequence number for this control message
  if (header_data.sequence_ != SequenceNumber::SEQUENCENUMBER_UNKNOWN())
  {
    // Update the expected sequence number for all readers
    RepoIdToReaderInfoMap::iterator reader;
    for (reader = reader_info_.begin(); reader != reader_info_.end(); ++reader)
    {
      reader->second.expected_sequence_ = sequence_number_;
    }
  }

  return message;
}

DDS::ReturnCode_t
DataWriterImpl::create_sample_data_message(DataSample* data,
                                           DDS::InstanceHandle_t instance_handle,
                                           DataSampleHeader& header_data,
                                           ACE_Message_Block*& message,
                                           const DDS::Time_t& source_timestamp,
                                           bool content_filter)
{
  PublicationInstance* const instance =
    data_container_->get_handle_instance(instance_handle);

  if (0 == instance) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) DataWriterImpl::create_sample_data_message ")
                      ACE_TEXT("failed to find instance for handle %d\n"),
                      instance_handle),
                     DDS::RETCODE_ERROR);
  }

  header_data.message_id_ = SAMPLE_DATA;
  header_data.byte_order_ =
    this->swap_bytes() ? !ACE_CDR_BYTE_ORDER : ACE_CDR_BYTE_ORDER;
  header_data.coherent_change_ = this->coherent_;
  header_data.group_coherent_ =
    this->publisher_servant_->qos_.presentation.access_scope
    == DDS::GROUP_PRESENTATION_QOS;
  header_data.content_filter_ = content_filter;
  header_data.cdr_encapsulation_ = this->cdr_encapsulation();
  header_data.message_length_ = static_cast<ACE_UINT32>(data->total_length());
  header_data.sequence_repair_ = need_sequence_repair();
  if (this->sequence_number_ == SequenceNumber::SEQUENCENUMBER_UNKNOWN()) {
    this->sequence_number_ = SequenceNumber();
  } else {
    ++this->sequence_number_;
  }
  header_data.sequence_ = this->sequence_number_;
  header_data.source_timestamp_sec_ = source_timestamp.sec;
  header_data.source_timestamp_nanosec_ = source_timestamp.nanosec;

  if (qos_.lifespan.duration.sec != DDS::DURATION_INFINITE_SEC
      || qos_.lifespan.duration.nanosec != DDS::DURATION_INFINITE_NSEC) {
    header_data.lifespan_duration_ = true;
    header_data.lifespan_duration_sec_ = qos_.lifespan.duration.sec;
    header_data.lifespan_duration_nanosec_ = qos_.lifespan.duration.nanosec;
  }

  header_data.publication_id_ = publication_id_;
  header_data.publisher_id_ = this->publisher_servant_->publisher_id_;
  size_t max_marshaled_size = header_data.max_marshaled_size();

  ACE_NEW_MALLOC_RETURN(message,
                        static_cast<ACE_Message_Block*>(
                          mb_allocator_->malloc(sizeof(ACE_Message_Block))),
                        ACE_Message_Block(max_marshaled_size,
                                          ACE_Message_Block::MB_DATA,
                                          data, //cont
                                          0, //data
                                          header_allocator_, //alloc_strategy
                                          0, //locking_strategy
                                          ACE_DEFAULT_MESSAGE_BLOCK_PRIORITY,
                                          ACE_Time_Value::zero,
                                          ACE_Time_Value::max_time,
                                          db_allocator_,
                                          mb_allocator_),
                        DDS::RETCODE_ERROR);

  *message << header_data;
  return DDS::RETCODE_OK;
}

void
DataWriterImpl::data_delivered(const DataSampleListElement* sample)
{
  DBG_ENTRY_LVL("DataWriterImpl","data_delivered",6);

  if (!(sample->publication_id_ == this->publication_id_)) {
    GuidConverter sample_converter(sample->publication_id_);
    GuidConverter writer_converter(publication_id_);
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::data_delivered: ")
               ACE_TEXT(" The publication id %C from delivered element ")
               ACE_TEXT("does not match the datawriter's id %C\n"),
               std::string(sample_converter).c_str(),
               std::string(writer_converter).c_str()));
    return;
  }

  this->data_container_->data_delivered(sample);

  ++data_delivered_count_;
}

void
DataWriterImpl::control_delivered(ACE_Message_Block* sample)
{
  DBG_ENTRY_LVL("DataWriterImpl","control_delivered",6);
  ++control_delivered_count_;
  sample->release();
}

void
DataWriterImpl::deliver_ack(
  const DataSampleHeader& header,
  DataSample*             data)
{
  Serializer serializer(
    data,
    header.byte_order_ != ACE_CDR_BYTE_ORDER);
  SequenceNumber ack;
  serializer >> ack;

  if (DCPS_debug_level > 0) {
    GuidConverter debugConverter(this->publication_id_);
    GuidConverter debugConverter2(header.publication_id_);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DataWriterImpl::deliver_ack() - ")
               ACE_TEXT("publication %C received update for ")
               ACE_TEXT("sample %q from subscription %C.\n"),
               std::string(debugConverter).c_str(),
               ack.getValue(),
               std::string(debugConverter2).c_str()));
  }

  ACE_GUARD(ACE_SYNCH_MUTEX, guard, this->wfaLock_);

  if (this->idToSequence_[ header.publication_id_] < ack) {
    this->idToSequence_[ header.publication_id_] = ack;

    if (DCPS_debug_level > 0) {
      GuidConverter debugConverter(header.publication_id_);
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DataWriterImpl::deliver_ack() - ")
                 ACE_TEXT("broadcasting update.\n")));
    }

    this->wfaCondition_.broadcast();
  }
}

EntityImpl*
DataWriterImpl::parent() const
{
  return this->publisher_servant_;
}

bool
DataWriterImpl::check_transport_qos(const TransportInst&)
{
  // DataWriter does not impose any constraints on which transports
  // may be used based on QoS.
  return true;
}

bool
DataWriterImpl::coherent_changes_pending()
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   get_lock(),
                   false);

  return this->coherent_;
}

void
DataWriterImpl::begin_coherent_changes()
{
  ACE_GUARD(ACE_Recursive_Thread_Mutex,
            guard,
            get_lock());

  this->coherent_ = true;
}

void
DataWriterImpl::end_coherent_changes(const GroupCoherentSamples& group_samples)
{
  // PublisherImpl::pi_lock_ should be held.
  ACE_GUARD(ACE_Recursive_Thread_Mutex,
            guard,
            get_lock());

  CoherentChangeControl end_msg;
  end_msg.coherent_samples_.num_samples_ = this->coherent_samples_;
  end_msg.coherent_samples_.last_sample_ = this->sequence_number_;
  end_msg.group_coherent_
    = this->publisher_servant_->qos_.presentation.access_scope == ::DDS::GROUP_PRESENTATION_QOS;
  if (end_msg.group_coherent_) {
    end_msg.publisher_id_ = this->publisher_servant_->publisher_id_;
    end_msg.group_coherent_samples_ = group_samples;
  }

  ACE_Message_Block* data = 0;
  size_t max_marshaled_size = end_msg.max_marshaled_size();

  ACE_NEW(data, ACE_Message_Block(max_marshaled_size));

  Serializer serializer(
      data,
      this->swap_bytes());

  serializer << end_msg;

  DDS::Time_t source_timestamp =
    time_value_to_time(ACE_OS::gettimeofday());

  DataSampleHeader header;
  ACE_Message_Block* control =
    create_control_message(END_COHERENT_CHANGES, header, data, source_timestamp);

  if (this->send_control(header, control) == SEND_CONTROL_ERROR) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::end_coherent_changes:")
               ACE_TEXT(" unable to send END_COHERENT_CHANGES control message!\n")));
  }

  this->coherent_ = false;
  this->coherent_samples_ = 0;
}

PublisherImpl*
DataWriterImpl::get_publisher_servant()
{
  return publisher_servant_;
}

void
DataWriterImpl::data_dropped(const DataSampleListElement* element,
                             bool dropped_by_transport)
{
  DBG_ENTRY_LVL("DataWriterImpl","data_dropped",6);
  this->data_container_->data_dropped(element, dropped_by_transport);

  ++data_dropped_count_;
}

void
DataWriterImpl::control_dropped(ACE_Message_Block* sample,
                                bool /* dropped_by_transport */)
{
  DBG_ENTRY_LVL("DataWriterImpl","control_dropped",6);
  ++control_dropped_count_;
  sample->release();
}

void
DataWriterImpl::unregistered(::DDS::InstanceHandle_t /* instance_handle */)
{
}

DDS::DataWriterListener*
DataWriterImpl::listener_for(::DDS::StatusKind kind)
{
  // per 2.1.4.3.1 Listener Access to Plain Communication Status
  // use this entities factory if listener is mask not enabled
  // for this kind.
  if (fast_listener_ == 0 || (listener_mask_ & kind) == 0) {
    return publisher_servant_->listener_for(kind);

  } else {
    return fast_listener_;
  }
}

int
DataWriterImpl::handle_timeout(const ACE_Time_Value &tv,
                               const void * /* arg */)
{
  bool liveliness_lost = false;

  ACE_Time_Value elapsed = tv - last_liveliness_activity_time_;

  if (elapsed >= liveliness_check_interval_) {
    if (this->qos_.liveliness.kind == DDS::AUTOMATIC_LIVELINESS_QOS) {
      //Not recent enough then send liveliness message.
      if (DCPS_debug_level > 9) {
        GuidConverter converter(publication_id_);
        ACE_DEBUG((LM_DEBUG,
                   ACE_TEXT("(%P|%t) DataWriterImpl::handle_timeout: ")
                   ACE_TEXT("%C sending LIVELINESS message.\n"),
                   std::string(converter).c_str()));
      }

      if (this->send_liveliness(tv) == false)
        liveliness_lost = true;

    } else
      liveliness_lost = true;

  } else {
    // Recent enough. Schedule the interval.
    if (reactor_->cancel_timer(this) == -1) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("DataWriterImpl::handle_timeout: %p.\n"),
                        ACE_TEXT("cancel_timer")),
                       -1);
    }

    ACE_Time_Value remain = liveliness_check_interval_ - elapsed;

    if (reactor_->schedule_timer(this,
                                 0,
                                 remain,
                                 liveliness_check_interval_) == -1) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("DataWriterImpl::handle_timeout: %p.\n"),
                        ACE_TEXT("schedule_timer")),
                       -1);
    }
  }

  if (!this->liveliness_lost_ && liveliness_lost) {
    ++ this->liveliness_lost_status_.total_count;
    ++ this->liveliness_lost_status_.total_count_change;

    DDS::DataWriterListener* listener =
      listener_for(::DDS::LIVELINESS_LOST_STATUS);

    if (listener != 0) {
      listener->on_liveliness_lost(this->dw_local_objref_.in(),
                                   this->liveliness_lost_status_);
    }
  }

  this->liveliness_lost_ = liveliness_lost;
  return 0;
}

int
DataWriterImpl::handle_close(ACE_HANDLE,
                             ACE_Reactor_Mask)
{
  this->_remove_ref();
  return 0;
}

bool
DataWriterImpl::send_liveliness(const ACE_Time_Value& now)
{
  DDS::Time_t t = time_value_to_time(now);
  DataSampleHeader header;
  ACE_Message_Block* liveliness_msg =
    this->create_control_message(DATAWRITER_LIVELINESS, header, 0, t);

  if (this->send_control(header, liveliness_msg) == SEND_CONTROL_ERROR) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DataWriterImpl::send_liveliness: ")
                      ACE_TEXT(" send_control failed. \n")),
                     false);

  } else {
    last_liveliness_activity_time_ = now;
    return true;
  }
}

PublicationInstance*
DataWriterImpl::get_handle_instance(::DDS::InstanceHandle_t handle)
{
  PublicationInstance* instance = 0;

  if (0 != data_container_) {
    instance = data_container_->get_handle_instance(handle);
  }

  return instance;
}

void
DataWriterImpl::notify_publication_disconnected(const ReaderIdSeq& subids)
{
  DBG_ENTRY_LVL("DataWriterImpl","notify_publication_disconnected",6);

  if (!is_bit_) {
    // Narrow to DDS::DCPS::DataWriterListener. If a DDS::DataWriterListener
    // is given to this DataWriter then narrow() fails.
    DataWriterListener_var the_listener =
      DataWriterListener::_narrow(this->listener_.in());

    if (!CORBA::is_nil(the_listener.in())) {
      PublicationDisconnectedStatus status;
      // Since this callback may come after remove_association which
      // removes the reader from id_to_handle map, we can ignore this
      // error.
      this->lookup_instance_handles(subids,
                                    status.subscription_handles);
      the_listener->on_publication_disconnected(this->dw_local_objref_.in(),
                                                status);
    }
  }
}

void
DataWriterImpl::notify_publication_reconnected(const ReaderIdSeq& subids)
{
  DBG_ENTRY_LVL("DataWriterImpl","notify_publication_reconnected",6);

  if (!is_bit_) {
    // Narrow to DDS::DCPS::DataWriterListener. If a
    // DDS::DataWriterListener is given to this DataWriter then
    // narrow() fails.
    DataWriterListener_var the_listener =
      DataWriterListener::_narrow(this->listener_.in());

    if (!CORBA::is_nil(the_listener.in())) {
      PublicationDisconnectedStatus status;

      // If it's reconnected then the reader should be in id_to_handle
      // map otherwise log with an error.
      if (this->lookup_instance_handles(subids,
                                        status.subscription_handles) == false) {
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) ERROR: DataWriterImpl::"
                   "notify_publication_reconnected: "
                   "lookup_instance_handles failed\n"));
      }

      the_listener->on_publication_reconnected(this->dw_local_objref_.in(),
                                               status);
    }
  }
}

void
DataWriterImpl::notify_publication_lost(const ReaderIdSeq& subids)
{
  DBG_ENTRY_LVL("DataWriterImpl","notify_publication_lost",6);

  if (!is_bit_) {
    // Narrow to DDS::DCPS::DataWriterListener. If a
    // DDS::DataWriterListener is given to this DataWriter then
    // narrow() fails.
    DataWriterListener_var the_listener =
      DataWriterListener::_narrow(this->listener_.in());

    if (!CORBA::is_nil(the_listener.in())) {
      PublicationLostStatus status;

      // Since this callback may come after remove_association which removes
      // the reader from id_to_handle map, we can ignore this error.
      this->lookup_instance_handles(subids,
                                    status.subscription_handles);
      the_listener->on_publication_lost(this->dw_local_objref_.in(),
                                        status);
    }
  }
}

void
DataWriterImpl::notify_publication_lost(const DDS::InstanceHandleSeq& handles)
{
  DBG_ENTRY_LVL("DataWriterImpl","notify_publication_lost",6);

  if (!is_bit_) {
    // Narrow to DDS::DCPS::DataWriterListener. If a
    // DDS::DataWriterListener is given to this DataWriter then
    // narrow() fails.
    DataWriterListener_var the_listener =
      DataWriterListener::_narrow(this->listener_.in());

    if (!CORBA::is_nil(the_listener.in())) {
      PublicationLostStatus status;

      CORBA::ULong len = handles.length();
      status.subscription_handles.length(len);

      for (CORBA::ULong i = 0; i < len; ++ i) {
        status.subscription_handles[i] = handles[i];
      }

      the_listener->on_publication_lost(this->dw_local_objref_.in(),
                                        status);
    }
  }
}

void
DataWriterImpl::notify_connection_deleted()
{
  DBG_ENTRY_LVL("DataWriterImpl","notify_connection_deleted",6);

  // Narrow to DDS::DCPS::DataWriterListener. If a DDS::DataWriterListener
  // is given to this DataWriter then narrow() fails.
  DataWriterListener_var the_listener =
    DataWriterListener::_narrow(this->listener_.in());

  if (!CORBA::is_nil(the_listener.in()))
    the_listener->on_connection_deleted(this->dw_local_objref_.in());
}

bool
DataWriterImpl::lookup_instance_handles(const ReaderIdSeq& ids,
                                        DDS::InstanceHandleSeq & hdls)
{
  if (DCPS_debug_level > 9) {
    CORBA::ULong const size = ids.length();
    const char* separator = "";
    std::stringstream buffer;

    for (unsigned long i = 0; i < size; ++i) {
      buffer << separator << GuidConverter(ids[i]);
      separator = ", ";
    }

    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DataWriterImpl::lookup_instance_handles: ")
               ACE_TEXT("searching for handles for reader Ids: %C.\n"),
               buffer.str().c_str()));
  }

  CORBA::ULong const num_rds = ids.length();
  hdls.length(num_rds);

  for (CORBA::ULong i = 0; i < num_rds; ++i) {
    hdls[i] = this->participant_servant_->get_handle(ids[i]);
  }

  return true;
}

bool
DataWriterImpl::persist_data()
{
  return this->data_container_->persist_data();
}

void
DataWriterImpl::reschedule_deadline()
{
  if (this->watchdog_.get() != 0) {
    this->data_container_->reschedule_deadline();
  }
}

void
DataWriterImpl::wait_pending()
{
  this->data_container_->wait_pending();
}

void
DataWriterImpl::get_instance_handles(InstanceHandleVec& instance_handles)
{
  this->data_container_->get_instance_handles(instance_handles);
}

void
DataWriterImpl::get_readers(IdSet& readers)
{
  ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, this->lock_);
  readers = this->readers_;
}

void
DataWriterImpl::retrieve_inline_qos_data(TransportSendListener::InlineQosData& qos_data) const
{
  this->publisher_servant_->get_qos(qos_data.pub_qos);
  qos_data.dw_qos = this->qos_;
  qos_data.topic_name = this->topic_name_.in();
}

bool
DataWriterImpl::need_sequence_repair() const
{
  for (RepoIdToReaderInfoMap::const_iterator it = reader_info_.begin(),
       end = reader_info_.end(); it != end; ++it) {
    if (it->second.expected_sequence_ != sequence_number_) {
      return true;
    }
  }
  return false;
}

} // namespace DCPS
} // namespace OpenDDS
