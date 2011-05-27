/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "SPMonitorImpl.h"
#include "MonitorFactoryImpl.h"
#include "monitorC.h"
#include "monitorTypeSupportImpl.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/DomainParticipantImpl.h"
#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>

namespace OpenDDS {
namespace DCPS {


SPMonitorImpl::SPMonitorImpl(MonitorFactoryImpl* monitor_factory,
                             Service_Participant* sp)
    : sp_(sp),
      monitor_factory_(monitor_factory)
{
  char host[256];
  ACE_OS::hostname(host, 256);
  hostname_ = host;
}

SPMonitorImpl::~SPMonitorImpl()
{
}

void
SPMonitorImpl::report()
{
  if (CORBA::is_nil(this->sp_writer_.in())) {
    this->sp_writer_ = this->monitor_factory_->get_sp_writer();
  }

  // If the SP writer is not available, it is too soon to report
  if (!CORBA::is_nil(this->sp_writer_.in())) {
    ServiceParticipantReport report;
    report.host = hostname_.c_str();
    report.pid  = ACE_OS::getpid();
    DDS::DomainParticipantFactory_var pf = TheParticipantFactory;
    const DomainParticipantFactoryImpl::DPMap& participants =
      dynamic_cast<DomainParticipantFactoryImpl*>(pf.in())->participants();
    CORBA::ULong length = 0;
    for (DomainParticipantFactoryImpl::DPMap::const_iterator mapIter = participants.begin();
         mapIter != participants.end();
         ++mapIter) {
      for (DomainParticipantFactoryImpl::DPSet::const_iterator iter = mapIter->second.begin();
           iter != mapIter->second.end();
           ++iter) {
        report.domain_participants.length(length+1);
        report.domain_participants[length] = iter->svt_->get_id();
        length++;
      }
    }
    length = 0;
    const TransportFactory::ImplMap& transports =
      TransportFactory::instance()->get_transport_impl_map();
    for (TransportFactory::ImplMap::const_iterator mapIter = transports.begin();
         mapIter != transports.end();
         ++mapIter) {
      report.transports.length(length+1);
      report.transports[length] = mapIter->first;
      length++;
    }
    this->sp_writer_->write(report, DDS::HANDLE_NIL);
  }
}
  
} // namespace DCPS
} // namespace OpenDDS

