/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "EntryExit.h"
#include "TransportReassembly.h"

template<typename TH, typename DSH>
ACE_INLINE int
OpenDDS::DCPS::TransportReceiveStrategy<TH, DSH>::start()
{
  DBG_ENTRY_LVL("TransportReceiveStrategy","start",6);
  return this->start_i();
}

template<typename TH, typename DSH>
ACE_INLINE void
OpenDDS::DCPS::TransportReceiveStrategy<TH, DSH>::stop()
{
  DBG_ENTRY_LVL("TransportReceiveStrategy","stop",6);
  this->stop_i();
}

template<typename TH, typename DSH>
ACE_INLINE const TH&
OpenDDS::DCPS::TransportReceiveStrategy<TH, DSH>::received_header() const
{
  DBG_ENTRY_LVL("TransportReceiveStrategy","received_header",6);
  return this->receive_transport_header_;
}

template<typename TH, typename DSH>
ACE_INLINE size_t
OpenDDS::DCPS::TransportReceiveStrategy<TH, DSH>::successor_index(size_t index) const
{
  return ++index % RECEIVE_BUFFERS;
}

template<typename TH, typename DSH>
ACE_INLINE void
OpenDDS::DCPS::TransportReceiveStrategy<TH, DSH>::relink(bool)
{
  // The subclass needs implement this function for re-establishing
  // the link upon recv failure.
}

template<typename TH, typename DSH>
ACE_INLINE void
OpenDDS::DCPS::TransportReceiveStrategy<TH, DSH>::data_unavailable(
  const SequenceRange& dropped)
{
  if (this->reassembly_) {
    this->reassembly_->data_unavailable(dropped);
  }
}
