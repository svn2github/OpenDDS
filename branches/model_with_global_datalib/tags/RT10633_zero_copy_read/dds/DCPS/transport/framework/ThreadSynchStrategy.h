// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_THREADSYNCHSTRATEGY_H
#define TAO_DCPS_THREADSYNCHSTRATEGY_H

#include "dds/DCPS/dcps_export.h"

namespace TAO
{
  namespace DCPS
  {

    class ThreadSynch;
    class ThreadSynchResource;


//MJM: Some class documentation here would be extremely helpful.
    class TAO_DdsDcps_Export ThreadSynchStrategy
    {
      public:

        virtual ~ThreadSynchStrategy();

        virtual ThreadSynch* create_synch_object
                                   (ThreadSynchResource* synch_resource) = 0;


      protected:

        ThreadSynchStrategy();
    };

  } /* namespace DCPS */
} /* namespace TAO */

#if defined (__ACE_INLINE__)
#include "ThreadSynchStrategy.inl"
#endif /* __ACE_INLINE__ */

#endif  /* TAO_DCPS_THREADSYNCHSTRATEGY_H */
