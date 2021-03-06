/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "dds/DCPS/RTPS/MessageTypes.h"

namespace OpenDDS {
namespace DCPS {

ACE_INLINE size_t
RtpsTransportHeader::max_marshaled_size()
{
  return RTPS::RTPSHDR_SZ;
}

ACE_INLINE
RtpsTransportHeader::RtpsTransportHeader()
  : length_(0)
  , valid_(false)
{
}

ACE_INLINE
RtpsTransportHeader::RtpsTransportHeader(ACE_Message_Block& mb)
  : length_(0)
  , valid_(false)
{
  init(mb);
}

ACE_INLINE RtpsTransportHeader&
RtpsTransportHeader::operator=(ACE_Message_Block& mb)
{
  // length_ is not updated in a call to operator=()
  init(mb);
  return *this;
}

ACE_INLINE void
RtpsTransportHeader::incomplete(ACE_Message_Block& mb)
{
  // If we get a datagram with an incomplete header, we need to skip
  // and consume the received bytes so the transport framework doesn't
  // try to give us these bytes again on the next call to handle_input().
  ACE_Message_Block* cur = &mb;
  for (size_t len = mb.total_length(); cur && len; cur = cur->cont()) {
    len -= cur->length();
    cur->rd_ptr(cur->length());
  }
}

ACE_INLINE bool
RtpsTransportHeader::valid() const
{
  return valid_;
}

ACE_INLINE bool
RtpsTransportHeader::last_fragment()
{
  return false;
}

ACE_INLINE void
RtpsTransportHeader::last_fragment(bool /*frag*/)
{
}

}
}
