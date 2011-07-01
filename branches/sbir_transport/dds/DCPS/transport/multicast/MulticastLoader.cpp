/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "MulticastLoader.h"
#include "MulticastGenerator.h"
#include "MulticastInst.h"

#include "dds/DCPS/transport/framework/TheTransportFactory.h"
#include "dds/DCPS/transport/framework/TransportType.h"

namespace OpenDDS {
namespace DCPS {

class MulticastType : public TransportType {
public:
  const char* name() { return "multicast"; }

  TransportInst* new_inst(const std::string& name)
  {
    return new MulticastInst(name);
  }

  static TransportInst* new_default_unreliable()
  {
    MulticastInst* mi = new MulticastInst(TransportRegistry::DEFAULT_INST_PREFIX
                                          + "0410_MCAST_UNRELIABLE");
    mi->reliable_ = false;
    return mi;
  }

  static TransportInst* new_default_reliable()
  {
   return new MulticastInst(TransportRegistry::DEFAULT_INST_PREFIX
                            + "0420_MCAST_RELIABLE");
  }
};

int
MulticastLoader::init(int /*argc*/, ACE_TCHAR* /*argv*/[])
{
  static bool initialized(false);

  if (initialized) return 0;  // already initialized

  TransportRegistry* registry = TheTransportRegistry;
  registry->register_type(new MulticastType);
  TransportConfig_rch cfg =
    registry->get_config(TransportRegistry::DEFAULT_CONFIG_NAME);
  cfg->sorted_insert(MulticastType::new_default_unreliable());
  cfg->sorted_insert(MulticastType::new_default_reliable());

  TheTransportFactory->register_generator(ACE_TEXT("multicast"),
                                          new MulticastGenerator);
  initialized = true;

  return 0;
}

ACE_FACTORY_DEFINE(OpenDDS_Multicast, MulticastLoader)
ACE_STATIC_SVC_DEFINE(
  MulticastLoader,
  ACE_TEXT("OpenDDS_Multicast"),
  ACE_SVC_OBJ_T,
  &ACE_SVC_NAME(MulticastLoader),
  ACE_Service_Type::DELETE_THIS | ACE_Service_Type::DELETE_OBJ,
  0)

} // namespace DCPS
} // namespace OpenDDS
