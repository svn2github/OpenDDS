// -*- C++ -*-
//
// $Id$

#include "DCPS/DdsDcps_pch.h"
#include  "ReceivedDataElementList.h"

#if !defined (__ACE_INLINE__)
# include "ReceivedDataElementList.inl"
#endif /* ! __ACE_INLINE__ */


TAO::DCPS::ReceivedDataElementList::ReceivedDataElementList(InstanceState *instance_state)
          : head_(0), tail_(0), size_(0), instance_state_(instance_state)
{
}

TAO::DCPS::ReceivedDataElementList::~ReceivedDataElementList()
{
  // The memory pointed to by instance_state_ is owned by
  // another object.
}
   
bool
TAO::DCPS::ReceivedDataElementList::remove(ReceivedDataElement *data_sample)
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


