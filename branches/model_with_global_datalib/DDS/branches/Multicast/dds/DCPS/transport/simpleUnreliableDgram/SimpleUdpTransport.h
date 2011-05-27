/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_SIMPLEUDPTRANSPORT_H
#define OPENDDS_DCPS_SIMPLEUDPTRANSPORT_H

#include "SimpleUnreliableDgram_export.h"
#include "SimpleUnreliableDgramTransport.h"

namespace OpenDDS {
namespace DCPS {

class SimpleUnreliableDgram_Export SimpleUdpTransport : public SimpleUnreliableDgramTransport {
public:

  SimpleUdpTransport();
  virtual ~SimpleUdpTransport();

protected:

  virtual int configure_socket(TransportConfiguration* config);

  virtual int connection_info_i
  (TransportInterfaceInfo& local_info) const;

  virtual void deliver_sample(ReceivedDataSample&  sample,
                              const ACE_INET_Addr& remote_address,
                              CORBA::Long          priority);

private:

  ACE_TString    local_address_str_;
  ACE_INET_Addr  local_address_;
};

} // namespace DCPS
} // namespace OpenDDS

#if defined (__ACE_INLINE__)
#include "SimpleUdpTransport.inl"
#endif /* __ACE_INLINE__ */

#endif  /* OPENDDS_DCPS_SIMPLEUDPTRANSPORT_H */
