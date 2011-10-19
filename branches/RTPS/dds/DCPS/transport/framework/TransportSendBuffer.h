/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef DCPS_TRANSPORTSENDBUFFER_H
#define DCPS_TRANSPORTSENDBUFFER_H

#include "dds/DCPS/dcps_export.h"

#include "TransportRetainedElement.h"
#include "TransportReplacedElement.h"
#include "TransportSendStrategy.h"

#include "dds/DCPS/Definitions.h"

#include <map>
#include <utility>

class ACE_Message_Block;

namespace OpenDDS {
namespace DCPS {

/// Abstract base class that forms the interface for TransportSendStrategy
/// to store data for potential retransmission.  Derived classes actually
/// store the data and can utilize TransportSendBuffer's friendship in
/// TransportSendStrategy to retransmit (see method "resend_one").
class OpenDDS_Dcps_Export TransportSendBuffer {
public:
  size_t capacity() const;
  void bind(TransportSendStrategy* strategy);

  virtual void retain_all(RepoId pub_id) = 0;
  virtual void insert(SequenceNumber sequence,
                      TransportSendStrategy::QueueType* queue,
                      ACE_Message_Block* chain) = 0;

protected:
  explicit TransportSendBuffer(size_t capacity) : capacity_(capacity) {}
  virtual ~TransportSendBuffer();

  typedef TransportSendStrategy::LockType LockType;
  typedef TransportSendStrategy::QueueType QueueType;
  typedef std::pair<QueueType*, ACE_Message_Block*> BufferType;

  void resend_one(const BufferType& buffer);
  LockType& strategy_lock() { return this->strategy_->lock_; }

  TransportSendStrategy* strategy_;
  const size_t capacity_;

private:
  TransportSendBuffer(const TransportSendBuffer&); // unimplemented
  TransportSendBuffer& operator=(const TransportSendBuffer&); // unimplemented
};

/// Implementation of TransportSendBuffer that manages data for a single
/// domain of SequenceNumbers -- for a given SingelSendBuffer object, the
/// sequence numbers passed to insert() must be generated from the same place.
class OpenDDS_Dcps_Export SingleSendBuffer
  : public TransportSendBuffer, public RcObject<ACE_SYNCH_MUTEX> {
public:
  void release_all();
  void release(BufferType& buffer);

  size_t n_chunks() const;

  SingleSendBuffer(size_t capacity, size_t max_samples_per_packet);

  bool resend(const SequenceRange& range);

  SequenceNumber low() const;
  SequenceNumber high() const;
  bool empty() const;

  void retain_all(RepoId pub_id);
  void insert(SequenceNumber sequence,
              TransportSendStrategy::QueueType* queue,
              ACE_Message_Block* chain);

private:
  size_t n_chunks_;

  TransportRetainedElementAllocator retained_allocator_;
  MessageBlockAllocator retained_mb_allocator_;
  DataBlockAllocator retained_db_allocator_;
  TransportReplacedElementAllocator replaced_allocator_;
  MessageBlockAllocator replaced_mb_allocator_;
  DataBlockAllocator replaced_db_allocator_;

  typedef std::map<SequenceNumber, BufferType> BufferMap;
  BufferMap buffers_;
};

} // namespace DCPS
} // namespace OpenDDS

#ifdef __ACE_INLINE__
# include "TransportSendBuffer.inl"
#endif  /* __ACE_INLINE__ */

#endif  /* DCPS_TRANSPORTSENDBUFFER_H */
