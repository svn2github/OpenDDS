/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "dds/DCPS/RawDataSample.h"

namespace OpenDDS {
namespace DCPS {


RawDataSample::RawDataSample()
  : sample_(0)
{
}

RawDataSample::RawDataSample(MessageId          mid,
                             int32_t            sec,
                             uint32_t           nano_sec,
                             PublicationId      pid,
                             bool               byte_order,
                             ACE_Message_Block* blk)
  : message_id_(mid)
  , publication_id_(pid)
  , sample_byte_order_(byte_order)
  , sample_(blk->duplicate())
{
  source_timestamp_.sec = sec;
  source_timestamp_.nanosec = nano_sec;
}

RawDataSample::~RawDataSample()
{
  if (sample_)
    sample_->release();
}

RawDataSample::RawDataSample(const RawDataSample& other)
  : message_id_(other.message_id_)
  , source_timestamp_(other.source_timestamp_)
  , publication_id_(other.publication_id_)
  , sample_byte_order_(other.sample_byte_order_)
  , sample_(other.sample_->duplicate())
{
}


RawDataSample&
RawDataSample::operator=(const RawDataSample& other)
{
  RawDataSample tmp(other);
  std::swap(*this, tmp);
  return *this;
}

} // namespace DCPS
} // namespace OpenDDS
