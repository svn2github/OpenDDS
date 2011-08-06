/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "dds/DCPS/dcps_export.h"

#include "DDSTEST.h"

#include "ace/Log_Msg.h"

#include "dds/DCPS/EntityImpl.h"
#include "dds/DdsDcpsDomainC.h"

#include "dds/DCPS/transport/framework/TransportClient.h"
#include "dds/DCPS/transport/framework/TransportConfig.h"

#include <vector>

bool
::DDS_TEST::supports(const OpenDDS::DCPS::EntityImpl* entity, const std::string& protocol_name)
{

  const OpenDDS::DCPS::TransportConfig_rch tc = entity->transport_config();

  if (tc.is_nil())
    {
      ACE_ERROR_RETURN((LM_INFO,
                        ACE_TEXT("(%P|%t) Null transport config for entity %@.\n"),
                        entity),
                       0);
    }

  for (std::vector<OpenDDS::DCPS::TransportInst_rch>::const_iterator it = tc->instances_.begin(); it != tc->instances_.end(); ++it)
    {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) Checking '%C' == '%C' ...\n"),
                 protocol_name.c_str(),
                 (*it)->name().c_str()));

      if ((*it)->name() == protocol_name)
        {
          ACE_ERROR_RETURN((LM_INFO,
                            ACE_TEXT("(%P|%t) Found transport '%C'\n."), protocol_name.c_str()),
                           true);
        }
    }

  ACE_ERROR_RETURN((LM_INFO,
                    ACE_TEXT("(%P|%t) Unable to find transport %C.\n"),
                    protocol_name.c_str()),
                   false);
}

bool
::DDS_TEST::supports(const OpenDDS::DCPS::TransportClient* tc, const std::string& name)
{
  if (tc == 0)
    {
      ACE_ERROR_RETURN((LM_INFO,
                        ACE_TEXT("(%P|%t) Null transport client.\n")),
                       0);
    }

  for (std::vector<OpenDDS::DCPS::TransportImpl_rch>::const_iterator it = tc->impls_.begin();
          it != tc->impls_.end(); ++it)
    {

      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) Checking '%C' == '%C' ...\n"),
                 name.c_str(),
                 (*it)->config()->name().c_str()));

      if ((*it)->config()->name() == name)
        {
          ACE_ERROR_RETURN((LM_INFO,
                            ACE_TEXT("(%P|%t) Found transport '%C'.\n"), name.c_str()),
                           1);
        }
    }

  ACE_ERROR_RETURN((LM_INFO,
                    ACE_TEXT("(%P|%t) Unable to find transport %C.\n"),
                    name.c_str()),
                   0);
}

