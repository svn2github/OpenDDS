/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "PublisherMonitorImpl.h"
#include "monitorC.h"
#include "monitorTypeSupportImpl.h"
#include "dds/DCPS/PublisherImpl.h"
#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>
#include <dds/DCPS/DomainParticipantImpl.h>

namespace OpenDDS {
namespace DCPS {


PublisherMonitorImpl::PublisherMonitorImpl(PublisherImpl* pub,
              OpenDDS::DCPS::PublisherReportDataWriter_ptr pub_writer)
  : pub_(pub),
    pub_writer_(PublisherReportDataWriter::_duplicate(pub_writer))
{
}

PublisherMonitorImpl::~PublisherMonitorImpl()
{
}

void
PublisherMonitorImpl::report() {
  if (!CORBA::is_nil(this->pub_writer_.in())) {
    PublisherReport report;
    report.handle = pub_->get_instance_handle();
    DDS::DomainParticipant_var dp = pub_->get_participant();
    report.dp_id   = dynamic_cast<DomainParticipantImpl*>(dp.in())->get_id();
    TransportImpl_rch ti = pub_->get_transport_impl();
    //report.transport_id = // No direct way to look up the transport ID
    PublisherImpl::PublicationIdVec writers;
    pub_->get_publication_ids(writers);
    CORBA::ULong length = 0;
    for (PublisherImpl::PublicationIdVec::iterator iter = writers.begin();
         iter != writers.end();
         ++iter) {
      report.writers.length(length+1);
      report.writers[length] = *iter;
      length++;
    }
    
    //report.writers  = 
    this->pub_writer_->write(report, DDS::HANDLE_NIL);
  }
}


} // namespace DCPS
} // namespace OpenDDS

