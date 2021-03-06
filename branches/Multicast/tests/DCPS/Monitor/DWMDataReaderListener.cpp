/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>

#include <dds/DdsDcpsSubscriptionC.h>
#include <dds/DCPS/Service_Participant.h>

#include "DWMDataReaderListener.h"
#include <dds/monitor/monitorTypeSupportC.h>
#include <dds/monitor/monitorTypeSupportImpl.h>

#include <iostream>

DWMDataReaderListenerImpl::DWMDataReaderListenerImpl()
{
}

DWMDataReaderListenerImpl::~DWMDataReaderListenerImpl()
{
}

void DWMDataReaderListenerImpl::on_data_available(DDS::DataReader_ptr reader)
throw(CORBA::SystemException)
{
  try {
    OpenDDS::DCPS::DataWriterReportDataReader_var dwm_dr =
      OpenDDS::DCPS::DataWriterReportDataReader::_narrow(reader);

    if (CORBA::is_nil(dwm_dr.in())) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("%N:%l: on_data_available()")
                 ACE_TEXT(" ERROR: _narrow failed!\n")));
      ACE_OS::exit(-1);
    }

    OpenDDS::DCPS::DataWriterReport dwr;
    DDS::SampleInfo si;

    DDS::ReturnCode_t status = dwm_dr->take_next_sample(dwr, si) ;

    if (status == DDS::RETCODE_OK) {
      std::cout << "SampleInfo.sample_rank = " << si.sample_rank << std::endl;
      std::cout << "SampleInfo.instance_state = " << si.instance_state << std::endl;

      if (si.valid_data) {
        std::cout << "DataWriterReport:" << std::endl
                  << "  dw_id        = " << dwr.dw_id    << std::endl
                  << "  topic_id     = " << dwr.topic_id << std::endl;
        std::cout << "  instances    = " << std::endl;
        for (CORBA::ULong i = 0; i < dwr.instances.length(); i++) {
          std::cout << "    " << dwr.instances[i] << std::endl;
        }
        std::cout << "  associations = " << std::endl;
        for (CORBA::ULong i = 0; i < dwr.associations.length(); i++) {
          std::cout << "    dr_id = " << dwr.associations[i].dr_id << std::endl;
        }

      } else if (si.instance_state == DDS::NOT_ALIVE_DISPOSED_INSTANCE_STATE) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: instance is disposed\n")));

      } else if (si.instance_state == DDS::NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: instance is unregistered\n")));

      } else {
        ACE_ERROR((LM_ERROR,
                   ACE_TEXT("%N:%l: on_data_available()")
                   ACE_TEXT(" ERROR: unknown instance state: %d\n"),
                   si.instance_state));
      }

    } else {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("%N:%l: on_data_available()")
                 ACE_TEXT(" ERROR: unexpected status: %d\n"),
                 status));
    }

  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in on_data_available():");
    ACE_OS::exit(-1);
  }
}

void DWMDataReaderListenerImpl::on_requested_deadline_missed(
  DDS::DataReader_ptr,
  const DDS::RequestedDeadlineMissedStatus &)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_requested_deadline_missed()\n")));
}

void DWMDataReaderListenerImpl::on_requested_incompatible_qos(
  DDS::DataReader_ptr,
  const DDS::RequestedIncompatibleQosStatus &)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_requested_incompatible_qos()\n")));
}

void DWMDataReaderListenerImpl::on_liveliness_changed(
  DDS::DataReader_ptr,
  const DDS::LivelinessChangedStatus &)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_liveliness_changed()\n")));
}

void DWMDataReaderListenerImpl::on_subscription_matched(
  DDS::DataReader_ptr,
  const DDS::SubscriptionMatchedStatus &)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_subscription_matched()\n")));
}

void DWMDataReaderListenerImpl::on_sample_rejected(
  DDS::DataReader_ptr,
  const DDS::SampleRejectedStatus&)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_sample_rejected()\n")));
}

void DWMDataReaderListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
throw(CORBA::SystemException)
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%N:%l: INFO: on_sample_lost()\n")));
}
