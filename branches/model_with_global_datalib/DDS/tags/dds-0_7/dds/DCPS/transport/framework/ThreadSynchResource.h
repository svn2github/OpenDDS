// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_THREADSYNCHRESOURCE_H
#define TAO_DCPS_THREADSYNCHRESOURCE_H

namespace TAO
{
  namespace DCPS
  {

    class ThreadSynchResource
    {
      public:

        virtual ~ThreadSynchResource();

        virtual void wait_to_unclog() = 0;


      protected:

        ThreadSynchResource();
    };

  } /* namespace DCPS */
} /* namespace TAO */

#if defined (__ACE_INLINE__)
#include "ThreadSynchResource.inl"
#endif /* __ACE_INLINE__ */

#endif  /* TAO_DCPS_THREADSYNCHRESOURCE_H */
