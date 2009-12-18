/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "UdpDataLink.h"
#include "UdpTransport.h"

#include "ace/Log_Msg.h"
#include "ace/OS_NS_string.h"

#ifndef __ACE_INLINE__
# include "UdpDataLink.inl"
#endif  /* __ACE_INLINE__ */

namespace OpenDDS {
namespace DCPS {

UdpDataLink::UdpDataLink(UdpTransport* transport)
  : DataLink(transport,
             0) // priority
{
}

bool
UdpDataLink::open(const ACE_INET_Addr& remote_address)
{
  int error;

  ACE_INET_Addr& local_address(this->config_->local_address_);
  if ((error = this->socket_.open(local_address)) != 0) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("UdpDataLink::open: open failed: %C\n"),
                      ACE_OS::strerror(error)),
                     false);
  }

  if ((error = start(this->send_strategy_.in(),
                     this->recv_strategy_.in())) != 0) {
    stop_i();
    ACE_ERROR_RETURN((LM_ERROR,
		      ACE_TEXT("(%P|%t) ERROR: ")
		      ACE_TEXT("UdpDataLink::open: start failed: %d\n"),
                      error),
                     false);
  }

  this->remote_address_ = remote_address;

  return true;
}

void
UdpDataLink::stop_i()
{
  this->socket_.close();
}

} // namespace DCPS
} // namespace OpenDDS
