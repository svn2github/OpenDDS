// -*- C++ -*-
//
// $Id$
#include "DataReaderListener.h"
#include "dds/DCPS/Service_Participant.h"
#include "../TypeNoKeyBounded/Pt128TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt512TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt2048TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt8192TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt128TypeSupportImpl.h"
#include "../TypeNoKeyBounded/Pt512TypeSupportImpl.h"
#include "../TypeNoKeyBounded/Pt2048TypeSupportImpl.h"
#include "../TypeNoKeyBounded/Pt8192TypeSupportImpl.h"

extern long subscriber_delay_msec; // from common.h

// defined this to build with code only supporting the zero-copy type
// but strongly supporting it.
// Comment out this line to build with code that supports the default use of zero-copy
// but also supports the original sequence type.
#define WITH_ZERO_COPY_ONLY

template<class Tseq, class Iseq, class R, class R_ptr, class R_var, class Rimpl>
int read (::DDS::DataReader_ptr reader, bool use_zero_copy_reads)
{
  // TWF: There is an optimization to the test by
  // using a pointer to the known servant and
  // static_casting it to the servant
  R_var var_dr
    = R::_narrow(reader);

  R_ptr pt_dr = var_dr.ptr();
  Rimpl* dr_servant =
      ::TAO::DCPS::reference_to_servant< Rimpl, R_ptr>
              (pt_dr);

  if (subscriber_delay_msec)
    {
      ACE_Time_Value delay ( subscriber_delay_msec / 1000,
                            (subscriber_delay_msec % 1000) * 1000);
      ACE_OS::sleep (delay);
    }

  const ::CORBA::Long max_read_samples = 100;
#ifdef WITH_ZERO_COPY_ONLY
  Tseq samples(use_zero_copy_reads ? 0 : max_read_samples, max_read_samples);
  Iseq infos(  use_zero_copy_reads ? 0 : max_read_samples, max_read_samples);
#else
  Tseq samples(0); //max_read_samples);
  Iseq infos(0); //max_read_samples);
#endif

  int samples_recvd = 0;
  DDS::ReturnCode_t status;
  // initialize to zero.

  status = dr_servant->read (
    samples,
    infos,
    max_read_samples,
    ::DDS::NOT_READ_SAMPLE_STATE,
    ::DDS::ANY_VIEW_STATE,
    ::DDS::ANY_INSTANCE_STATE);

  if (status == ::DDS::RETCODE_OK)
    {
      samples_recvd = samples.length ();
    }
  else if (status == ::DDS::RETCODE_NO_DATA)
    {
      ACE_ERROR((LM_ERROR, " Empty read!\n"));
    }
  else
    {
        ACE_OS::printf (" read  data: Error: %d\n", status) ;
    }

  status = dr_servant->return_loan(samples, infos);
  if (status != ::DDS::RETCODE_OK)
  {
      // TBD - why is printf used for errors?
        ACE_OS::printf (" return_loan: Error: %d\n", status) ;
      ACE_ERROR((LM_ERROR, " return_loan gave error status %d\n", status));

  }

  return samples_recvd;
}











// Implementation skeleton constructor
DataReaderListenerImpl::DataReaderListenerImpl (int num_publishers,
                                                int num_samples,
                                                int data_size,
                                                int read_interval,
                                                bool use_zero_copy_reads)
:
  samples_lost_count_(0),
  samples_rejected_count_(0),
  samples_received_count_(0),
  total_samples_count_(0),
  read_interval_ (read_interval),
  use_zero_copy_reads_(use_zero_copy_reads),
  num_publishers_(num_publishers),
  num_samples_ (num_samples),
  data_size_ (data_size),
  num_floats_per_sample_ (data_size)
  {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) DataReaderListenerImpl::DataReaderListenerImpl\n")));

    stats_.init(num_publishers_, num_samples_, data_size_, num_floats_per_sample_*4 + 8);

    int total_samples = num_publishers_ * num_samples_;
    if (0 != total_samples % read_interval_)
    {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("ERROR: read_interval %d is not an even divisor of the total number of samples %d\n")
        ACE_TEXT("ERROR: The subscriber will never finish!\n"),
        read_interval_,
        total_samples));
    }

  }


DataReaderListenerImpl::~DataReaderListenerImpl (void)
  {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) DataReaderListenerImpl::~DataReaderListenerImpl\n")));
  }


void DataReaderListenerImpl::on_requested_deadline_missed (
    ::DDS::DataReader_ptr reader,
    const ::DDS::RequestedDeadlineMissedStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader);
    ACE_UNUSED_ARG(status);
  }


void DataReaderListenerImpl::on_requested_incompatible_qos (
    ::DDS::DataReader_ptr reader,
    const ::DDS::RequestedIncompatibleQosStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader);
    ACE_UNUSED_ARG(status);
  }


void DataReaderListenerImpl::on_liveliness_changed (
    ::DDS::DataReader_ptr reader,
    const ::DDS::LivelinessChangedStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader);
    ACE_UNUSED_ARG(status);
    ACE_DEBUG((LM_INFO,"(%P|%t) DataReaderListenerImpl::on_liveliness_changed called\n"));
  }


void DataReaderListenerImpl::on_subscription_match (
    ::DDS::DataReader_ptr reader,
    const ::DDS::SubscriptionMatchStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader) ;
    ACE_UNUSED_ARG(status) ;
    ACE_DEBUG((LM_INFO,"(%P|%t) DataReaderListenerImpl::on_subscription_match called\n"));
  }


void DataReaderListenerImpl::on_sample_rejected(
    ::DDS::DataReader_ptr reader,
    const DDS::SampleRejectedStatus& status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader) ;
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) DataReaderListenerImpl::on_sample_rejected\n")));

    GuardType guard(this->lock_);
    samples_rejected_count_ += status.total_count_change;
    total_samples_count_ += status.total_count_change;
    stats_.samples_received(status.total_count_change);
  }


void DataReaderListenerImpl::on_data_available(
    ::DDS::DataReader_ptr reader
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader) ;
    //ACE_DEBUG((LM_DEBUG,
    //  ACE_TEXT("(%P|%t) DataReaderListenerImpl::on_data_available\n")));

    GuardType guard(this->lock_);
    samples_received_count_++;
    total_samples_count_++;

    if (0 == total_samples_count_ % read_interval_)
    {
      // perform the read
      int samples_read = read_samples(reader);
      ACE_UNUSED_ARG(samples_read);
      //ACE_DEBUG((LM_DEBUG,
      //  ACE_TEXT("(%P|%t) DataReaderListenerImpl read %d samples\n"),
      //  samples_read));
    }
  }


void DataReaderListenerImpl::on_sample_lost(
    ::DDS::DataReader_ptr reader,
    const DDS::SampleLostStatus& status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader) ;
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) DataReaderListenerImpl::on_sample_lost\n")));

    GuardType guard(this->lock_);
    samples_lost_count_ += status.total_count_change;
    total_samples_count_ += status.total_count_change;
    stats_.samples_received(status.total_count_change);
  }


int DataReaderListenerImpl::read_samples (::DDS::DataReader_ptr reader)
{
  int num_read = 0;

#ifndef WITH_ZERO_COPY_ONLY
  if (use_zero_copy_reads_) {
#endif
    switch ( num_floats_per_sample_ )
    {

    case 128:
        {
        num_read = read < ::Mine::Pt128ZCSeq,
                          ::TAO::DCPS::SampleInfoZCSeq,
                        ::Mine::Pt128DataReader,
                        ::Mine::Pt128DataReader_ptr,
                        ::Mine::Pt128DataReader_var,
                        ::Mine::Pt128DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    case 512:
        {
        num_read = read < ::Mine::Pt512ZCSeq,
                          ::TAO::DCPS::SampleInfoZCSeq,
                        ::Mine::Pt512DataReader,
                        ::Mine::Pt512DataReader_ptr,
                        ::Mine::Pt512DataReader_var,
                        ::Mine::Pt512DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    case 2048:
        {
        num_read = read < ::Mine::Pt2048ZCSeq,
                          ::TAO::DCPS::SampleInfoZCSeq,
                        ::Mine::Pt2048DataReader,
                        ::Mine::Pt2048DataReader_ptr,
                        ::Mine::Pt2048DataReader_var,
                        ::Mine::Pt2048DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    case 8192:
        {
        num_read = read < ::Mine::Pt8192ZCSeq,
                          ::TAO::DCPS::SampleInfoZCSeq,
                        ::Mine::Pt8192DataReader,
                        ::Mine::Pt8192DataReader_ptr,
                        ::Mine::Pt8192DataReader_var,
                        ::Mine::Pt8192DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    default:
        ACE_ERROR((LM_ERROR,"ERROR: bad data size %d\n", data_size_));
        return 0;
        break;
    };

#ifndef WITH_ZERO_COPY_ONLY
  }
  else {


    switch ( num_floats_per_sample_ )
    {

    case 128:
        {
        num_read = read < ::Mine::Pt128Seq,
                          ::DDS::SampleInfoSeq,
                        ::Mine::Pt128DataReader,
                        ::Mine::Pt128DataReader_ptr,
                        ::Mine::Pt128DataReader_var,
                        ::Mine::Pt128DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    case 512:
        {
        num_read = read < ::Mine::Pt512Seq,
                          ::DDS::SampleInfoSeq,
                        ::Mine::Pt512DataReader,
                        ::Mine::Pt512DataReader_ptr,
                        ::Mine::Pt512DataReader_var,
                        ::Mine::Pt512DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    case 2048:
        {
        num_read = read < ::Mine::Pt2048Seq,
                          ::DDS::SampleInfoSeq,
                        ::Mine::Pt2048DataReader,
                        ::Mine::Pt2048DataReader_ptr,
                        ::Mine::Pt2048DataReader_var,
                        ::Mine::Pt2048DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    case 8192:
        {
        num_read = read < ::Mine::Pt8192Seq,
                          ::DDS::SampleInfoSeq,
                        ::Mine::Pt8192DataReader,
                        ::Mine::Pt8192DataReader_ptr,
                        ::Mine::Pt8192DataReader_var,
                        ::Mine::Pt8192DataReaderImpl>
                            (reader, use_zero_copy_reads_);
        }
        break;

    default:
        ACE_ERROR((LM_ERROR,"ERROR: bad data size %d\n", data_size_));
        return 0;
        break;
  };

  }
#endif

  stats_.samples_received(num_read);

  return num_read;
}


bool DataReaderListenerImpl::is_finished ()
{
ACE_DEBUG((LM_DEBUG,
          ACE_TEXT("(%P|%t) DataReaderListenerImpl %d of %d samples received\n"),
          stats_.packet_count_,
          stats_.expected_packets_));

  bool is_finished = stats_.all_packets_received();
  if (is_finished)
    {
      stats_.dump();
    }

  return is_finished;
}
