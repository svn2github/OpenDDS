/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "dds/DCPS/transport/framework/EntryExit.h"

ACE_INLINE
OpenDDS::DCPS::ReliableMulticast::detail::Packet::Packet(
  id_type id,
  const PacketType& type,
  id_type begin,
  id_type end)
  : id_(id)
  , type_(type)
  , nack_begin_(begin)
  , nack_end_(end)
{
}
