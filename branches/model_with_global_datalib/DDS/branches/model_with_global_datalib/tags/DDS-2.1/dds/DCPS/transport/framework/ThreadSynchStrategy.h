/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_THREADSYNCHSTRATEGY_H
#define OPENDDS_DCPS_THREADSYNCHSTRATEGY_H

#include "dds/DCPS/dcps_export.h"

namespace OpenDDS {
namespace DCPS {

class ThreadSynch;
class ThreadSynchResource;

//MJM: Some class documentation here would be extremely helpful.
class OpenDDS_Dcps_Export ThreadSynchStrategy {
public:

  virtual ~ThreadSynchStrategy();

  virtual ThreadSynch* create_synch_object(
    ThreadSynchResource* synch_resource,
    long                 priority,
    int                  scheduler) = 0;

protected:

  ThreadSynchStrategy();
};

} // namespace DCPS
} // namespace OpenDDS

#if defined (__ACE_INLINE__)
#include "ThreadSynchStrategy.inl"
#endif /* __ACE_INLINE__ */

#endif  /* OPENDDS_DCPS_THREADSYNCHSTRATEGY_H */
