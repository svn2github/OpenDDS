// -*- C++ -*-
//
// $Id$
#include "DataReaderListener.h"
#include "MessengerTypeSupportC.h"
#include "MessengerTypeSupportImpl.h"
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Qos_Helper.h>
#include <ace/streams.h>

#include "performance-tests/DCPS/dummyTCP/PerformanceTest.h"

using namespace Messenger;

// Implementation skeleton constructor
DataReaderListenerImpl::DataReaderListenerImpl()
  : num_reads_(0)
{
}

// Implementation skeleton destructor
DataReaderListenerImpl::~DataReaderListenerImpl ()
{
}

void DataReaderListenerImpl::on_data_available(DDS::DataReader_ptr reader)
  throw (CORBA::SystemException)
{

  PerformanceTest::stop_test("Subscriber Side Transport Performance Test",
                                   "DataReaderListenerImpl::on_data_available\n");

  num_reads_ ++;

  try {
    ::Messenger::MessageDataReader_var message_dr = ::Messenger::MessageDataReader::_narrow(reader);
    if (CORBA::is_nil (message_dr.in ())) {
      cerr << "read: _narrow failed." << endl;
      exit(1);
    }

    Messenger::Message message;
    DDS::SampleInfo si ;
    DDS::ReturnCode_t status = message_dr->take_next_sample(message, si) ;

    if (status == DDS::RETCODE_OK) {

      //ACE_Time_Value atv = OpenDDS::DCPS::time_to_time_value (message.timestamp);
      //const time_t ltime = atv.sec();
      //cout  << "Subscriber got the timestamp: " << ACE_OS::ctime (&ltime) << endl;
    } else if (status == DDS::RETCODE_NO_DATA) {
      cerr << "ERROR: reader received DDS::RETCODE_NO_DATA!" << endl;
    } else {
      cerr << "ERROR: read Message: Error: " <<  status << endl;
    }
  } catch (CORBA::Exception& e) {
    cerr << "Exception caught in read:" << endl << e << endl;
    exit(1);
  }
}

void DataReaderListenerImpl::on_requested_deadline_missed (
    DDS::DataReader_ptr,
    const DDS::RequestedDeadlineMissedStatus &)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_requested_deadline_missed" << endl;
}

void DataReaderListenerImpl::on_requested_incompatible_qos (
    DDS::DataReader_ptr,
    const DDS::RequestedIncompatibleQosStatus &)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_requested_incompatible_qos" << endl;
}

void DataReaderListenerImpl::on_liveliness_changed (
    DDS::DataReader_ptr,
    const DDS::LivelinessChangedStatus &)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_liveliness_changed" << endl;
}

void DataReaderListenerImpl::on_subscription_match (
    DDS::DataReader_ptr,
    const DDS::SubscriptionMatchStatus &)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_subscription_match" << endl;
}

void DataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr,
    const DDS::SampleRejectedStatus&)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_sample_rejected" << endl;
}

void DataReaderListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_sample_lost" << endl;
}

void DataReaderListenerImpl::on_subscription_disconnected (
  DDS::DataReader_ptr,
  const ::OpenDDS::DCPS::SubscriptionDisconnectedStatus &)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_subscription_disconnected" << endl;
}

void DataReaderListenerImpl::on_subscription_reconnected (
  DDS::DataReader_ptr,
  const ::OpenDDS::DCPS::SubscriptionReconnectedStatus &)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_subscription_reconnected" << endl;
}

void DataReaderListenerImpl::on_subscription_lost (
  DDS::DataReader_ptr,
  const ::OpenDDS::DCPS::SubscriptionLostStatus &)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_subscription_lost" << endl;
}

void DataReaderListenerImpl::on_connection_deleted (
  DDS::DataReader_ptr)
  throw (CORBA::SystemException)
{
  cerr << "DataReaderListenerImpl::on_connection_deleted" << endl;
}
