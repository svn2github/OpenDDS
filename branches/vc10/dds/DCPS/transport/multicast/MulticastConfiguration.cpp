/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "MulticastConfiguration.h"
#include "MulticastLoader.h"

#include "dds/DCPS/transport/framework/NullSynchStrategy.h"
#include "dds/DCPS/transport/framework/TransportDefs.h"

namespace {

const bool DEFAULT_TO_IPV6(false);

const u_short DEFAULT_PORT_OFFSET(49400);

const char* DEFAULT_IPV4_GROUP_ADDRESS("224.0.0.128");
const char* DEFAULT_IPV6_GROUP_ADDRESS("FF01::80");

const bool DEFAULT_RELIABLE(true);

const double DEFAULT_SYN_BACKOFF(2.0);
const long DEFAULT_SYN_INTERVAL(250);
const long DEFAULT_SYN_TIMEOUT(30000);

const size_t DEFAULT_NAK_DEPTH(32);
const long DEFAULT_NAK_INTERVAL(500);
const long DEFAULT_NAK_DELAY_INTERVALS(4);
const long DEFAULT_NAK_MAX(3);
const long DEFAULT_NAK_TIMEOUT(30000);

const char DEFAULT_TTL(1);

} // namespace

namespace OpenDDS {
namespace DCPS {

MulticastConfiguration::MulticastConfiguration()
  : TransportConfiguration(new NullSynchStrategy()),
    default_to_ipv6_(DEFAULT_TO_IPV6),
    port_offset_(DEFAULT_PORT_OFFSET),
    reliable_(DEFAULT_RELIABLE),
    syn_backoff_(DEFAULT_SYN_BACKOFF),
    nak_depth_(DEFAULT_NAK_DEPTH),
    nak_delay_intervals_(DEFAULT_NAK_DELAY_INTERVALS),
    nak_max_(DEFAULT_NAK_MAX),
    ttl_(DEFAULT_TTL),
#if defined (ACE_DEFAULT_MAX_SOCKET_BUFSIZ)
    rcv_buffer_size_(ACE_DEFAULT_MAX_SOCKET_BUFSIZ)
#else
    // Use system default values.
    rcv_buffer_size_(0)
#endif
{
  default_group_address(this->group_address_, DEFAULT_MULTICAST_ID);

  this->syn_interval_.msec(DEFAULT_SYN_INTERVAL);
  this->syn_timeout_.msec(DEFAULT_SYN_TIMEOUT);

  this->nak_interval_.msec(DEFAULT_NAK_INTERVAL);
  this->nak_timeout_.msec(DEFAULT_NAK_TIMEOUT);

  this->transport_type_ = MULTICAST_TRANSPORT_TYPE;
}

int
MulticastConfiguration::load(const TransportIdType& id,
                             ACE_Configuration_Heap& config)
{
  TransportConfiguration::load(id, config); // delegate to parent

  ACE_Configuration_Section_Key transport_key;

  ACE_TString section_name = id_to_section_name(id);
  if (config.open_section(config.root_section(),
                          section_name.c_str(),
                          0,  // create
                          transport_key) != 0) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("MulticastConfiguration::load: ")
                      ACE_TEXT("unable to open section: [%C]!\n"),
                      section_name.c_str()),
                     -1);
  }

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("default_to_ipv6"),
                   this->default_to_ipv6_, bool)

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("port_offset"),
                   this->port_offset_, u_short)

  ACE_TString group_address_s;
  GET_CONFIG_STRING_VALUE(config, transport_key, ACE_TEXT("group_address"),
                          group_address_s)
  if (group_address_s.is_empty()) {
    default_group_address(this->group_address_, id);
  } else {
    this->group_address_.set(group_address_s.c_str());
  }

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("reliable"),
                   this->reliable_, bool)

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("syn_backoff"),
                   this->syn_backoff_, double)

  GET_CONFIG_TIME_VALUE(config, transport_key, ACE_TEXT("syn_interval"),
                        this->syn_interval_)

  GET_CONFIG_TIME_VALUE(config, transport_key, ACE_TEXT("syn_timeout"),
                        this->syn_timeout_)

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("nak_depth"),
                   this->nak_depth_, size_t)

  GET_CONFIG_TIME_VALUE(config, transport_key, ACE_TEXT("nak_interval"),
                        this->nak_interval_)

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("nak_delay_intervals"),
                        this->nak_delay_intervals_, size_t)

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("nak_max"),
                        this->nak_max_, size_t)

  GET_CONFIG_TIME_VALUE(config, transport_key, ACE_TEXT("nak_timeout"),
                        this->nak_timeout_)

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("ttl"),
                   this->ttl_, char)

  GET_CONFIG_VALUE(config, transport_key, ACE_TEXT("rcv_buffer_size"),
                   this->rcv_buffer_size_, size_t)

   return 0;
}

void
MulticastConfiguration::default_group_address(ACE_INET_Addr& group_address,
                                              const TransportIdType& id)
{
  u_short port_number(this->port_offset_ + id);

  if (this->default_to_ipv6_) {
    group_address.set(port_number, DEFAULT_IPV6_GROUP_ADDRESS);
  } else {
    group_address.set(port_number, DEFAULT_IPV4_GROUP_ADDRESS);
  }
}


void
MulticastConfiguration::dump()
{
  // Acquire lock on the log so the entire dump is output as a block
  // (at least for each process).
  ACE_Log_Msg::instance()->acquire();

  TransportConfiguration::dump();

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("default_to_ipv6: %C.\n"),
             (this->default_to_ipv6_ ? "true" : "false")));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("port_offset: %d.\n"),
             this->port_offset_));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("group_address: %C:%d.\n"),
             this->group_address_.get_host_addr(),
             this->group_address_.get_port_number()));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("reliable: %C.\n"),
             (this->reliable_ ? "true" : "false")));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("syn_backoff: %l.\n"),
             this->syn_backoff_));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("syn_interval: %d.\n"),
             this->syn_interval_.msec()));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("syn_timeout: %d.\n"),
             this->syn_timeout_.msec()));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("nak_depth: %d.\n"),
             this->nak_depth_));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("nak_interval: %d.\n"),
             this->nak_interval_.msec()));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("nak_delay_intervals: %d.\n"),
             this->nak_delay_intervals_));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("nak_max: %d.\n"),
             this->nak_max_));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("nak_timeout: %d.\n"),
             this->nak_timeout_.msec()));
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
             ACE_TEXT("ttl: 0x%x.\n"),
             this->ttl_));

  if (this->rcv_buffer_size_ == 0) {
    ACE_DEBUG((LM_DEBUG,
         ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
         ACE_TEXT("rcv_buffer_size: %C.\n\n"),
         "System Default Value"));
  } else {
    ACE_DEBUG((LM_DEBUG,
         ACE_TEXT("(%P|%t) MulticastConfiguration::dump() - ")
         ACE_TEXT("rcv_buffer_size: %d.\n\n"),
         this->rcv_buffer_size_));
  }

  ACE_Log_Msg::instance()->release();
}

} // namespace DCPS
} // namespace OpenDDS
