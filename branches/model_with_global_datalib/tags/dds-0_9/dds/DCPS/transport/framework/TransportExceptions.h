// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_TRANSPORT_EXCEPTIONS_H
#define TAO_DCPS_TRANSPORT_EXCEPTIONS_H

namespace TAO
{
  namespace DCPS
  {

    namespace Transport
    {
      class Exception {};
      class NotFound       : public Exception {};
      class Duplicate      : public Exception {};
      class UnableToCreate : public Exception {};
      class MiscProblem    : public Exception {};
      class NotConfigured  : public Exception {};
      class ConfigurationConflict  : public Exception {};
    }

  }  /* namespace DCPS */

}  /* namespace TAO */

#endif  /* TAO_DCPS_TRANSPORT_EXCEPTIONS_H */
