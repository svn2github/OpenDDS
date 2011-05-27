/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "Multicast.h"
#include "MulticastLoader.h"

#include "ace/Service_Config.h"

namespace OpenDDS {
namespace DCPS {

MulticastInitializer::MulticastInitializer()
{
  ACE_Service_Config::process_directive(ace_svc_desc_MulticastLoader);

#if (OPENDDS_MULTICAST_HAS_DLL == 0)
  ACE_Service_Config::process_directive(ACE_TEXT("static MulticastLoader"));
#endif  /* OPENDDS_MULTICAST_HAS_DLL */
}

} // namespace DCPS
} // namespace OpenDDS
