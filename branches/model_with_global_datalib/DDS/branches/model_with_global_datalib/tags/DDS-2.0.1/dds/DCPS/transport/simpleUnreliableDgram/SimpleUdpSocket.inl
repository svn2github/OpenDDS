/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "dds/DCPS/transport/framework/EntryExit.h"
#include "ace/Synch.h"

ACE_INLINE
OpenDDS::DCPS::SimpleUdpSocket::SimpleUdpSocket()
{
  DBG_ENTRY_LVL("SimpleUdpSocket","SimpleUdpSocket",6);
}

ACE_INLINE
ACE_HANDLE
OpenDDS::DCPS::SimpleUdpSocket::get_handle() const
{
  DBG_ENTRY_LVL("SimpleUdpSocket","get_handle",6);
  return this->socket_.get_handle();
}

ACE_INLINE
ACE_SOCK&
OpenDDS::DCPS::SimpleUdpSocket::socket()
{
  DBG_ENTRY_LVL("SimpleUdpSocket","socket",6);
  return this->socket_;
}

ACE_INLINE int
OpenDDS::DCPS::SimpleUdpSocket::open_socket(ACE_INET_Addr& local_address,
                                            const ACE_INET_Addr& multicast_group_address,
                                            bool receiver)
{
  DBG_ENTRY_LVL("SimpleUdpSocket","open_socket",6);

  ACE_UNUSED_ARG(multicast_group_address);
  ACE_UNUSED_ARG(receiver);
  int result = -1;

  GuardType guard(this->lock_);

  result = this->socket_.open(local_address);

  if (result == 0) {
    ACE_INET_Addr address;

    if (this->socket_.get_local_addr(address) != 0) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: SimpleUdpSocket::open_socket ")
                        ACE_TEXT("- %p"),
                        ACE_TEXT("cannot get local addr\n")),
                       -1);
    }

    local_address.set_port_number(address.get_port_number());
    this->local_address_ = local_address;

  } else {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: SimpleUdpSocket::open_socket ")
                      ACE_TEXT("- %p"),
                      ACE_TEXT("open\n")),
                     -1);
  }

  return 0;
}

ACE_INLINE void
OpenDDS::DCPS::SimpleUdpSocket::close_socket()
{
  DBG_ENTRY_LVL("SimpleUdpSocket","close_socket",6);

  // Make sure that no other thread is send()'ing to the socket_ right now.
  GuardType guard(this->lock_);
  this->socket_.close();

  this->remove_receive_strategy();
}

ACE_INLINE ssize_t
OpenDDS::DCPS::SimpleUdpSocket::send_bytes(const iovec iov[],
                                           int   n,
                                           const ACE_INET_Addr& remote_address)
{
  DBG_ENTRY_LVL("SimpleUdpSocket","send_bytes",6);

  // Protect the socket from multiple threads attempting to send at once.
  GuardType guard(this->lock_);

  return this->socket_.send(iov,n,remote_address);
}

ACE_INLINE ssize_t
OpenDDS::DCPS::SimpleUdpSocket::receive_bytes(iovec iov[],
                                              int   n,
                                              ACE_INET_Addr& remote_address)
{
  DBG_ENTRY_LVL("SimpleUdpSocket","receive_bytes",6);

  return this->socket_.recv(iov, n, remote_address);
}
