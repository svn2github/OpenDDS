/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_RELIABLEMULTICASTGENERATOR_H
#define OPENDDS_DCPS_RELIABLEMULTICASTGENERATOR_H

#include /**/ "ace/pre.h"
#include /**/ "ace/config-all.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "ReliableMulticast_Export.h"
#include "dds/DCPS/transport/framework/TransportGenerator.h"

namespace OpenDDS {
namespace DCPS {

//class TransportConfiguration;
//class TransportImplFactory;

class ReliableMulticast_Export ReliableMulticastTransportGenerator
  : public TransportGenerator {
public:
  virtual ~ReliableMulticastTransportGenerator();

  virtual TransportImplFactory* new_factory();

  virtual TransportConfiguration* new_configuration(const TransportIdType id);

  virtual void default_transport_ids(TransportIdList & ids);
};

} // namespace DCPS
} // namespace OpenDDS

#if defined (__ACE_INLINE__)
#include "ReliableMulticastTransportGenerator.inl"
#endif /* __ACE_INLINE__ */

#include /**/ "ace/post.h"

#endif /* OPENDDS_DCPS_RELIABLEMULTICASTGENERATOR_H */
