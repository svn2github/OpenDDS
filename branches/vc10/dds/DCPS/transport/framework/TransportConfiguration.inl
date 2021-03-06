/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "TransportDefs.h"
#include "PerConnectionSynchStrategy.h"
#include "EntryExit.h"

// TBD SOON - The ThreadStrategy objects need to be reference counted.
// TBD - Resolve Mike's questions/comments.
//MJM: Shouldn't we just treat them as *_ptr thingies here?  And not
//MJM: worry about ownership within the configuration object?  I guess I
//MJM: am assuming that the configuration object is just created to pass
//MJM: information about, not to maintain the information for a
//MJM: specified lifetime.  I could be very wrong about the use case.

ACE_INLINE
OpenDDS::DCPS::TransportConfiguration::TransportConfiguration(ThreadSynchStrategy* send_strategy)
  : swap_bytes_(0),
    queue_messages_per_pool_(DEFAULT_CONFIG_QUEUE_MESSAGES_PER_POOL),
    queue_initial_pools_(DEFAULT_CONFIG_QUEUE_INITIAL_POOLS),
    max_packet_size_(DEFAULT_CONFIG_MAX_PACKET_SIZE),
    max_samples_per_packet_(DEFAULT_CONFIG_MAX_SAMPLES_PER_PACKET),
    optimum_packet_size_(DEFAULT_CONFIG_OPTIMUM_PACKET_SIZE),
    thread_per_connection_(0),
    datalink_release_delay_(10000),
    datalink_control_chunks_(32),
    send_thread_strategy_(send_strategy)
{
  DBG_ENTRY_LVL("TransportConfiguration","TransportConfiguration",6);
  this->adjust_config_value();
}

// The class DOES take ownership of the parameter (DCB)
ACE_INLINE
void
OpenDDS::DCPS::TransportConfiguration::send_thread_strategy
(ThreadSynchStrategy* strategy)
{
  DBG_ENTRY_LVL("TransportConfiguration","send_thread_strategy",6);
  this->send_thread_strategy_ = strategy;
}

// This method does NOT give up ownership of the returned strategy (DCB)
ACE_INLINE
OpenDDS::DCPS::ThreadSynchStrategy*
OpenDDS::DCPS::TransportConfiguration::send_thread_strategy()
{
  DBG_ENTRY_LVL("TransportConfiguration","send_thread_strategy",6);
  return this->send_thread_strategy_.in();
}

ACE_INLINE
void
OpenDDS::DCPS::TransportConfiguration::adjust_config_value()
{
  // Ensure that the number of samples put into the packet does
  // not exceed the allowed number of io vectors to be sent by the OS.
  size_t old_value = max_samples_per_packet_;

  if ((2 * max_samples_per_packet_ + 1) > MAX_SEND_BLOCKS) {
    max_samples_per_packet_ = (MAX_SEND_BLOCKS + 1) / 2 - 1;
    ACE_DEBUG((LM_WARNING,
               ACE_TEXT("(%P|%t) \"max_samples_per_packet\" is adjusted from %u to %u\n"),
               old_value, max_samples_per_packet_));
  }
}
