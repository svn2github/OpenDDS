/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

template<typename DatumType>
ACE_INLINE
std::ostream& operator<<(
  std::ostream& str,
  const OpenDDS::DCPS::DataCollector<DatumType>& value)
{
  return value.insert(str);
}

namespace OpenDDS {
namespace DCPS {

template<typename DatumType>
ACE_INLINE
DataCollector<DatumType>::DataCollector(unsigned int bound, OnFull onFull)
  : buffer_(bound),
    writeAt_(0),
    bound_(bound),
    full_(false),
    onFull_(onFull)
{
  if (this->onFull_ == Unbounded) {
    this->buffer_.clear();
  }
}

template<typename DatumType>
ACE_INLINE
DataCollector<DatumType>&
DataCollector<DatumType>::operator<<(DatumType datum)
{
  this->collect(datum);
  return *this;
}

} // namespace DCPS
} // namespace OpenDDS
