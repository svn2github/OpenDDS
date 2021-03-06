/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef TAO_DDS_DCPS_RECEIVEDDATAELEMENTLIST_H
#define TAO_DDS_DCPS_RECEIVEDDATAELEMENTLIST_H

#include "ace/Atomic_Op_T.h"
#include "ace/Thread_Mutex.h"

#include "dcps_export.h"
#include "Definitions.h"
#include "dds/DdsDcpsInfrastructureC.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

namespace OpenDDS {
namespace DCPS {

class InstanceState;

class OpenDDS_Dcps_Export ReceivedDataElement {
public:
  ReceivedDataElement(PublicationId pub, void *received_data)
    : pub_(pub),
      registered_data_(received_data),
      sample_state_(DDS::NOT_READ_SAMPLE_STATE),
      coherent_change_(false),
      disposed_generation_count_(0),
      no_writers_generation_count_(0),
      zero_copy_cnt_(0),
      sequence_(0),
      previous_data_sample_(0),
      next_data_sample_(0),
      ref_count_(1)
  {}

  long dec_ref() {
    return --this->ref_count_;
  }

  long inc_ref() {
    return ++this->ref_count_;
  }

  long ref_count() {
    return this->ref_count_.value();
  }

  PublicationId pub_;

  /// Data sample received
  void *registered_data_;  // ugly, but works....

  /// Sample state for this data sample:
  /// DDS::NOT_READ_SAMPLE_STATE/DDS::READ_SAMPLE_STATE
  DDS::SampleStateKind sample_state_ ;

  ///Source time stamp for this data sample
  DDS::Time_t source_timestamp_;

  /// Reception time stamp for this data sample
  DDS::Time_t destination_timestamp_;

  /// Sample belongs to an active coherent change set
  bool coherent_change_;

  /// The data sample's instance's disposed_generation_count_
  /// at the time the sample was received
  size_t disposed_generation_count_ ;

  /// The data sample's instance's no_writers_generation_count_
  /// at the time the sample was received
  size_t no_writers_generation_count_ ;

  /// This is needed to know if delete DataReader should fail with
  /// PRECONDITION_NOT_MET because there are outstanding loans.
  ACE_Atomic_Op<ACE_Thread_Mutex, long> zero_copy_cnt_;

  /// The data sample's sequence number
  ACE_INT16 sequence_ ;

  /// the previous data sample in the ReceivedDataElementList
  ReceivedDataElement *previous_data_sample_ ;

  /// the next data sample in the ReceivedDataElementList
  ReceivedDataElement *next_data_sample_ ;

private:
  ACE_Atomic_Op<ACE_Thread_Mutex, long> ref_count_;

}; // class ReceivedDataElement

class OpenDDS_Dcps_Export ReceivedDataFilter {
public:
  ReceivedDataFilter() {}
  virtual ~ReceivedDataFilter() {}

  virtual bool operator()(ReceivedDataElement* /* data_sample */) {
    return false;
  }
};

class OpenDDS_Dcps_Export ReceivedDataOperation {
public:
  ReceivedDataOperation() {}
  virtual ~ReceivedDataOperation() {}

  virtual void operator()(ReceivedDataElement* /* data_sample */) {}
};

class OpenDDS_Dcps_Export ReceivedDataElementList {
public:
  ReceivedDataElementList(InstanceState *instance_state = 0) ;

  ~ReceivedDataElementList() ;

  void apply_all(ReceivedDataFilter& match, ReceivedDataOperation& func);

  // adds a data sample to the end of the list
  void add(ReceivedDataElement *data_sample) ;

  bool remove(ReceivedDataElement *data_sample) ;

  bool remove(ReceivedDataFilter& match, bool eval_all);

  ReceivedDataElement *remove_head() ;
  ReceivedDataElement *remove_tail() ;

  /// The first element of the list.
  ReceivedDataElement* head_ ;

  /// The last element of the list.
  ReceivedDataElement* tail_ ;

  /// Number of elements in the list.
  ssize_t              size_ ;

private:
  InstanceState *instance_state_ ;
}; // ReceivedDataElementList

} // namespace DCPS
} // namespace OpenDDS

#if defined (__ACE_INLINE__)
# include "ReceivedDataElementList.inl"
#endif  /* __ACE_INLINE__ */

#endif /* TAO_DDS_DCPS_RECEIVEDDATAELEMENTLIST_H  */
