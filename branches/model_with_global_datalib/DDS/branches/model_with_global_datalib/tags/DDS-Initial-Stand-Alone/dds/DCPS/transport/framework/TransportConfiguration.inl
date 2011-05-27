// -*- C++ -*-
//
// $Id$

#include  "TransportDefs.h"
#include  "PerConnectionSynchStrategy.h"
#include  "EntryExit.h"


// TBD SOON - The ThreadStrategy objects need to be reference counted.
// TBD - Resolve Mike's questions/comments.
//MJM: Shouldn't we just treat them as *_ptr thingies here?  And not
//MJM: worry about ownership within the configuration object?  I guess I
//MJM: am assuming that the configuration object is just created to pass
//MJM: information about, not to maintain the information for a
//MJM: specified lifetime.  I could be very wrong about the use case.

ACE_INLINE
TAO::DCPS::TransportConfiguration::TransportConfiguration()
  : swap_bytes_(0),
    queue_links_per_pool_(DEFAULT_CONFIG_QUEUE_LINKS_PER_POOL),
    queue_initial_pools_(DEFAULT_CONFIG_QUEUE_INITIAL_POOLS),
    max_packet_size_(DEFAULT_CONFIG_MAX_PACKET_SIZE),
    max_samples_per_packet_(DEFAULT_CONFIG_MAX_SAMPLES_PER_PACKET),
    optimum_packet_size_(DEFAULT_CONFIG_OPTIMUM_PACKET_SIZE)
    
{
  DBG_ENTRY("TransportConfiguration","TransportConfiguration");
  this->send_thread_strategy_ =  new PerConnectionSynchStrategy();

  // Ensure that the number of samples put into the packet does
  // not exceed the packet size.
  if ((2 * max_samples_per_packet_ + 1) > MAX_SEND_BLOCKS)
  {
    max_samples_per_packet_ = (MAX_SEND_BLOCKS + 1) / 2 - 1;
  }
}


// TBD - Resolve Mike's questions/comments.
//MJM: This is where we need to decide on ownership, 'nest pas?
ACE_INLINE
void
TAO::DCPS::TransportConfiguration::send_thread_strategy
                                         (ThreadSynchStrategy* strategy)
{
  DBG_SUB_ENTRY("TransportConfiguration","send_thread_strategy",1);

  if (this->send_thread_strategy_ != 0)
    {
      delete this->send_thread_strategy_;
    }

  this->send_thread_strategy_ = strategy;
}


ACE_INLINE
TAO::DCPS::ThreadSynchStrategy*
TAO::DCPS::TransportConfiguration::send_thread_strategy()
{
  DBG_SUB_ENTRY("TransportConfiguration","send_thread_strategy",2);
  return this->send_thread_strategy_;
}
