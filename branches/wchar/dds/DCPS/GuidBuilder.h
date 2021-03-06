/*
 * $Id$
 */

#ifndef DCPS_GUIDBUILDER_H
#define DCPS_GUIDBUILDER_H

#include "dds/DdsDcpsGuidC.h"

#include "GuidUtils.h"

#include "dcps_export.h"

namespace OpenDDS
{
namespace DCPS
{
class OpenDDS_Dcps_Export GuidBuilder
{
public:
  GuidBuilder();
  explicit GuidBuilder(GUID_t& guid);

  ~GuidBuilder();

  static GUID_t create();

  void guidPrefix0(long p0);  
  void guidPrefix1(long p1);
  void guidPrefix2(long p2);
  
  void entityId(EntityId_t entityId);
  void entityId(long entityId);
  
  void entityKey(long entityKey);
 
  void entityKind(CORBA::Octet entityKind);
  void entityKind(EntityKind kind); 

  operator GUID_t();

private:
  GUID_t  guid_cxx_;
  GUID_t& guid_;
};

} // namespace
} // namespace

#ifdef __ACE_INLINE__
# include "GuidBuilder.inl"
#endif /* __ACE_INLINE__ */

#endif /* DCPS_REPOIDBUILDER_H */
