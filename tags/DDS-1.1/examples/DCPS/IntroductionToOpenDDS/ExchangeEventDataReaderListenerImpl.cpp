// -*- C++ -*-
// *******************************************************************
//
// (c) Copyright 2006, Object Computing, Inc.
// All Rights Reserved.
//
// *******************************************************************

#include "ExchangeEventDataReaderListenerImpl.h"
#include "ExchangeEventTypeSupportC.h"
#include "ExchangeEventTypeSupportImpl.h"
#include <dds/DCPS/Service_Participant.h>
#include <ace/streams.h>

// Implementation skeleton constructor
ExchangeEventDataReaderListenerImpl::ExchangeEventDataReaderListenerImpl()
: is_exchange_closed_received_( 0 )
{
}

// Implementation skeleton destructor
ExchangeEventDataReaderListenerImpl::~ExchangeEventDataReaderListenerImpl ()
{
}

// app-specific
CORBA::Boolean
ExchangeEventDataReaderListenerImpl::is_exchange_closed_received()
{
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->is_exchange_closed_received_;
}


void ExchangeEventDataReaderListenerImpl::on_data_available(DDS::DataReader_ptr reader)
  throw (CORBA::SystemException)
{
  try {
    StockQuoter::ExchangeEventDataReader_var exchange_evt_dr 
      = StockQuoter::ExchangeEventDataReader::_narrow(reader);
    if (CORBA::is_nil (exchange_evt_dr.in ())) {
      cerr << "ExchangeEventDataReaderListenerImpl::on_data_available: _narrow failed." << endl;
      ACE_OS::exit(1);
    }

    StockQuoter::ExchangeEvent exchange_evt;
    DDS::SampleInfo si ;
    DDS::ReturnCode_t status = exchange_evt_dr->take_next_sample(exchange_evt, si) ;

    if (status == DDS::RETCODE_OK) {
      cout << "ExchangeEvent: exchange  = " << exchange_evt.exchange.in() << endl;

      switch ( exchange_evt.event ) {
        case StockQuoter::TRADING_OPENED:
          cout << "               event     = TRADING_OPENED" << endl;
          break;
        case StockQuoter::TRADING_CLOSED: {
          cout << "               event     = TRADING_CLOSED" << endl;
          ACE_Guard<ACE_Mutex> guard(this->lock_);
          this->is_exchange_closed_received_ = 1;
          break;
        }
        case StockQuoter::TRADING_SUSPENDED:
          cout << "               event     = TRADING_SUSPENDED" << endl;
          break;
        case StockQuoter::TRADING_RESUMED:
          cout << "               event     = TRADING_RESUMED" << endl;
          break;
        default:
          cerr << "ERROR: reader received unknown ExchangeEvent: " << exchange_evt.event << endl;
      }

      cout << "               timestamp = " << exchange_evt.timestamp      << endl;

      cout << "SampleInfo.sample_rank = " << si.sample_rank << endl;
    } else if (status == DDS::RETCODE_NO_DATA) {
      cerr << "ERROR: reader received DDS::RETCODE_NO_DATA!" << endl;
    } else {
      cerr << "ERROR: read ExchangeEvent: Error: " <<  status << endl;
    }
  } catch (CORBA::Exception& e) {
    cerr << "Exception caught in read:" << endl << e << endl;
    ACE_OS::exit(1);
  }
}

void ExchangeEventDataReaderListenerImpl::on_requested_deadline_missed (
    DDS::DataReader_ptr,
    const DDS::RequestedDeadlineMissedStatus &)
  throw (CORBA::SystemException)
{
  cerr << "ExchangeEventDataReaderListenerImpl::on_requested_deadline_missed" << endl;
}

void ExchangeEventDataReaderListenerImpl::on_requested_incompatible_qos (
    DDS::DataReader_ptr,
    const DDS::RequestedIncompatibleQosStatus &)
  throw (CORBA::SystemException)
{
  cerr << "ExchangeEventDataReaderListenerImpl::on_requested_incompatible_qos" << endl;
}

void ExchangeEventDataReaderListenerImpl::on_liveliness_changed (
    DDS::DataReader_ptr,
    const DDS::LivelinessChangedStatus &)
  throw (CORBA::SystemException)
{
  cerr << "ExchangeEventDataReaderListenerImpl::on_liveliness_changed" << endl;
}

void ExchangeEventDataReaderListenerImpl::on_subscription_match (
    DDS::DataReader_ptr,
    const DDS::SubscriptionMatchStatus &)
  throw (CORBA::SystemException)
{
  cerr << "ExchangeEventDataReaderListenerImpl::on_subscription_match" << endl;
}

void ExchangeEventDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr,
    const DDS::SampleRejectedStatus&)
  throw (CORBA::SystemException)
{
  cerr << "ExchangeEventDataReaderListenerImpl::on_sample_rejected" << endl;
}

void ExchangeEventDataReaderListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
  throw (CORBA::SystemException)
{
  cerr << "ExchangeEventDataReaderListenerImpl::on_sample_lost" << endl;
}
