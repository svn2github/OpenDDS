/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_DATASAMPLELIST_H
#define OPENDDS_DCPS_DATASAMPLELIST_H

#include "dds/DdsDcpsInfoUtilsC.h"
#include "Definitions.h"
#include "transport/framework/TransportDefs.h"
#include "Dynamic_Cached_Allocator_With_Overflow_T.h"
//#include "DataSampleHeader.h"
#include "DataSampleListElement.h"

//#include <map>
#include <iterator>

class DDS_TEST;

namespace OpenDDS {
namespace DCPS {

//const CORBA::ULong MAX_READERS_PER_ELEM = 5;
//typedef Dynamic_Cached_Allocator_With_Overflow<ACE_Thread_Mutex>
//  TransportSendElementAllocator;

//class TransportCustomizedElement;
//typedef Dynamic_Cached_Allocator_With_Overflow<ACE_Thread_Mutex>
//  TransportCustomizedElementAllocator;

struct DataSampleListElement;
typedef Cached_Allocator_With_Overflow<DataSampleListElement, ACE_Null_Mutex>
  DataSampleListElementAllocator;

const int MAX_READERS_TO_RESEND = 5;

//class TransportSendListener;
//struct PublicationInstance;

/**
* Currently we contain entire messages in a single ACE_Message_Block
* chain.
*/
typedef ACE_Message_Block DataSample;

/**
 * @struct DataSampleSendListIterator
 *
 * @brief @c DataSampleList STL-style iterator implementation.
 *
 * This class implements a STL-style iterator for the OpenDDS
 * @c DataSampleList class.  The resulting iterator may be used
 * @c with the STL generic algorithms.  It is meant for iteration
 * @c over the "send samples" in a @c DataSampleList.
 */
class OpenDDS_Dcps_Export DataSampleSendListIterator
  : public std::iterator<std::bidirectional_iterator_tag, DataSampleListElement> {
public:

  /// Default constructor.
  /**
   * This constructor is used when constructing an "end" iterator.
   */

  DataSampleSendListIterator(DataSampleListElement* head,
                         DataSampleListElement* tail,
                         DataSampleListElement* current);

  DataSampleSendListIterator& operator++();
  DataSampleSendListIterator  operator++(int);
  DataSampleSendListIterator& operator--();
  DataSampleSendListIterator  operator--(int);
  reference operator*();
  pointer operator->();

  bool
  operator==(const DataSampleSendListIterator& rhs) const {
    return this->head_ == rhs.head_
           && this->tail_ == rhs.tail_
           && this->current_ == rhs.current_;
  }

  bool
  operator!=(const DataSampleSendListIterator& rhs) const {
    return !(*this == rhs);
  }

private:
  DataSampleSendListIterator();

  DataSampleListElement* head_;
  DataSampleListElement* tail_;
  DataSampleListElement* current_;

};

/**
* Lists include a pointer to both the head and tail elements of the
* list.  Cache the number of elements in the list so that we don't have
* to traverse the list to ind this information.  For most lists that
* we manage, we append to the tail and remove from the head.  There are
* some lists where we remove from the middle, which are not handled by
* this list structure.
*/
class OpenDDS_Dcps_Export DataSampleList {

public:

  /// STL-style bidirectional iterator type.
  typedef DataSampleSendListIterator iterator;

  /// Default constructor clears the list.
  DataSampleList();
  virtual ~DataSampleList(){};

  /// Reset to initial state.
  void reset();

  ssize_t size() const {return size_;};

  DataSampleListElement* head() const {return head_;};

  DataSampleListElement* tail() const {return tail_;};

  virtual void enqueue_tail(const DataSampleListElement* element) = 0;

  virtual bool dequeue_head(DataSampleListElement*& stale) = 0;

  //PWO: took away the 'const' for the paramter 'stale'
  //virtual bool dequeue(/*const*/ DataSampleListElement* stale) = 0;

  //virtual DataSampleListElement* dequeue_sample(const DataSampleListElement* stale) = 0;

  /// This function assumes the list is the sending_data, sent_data,
  /// unsent_data or released_data which is linked by the
  /// next_sample/previous_sample.
  //void enqueue_tail_next_sample(DataSampleListElement* sample);

  /// This function assumes the list is the sending_data, sent_data,
  /// unsent_data or released_data which is linked by the
  /// next_sample/previous_sample.
  //bool dequeue_head_next_sample(DataSampleListElement*& stale);

  /// This function assumes the list is the sending_data or sent_data
  /// which is linked by the next_send_sample.
  //void enqueue_tail_next_send_sample(const DataSampleListElement* sample);

  /// This function assumes the list is the sending_data or sent_data
  /// which is linked by the next_send_sample.
  //bool dequeue_head_next_send_sample(DataSampleListElement*& stale);

  /// This function assumes the list is the instance samples that is
  /// linked by the next_instance_sample_.
//  void enqueue_tail_next_instance_sample(DataSampleListElement* sample);

  /// This function assumes the list is the instance samples that is
  /// linked by the next_instance_sample_.
//  bool dequeue_head_next_instance_sample(DataSampleListElement*& stale);

  /// This function assumes that the list is a list that linked using
  /// next_sample/previous_sample but the stale element's position is
  /// unknown.
  //bool dequeue_next_sample(DataSampleListElement* stale);

  /// This function assumes that the list is a list that linked using
  /// next_instance_sample but the stale element's position is
  /// unknown.
//  DataSampleListElement*
//  dequeue_next_instance_sample(const DataSampleListElement* stale);

  /// This function assumes that the list is a list that linked using
  /// next_send_sample but the stale element's position is
  /// unknown.
 // DataSampleListElement*
 // dequeue_next_send_sample(const DataSampleListElement* stale);

  /// This function assumes the appended list is a list linked with
  /// previous/next_sample_ and might be linked with next_send_sample_.
  /// If it's not linked with the next_send_sample_ then this function
  /// will make it linked before appending.
 // void enqueue_tail_next_send_sample(DataSampleList list);

  /// Return iterator to beginning of list.
  iterator begin();

  /// Return iterator to end of list.
  iterator end();

protected:

  /// The first element of the list.
  DataSampleListElement* head_;

  /// The last element of the list.
  DataSampleListElement* tail_;

  /// Number of elements in the list.
  ssize_t                size_;
  //TBD size is never negative so should be size_t but this ripples through
  // the transport code so leave it for now. SHH

};

class OpenDDS_Dcps_Export DataSampleWriterList : public DataSampleList {

 public:

  DataSampleWriterList() : DataSampleList(){};
  ~DataSampleWriterList(){};

  //void reset();

  void enqueue_tail(const DataSampleListElement* element);

  bool dequeue_head(DataSampleListElement*& stale);

  bool dequeue(const DataSampleListElement* stale);

};

class OpenDDS_Dcps_Export DataSampleInstanceList : public DataSampleList {

 public:

  DataSampleInstanceList() : DataSampleList(){};
  ~DataSampleInstanceList(){};

  //void reset();

  void enqueue_tail(const DataSampleListElement* element);

  bool dequeue_head(DataSampleListElement*& stale);

  bool dequeue(const DataSampleListElement* stale);

};

class OpenDDS_Dcps_Export DataSampleSendList : public DataSampleList {

 public:

  typedef DataSampleSendListIterator iterator;

  DataSampleSendList() : DataSampleList(){};
  ~DataSampleSendList(){};

  //void reset();
  static const DataSampleSendList* send_list_containing_element(const DataSampleListElement* element,
                                                                std::vector<DataSampleSendList*> send_lists);

  void enqueue_tail(const DataSampleListElement* element);
  void enqueue_tail(DataSampleSendList list);

  bool dequeue_head(DataSampleListElement*& stale);

  bool dequeue(const DataSampleListElement* stale);

  friend class ::DDS_TEST;

};

} // namespace DCPS
} // namespace OpenDDS

#if defined(__ACE_INLINE__)
#include "DataSampleList.inl"
#endif /* __ACE_INLINE__ */

#endif  /* OPENDDS_DCPS_DATASAMPLELIST_H */
