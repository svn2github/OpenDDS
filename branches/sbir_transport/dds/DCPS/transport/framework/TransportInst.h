/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_TRANSPORTINST_H
#define OPENDDS_DCPS_TRANSPORTINST_H

#include <ace/config.h>
#ifndef ACE_LACKS_PRAGMA_ONCE
#pragma once
#endif

#include "dds/DCPS/dcps_export.h"
#include "TransportDefs.h"
#include "ThreadSynchStrategy_rch.h"
#include "TransportImpl_rch.h"
#include "TransportImpl.h"
#include "PerConnectionSynchStrategy.h"
#include "dds/DCPS/RcObject_T.h"

#include "ace/Synch.h"

#include <string>

class ACE_Configuration_Heap;
class ACE_Configuration_Section_Key;

namespace OpenDDS {
namespace DCPS {

/**
 * @class TransportInst
 *
 * @brief Base class to hold configuration settings for TransportImpls.
 *
 * Each transport implementation will need to define a concrete
 * subclass of the TransportInst class.  The base
 * class (TransportInst) contains configuration settings that
 * are common to all (or most) concrete transport implementations.
 * The concrete transport implementation defines any configuration
 * settings that it requires within its concrete subclass of this
 * TransportInst base class.
 *
 * The TransportInst object is supplied to the
 * TransportImpl::configure() method.
 */
class OpenDDS_Dcps_Export TransportInst : public RcObject<ACE_SYNCH_MUTEX> {
public:

  std::string name() const { return name_; }

  void send_thread_strategy(const ThreadSynchStrategy_rch& strategy);

  ThreadSynchStrategy_rch send_thread_strategy() const;

  /// Overwrite the default configurations with the configuration from the
  /// given section in the ACE_Configuration_Heap object.
  virtual int load(ACE_Configuration_Heap& cf,
                   ACE_Configuration_Section_Key& sect);

  /// Diagnostic aid.
  void dump();
  virtual void dump(std::ostream& os);

  /// Format name of transport configuration parameter for use in
  /// conjunction with dump(std::ostream& os).
  static std::string formatNameForDump(const char* name);

  const std::string transport_type_;

  /// Number of pre-created link (list) objects per pool for the
  /// "send queue" of each DataLink.
  size_t queue_messages_per_pool_;

  /// Initial number of pre-allocated pools of link (list) objects
  /// for the "send queue" of each DataLink.
  size_t queue_initial_pools_;

  /// Max size (in bytes) of a packet (packet header + sample(s))
  ACE_UINT32 max_packet_size_;

  /// Max number of samples that should ever be in a single packet.
  size_t max_samples_per_packet_;

  /// Optimum size (in bytes) of a packet (packet header + sample(s)).
  ACE_UINT32 optimum_packet_size_;

  /// Flag for whether a new thread is needed for connection to
  /// send without backpressure.
  bool thread_per_connection_;

  /// Delay in milliseconds that the datalink should be released after all
  /// associations are removed. The default value is 10 seconds.
  long datalink_release_delay_;

  /// The number of chunks used to size allocators for transport control
  /// samples. The default value is 32.
  size_t datalink_control_chunks_;

protected:

  TransportInst(const char* type,
                const std::string& name,
                ThreadSynchStrategy* send_strategy
                  = new PerConnectionSynchStrategy);

  virtual ~TransportInst();

private:

  /// Adjust the configuration values which gives warning on adjusted
  /// value.
  void adjust_config_value();

  friend class TransportRegistry;
  void shutdown();

  friend class TransportClient;
  TransportImpl_rch impl();
  virtual TransportImpl* new_impl(const RcHandle<TransportInst>& inst) = 0;

  const std::string name_;

  /// Thread strategy used for sending data samples (and incomplete
  /// packets) when a DataLink has encountered "backpressure".
  ThreadSynchStrategy_rch send_thread_strategy_;

  TransportImpl_rch impl_;
  ACE_SYNCH_MUTEX lock_;
};

} // namespace DCPS
} // namespace OpenDDS

#if defined(__ACE_INLINE__)
#include "TransportInst.inl"
#endif /* __ACE_INLINE__ */

#endif
