// -*- C++ -*-
//
// $Id$
#include "DataReaderListener.h"
#include "MessengerTypeSupportC.h"
#include "MessengerTypeSupportImpl.h"
#include <dds/DCPS/Service_Participant.h>
#include <ace/streams.h>

using namespace Messenger;

// Implementation skeleton constructor
DataReaderListenerImpl::DataReaderListenerImpl()
  : num_reads_(0), last_non_durable_(0)
{
}

// Implementation skeleton destructor
DataReaderListenerImpl::~DataReaderListenerImpl ()
{
}

void DataReaderListenerImpl::on_data_available(DDS::DataReader_ptr reader)
{
  DDS::DataReaderQos qos;
  reader->get_qos(qos);
  const bool durable = qos.durability.kind > DDS::VOLATILE_DURABILITY_QOS;

  if (durable) ++num_reads_;

  try {
    MessageDataReader_var message_dr = MessageDataReader::_narrow(reader);
    if (CORBA::is_nil(message_dr)) {
      cerr << "read: _narrow failed." << endl;
      exit(1);
    }

    Messenger::Message message;
    DDS::SampleInfo si;
    DDS::ReturnCode_t status = message_dr->take_next_sample(message, si);

    if (status == DDS::RETCODE_OK) {
      cout << (durable ? "Durable " : "Volatile")
           << " count = " << message.count << endl;
      if (durable && (message.count != num_reads_)) {
        cerr << "ERROR: durable reader received out-of-order data" << endl;
      } else if (!durable) {
        if (last_non_durable_ && message.count != last_non_durable_ + 1) {
          cerr << "ERROR: volatile reader received out-of-order data" << endl;
        }
        last_non_durable_ = message.count;
      }
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
{
  cerr << "DataReaderListenerImpl::on_requested_deadline_missed" << endl;
}

void DataReaderListenerImpl::on_requested_incompatible_qos (
    DDS::DataReader_ptr,
    const DDS::RequestedIncompatibleQosStatus &)
{
  cerr << "DataReaderListenerImpl::on_requested_incompatible_qos" << endl;
}

void DataReaderListenerImpl::on_liveliness_changed (
    DDS::DataReader_ptr,
    const DDS::LivelinessChangedStatus &)
{
  cerr << "DataReaderListenerImpl::on_liveliness_changed" << endl;
}

void DataReaderListenerImpl::on_subscription_matched (
    DDS::DataReader_ptr,
    const DDS::SubscriptionMatchedStatus &)
{
  cerr << "DataReaderListenerImpl::on_subscription_matched" << endl;
}

void DataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr,
    const DDS::SampleRejectedStatus&)
{
  cerr << "DataReaderListenerImpl::on_sample_rejected" << endl;
}

void DataReaderListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
{
  cerr << "DataReaderListenerImpl::on_sample_lost" << endl;
}
