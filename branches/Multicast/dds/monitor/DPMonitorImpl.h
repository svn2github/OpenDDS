/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_DPMONITOR_IMPL_H
#define OPENDDS_DCPS_DPMONITOR_IMPL_H

#include "monitor_export.h"
#include "dds/DCPS/MonitorFactory.h"
#include "monitorTypeSupportImpl.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

namespace OpenDDS {
namespace DCPS {

class DPMonitorImpl : public Monitor {
public:
  DPMonitorImpl(DomainParticipantImpl* dp,
                OpenDDS::DCPS::DomainParticipantReportDataWriter_ptr dp_writer);
  virtual ~DPMonitorImpl();
  virtual void report();

private:
  DomainParticipantImpl* dp_;
  OpenDDS::DCPS::DomainParticipantReportDataWriter_var dp_writer_;
};

} // namespace DCPS
} // namespace OpenDDS

#endif /* OPENDDS_DCPS_DPMONITOR_IMPL_H */
