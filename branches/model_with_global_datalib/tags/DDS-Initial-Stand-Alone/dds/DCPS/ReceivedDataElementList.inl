// -*- C++ -*-
//
// $Id$

#include  "ReceivedDataElementList.h"
#include  "InstanceState.h"

using namespace TAO::DCPS ;

ACE_INLINE
ReceivedDataElementList::ReceivedDataElementList(InstanceState *instance_state)
          : head_(0), tail_(0), size_(0), instance_state_(instance_state)
{
}

ACE_INLINE
ReceivedDataElementList::~ReceivedDataElementList()
{
}
   
ACE_INLINE
void
ReceivedDataElementList::add(ReceivedDataElement *data_sample)
{
  // The default action is to simply add to the
  // tail - in the future we may want to add
  // to the middle of the list based on sequence
  // number and/or source timestamp

  data_sample->previous_data_sample_ = 0;
  data_sample->next_data_sample_ = 0;

  ++size_ ;
  if (!head_)
  {
    // First sample in the list.
    head_ = tail_ = data_sample ;
  }
  else
  {
    // Add to existing list.
    tail_->next_data_sample_ = data_sample ;
    data_sample->previous_data_sample_ = tail_;
    tail_ = data_sample;
  }

  if (instance_state_)
    {
      instance_state_->empty(false);
    }
}


ACE_INLINE
bool
ReceivedDataElementList::remove(ReceivedDataElement *data_sample)
{
  if (!head_)
  {
    return false;
  }

  bool found(false) ;
  for (ReceivedDataElement* item = head_ ; item != 0 ;
      item = item->next_data_sample_)
  {
    if (item == data_sample)
    {
      found = true ;
      break ;
    }
  }

  if (!found)
  {
    return false ;
  }

  size_-- ;

  if (data_sample == head_)
  {
    head_ = data_sample->next_data_sample_ ;
    if (head_)
    {
      head_->previous_data_sample_ = 0 ;
    }
  }
  else if (data_sample == tail_)
  {
    tail_ = data_sample->previous_data_sample_ ;
    if (tail_)
    {
      tail_->next_data_sample_ = 0 ;
    }
  }
  else
  {
    data_sample->previous_data_sample_->next_data_sample_ =
        data_sample->next_data_sample_ ;
    data_sample->next_data_sample_->previous_data_sample_ =
        data_sample->previous_data_sample_ ;
  }

  if (instance_state_ && size_ == 0)
    {
      // let the instance know it is empty
      instance_state_->empty(true);
    }
          
  return true ;
}



ACE_INLINE
ReceivedDataElement *
ReceivedDataElementList::remove_head()
{
  if (!size_)
  {
    return 0 ;
  }

  ReceivedDataElement *ptr = head_ ;

  remove(head_) ;

  return ptr ;
}



ACE_INLINE
ReceivedDataElement *
ReceivedDataElementList::remove_tail()
{
  if (!size_)
  {
    return 0 ;
  }

  ReceivedDataElement *ptr = tail_ ;

  remove(tail_) ;

  return ptr ;
}
