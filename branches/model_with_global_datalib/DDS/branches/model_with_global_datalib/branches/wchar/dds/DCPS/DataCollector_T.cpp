/// -*- C++ -*-
///
/// $Id$

#include "DataCollector_T.h"

#if !defined (__ACE_INLINE__)
#include "DataCollector_T.inl"
#endif /* __ACE_INLINE__ */

#include <iostream>

namespace OpenDDS { namespace DCPS {

template< typename DatumType>
DataCollector< DatumType>::~DataCollector()
{
}

template< typename DatumType>
void
DataCollector< DatumType>::collect( const DatumType& datum)
{
  if( this->onFull_ == Unbounded) {
    this->buffer_.push_back( datum);

  } else {
    // writeAt == bound only when we either have no buffer (bound == 0) or
    // when we are full with no wrapping (writeAt == bound)
    if( this->writeAt_ != this->bound_) {
      this->buffer_[ this->writeAt_++] = datum;

      // This datum filled the buffer.
      if( this->writeAt_ == this->bound_) {
        this->full_  = true;
        if( this->onFull_ == KeepNewest) {
          this->writeAt_ = 0;
        }
      }
    }
  }
}

template< typename DatumType>
unsigned int
DataCollector< DatumType>::size() const
{
  if( this->onFull_ == Unbounded) return this->buffer_.size();
  else if( this->full_)           return this->bound_;
  else                            return this->writeAt_;
}

template< typename DatumType>
std::ostream&
DataCollector< DatumType>::insert( std::ostream& str) const
{
  str.precision( 5);
  str << std::scientific;

  // Oldest data first.
  if( this->full_) {
    for( unsigned int index = this->writeAt_; index < this->bound_; ++index) {
      str << this->buffer_[ index] << std::endl;
    }
  }

  // Bounded case.
  int end = this->writeAt_;
  if( end == 0) {
    // Unbounded case.
    end = this->buffer_.size();
  }

  // Newest data last.
  for( int index = 0; index < end; ++index) {
    str << this->buffer_[ index] << std::endl;
  }

  return str;
}

}} // End namespace OpenDDS::DCPS

