// -*- C++ -*-
//
// $Id$

#include "Reader.h"
#include "TestException.h"
#include "dds/DCPS/transport/framework/ReceivedDataSample.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Serializer.h"
#include "../TypeNoKeyBounded/Pt128TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt512TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt2048TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt8192TypeSupportC.h"
#include "../TypeNoKeyBounded/Pt128TypeSupportImpl.h"
#include "../TypeNoKeyBounded/Pt512TypeSupportImpl.h"
#include "../TypeNoKeyBounded/Pt2048TypeSupportImpl.h"
#include "../TypeNoKeyBounded/Pt8192TypeSupportImpl.h"




template<class Tseq, class R, class R_var, class R_ptr, class Rimpl>
::DDS::ReturnCode_t read (TestStats* stats,
                          ::DDS::Subscriber_ptr subscriber,
                          ::DDS::DataReader_ptr reader)
{
  R_var pt_dr 
    = R::_narrow(reader ACE_ENV_ARG_PARAMETER);
  if (CORBA::is_nil (pt_dr.in ()))
    {
      ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) _narrow failed.\n")));
      throw TestException() ;
    }

  Rimpl* dr_servant =
      ::TAO::DCPS::reference_to_servant< Rimpl, R_ptr>
              (pt_dr.in() ACE_ENV_SINGLE_ARG_PARAMETER);

  const ::CORBA::Long max_read_samples = 100;
  Tseq samples(max_read_samples);
  ::DDS::SampleInfoSeq infos(max_read_samples);

  ACE_Array<bool> pub_finished(stats->num_publishers_);
  for (unsigned j =0; j < stats->num_publishers_; j++)
    {
      pub_finished[j] = false;
    }


  // wait for data to become available 
  // so we know to start reading
  if (!Reader::wait_for_data(subscriber, 10))
    ACE_ERROR_RETURN((LM_ERROR, 
      "ERROR: waited too long for the first sample\n"),
      -2);

  int num_reads = 0;
  int zero_reads = 0;
  DDS::ReturnCode_t status;
  ::DDS::SampleRejectedStatus rejected = 
         dr_servant->get_sample_rejected_status ();
  ::DDS::SampleLostStatus lost =
         dr_servant->get_sample_lost_status ();

  bool end_messages = false;

  while ( !stats->all_packets_received () && ! end_messages )
    {

      // very slow status = dr_servant->read_next_sample(sample, si) ;

      status = dr_servant->read (
        samples,
        infos,
        max_read_samples,
        ::DDS::NOT_READ_SAMPLE_STATE, 
        ::DDS::ANY_VIEW_STATE, 
        ::DDS::ANY_INSTANCE_STATE
        ACE_ENV_ARG_PARAMETER);
      ACE_CHECK_RETURN (::DDS::RETCODE_ERROR);//TBD do the right thing here.


      if (status == ::DDS::RETCODE_OK)
        {
          num_reads++;

          if (samples[samples.length () - 1].sequence_num == -1)
            {
              int valid_msg_count = 0;
              for (unsigned i = 0; i < samples.length (); i ++)
                {
                  if (samples[i].sequence_num != -1)
                  {
                    valid_msg_count ++;
                  }
                }

              stats->samples_received(valid_msg_count);

              bool all_finished = true;
              for (unsigned j = 0; j < stats->num_publishers_; j++)
                {
                  all_finished = all_finished && pub_finished[j];
                }

              if (all_finished)
                {
                  stats->finished();
                  end_messages = true;
                }
            }
          else
            {
              stats->samples_received(samples.length ());
            }
        }
      else if (status != ::DDS::RETCODE_NO_DATA)
        {
          num_reads++;
          zero_reads++;

          ACE_OS::printf (" read  data: Error: %d\n", status) ;
        }
      else
        {
          zero_reads++;


          //ACE_DEBUG((LM_DEBUG,"got RETCODE_NO_DATA\n"));
          rejected = dr_servant->get_sample_rejected_status ();
          if (rejected.total_count_change > 0)
            {
              //ACE_DEBUG((LM_DEBUG,"rejected %d samples\n", rejected.total_count_change));
              stats->samples_received(rejected.total_count_change);
            }
          lost = dr_servant->get_sample_lost_status ();
          if (lost.total_count_change > 0)
            {
              //ACE_DEBUG((LM_DEBUG,"lost %d samples\n", lost.total_count_change));
              stats->samples_received(lost.total_count_change);
            }

          // give the transport thread a chance to receive more and get the lock
          ACE_OS::thr_yield (); 

          //if (zero_reads > 500000)
          //  {
          //    ACE_ERROR((LM_ERROR, "Too many zero_reads!!!!\n"));
          //    ACE_DEBUG((LM_DEBUG,"samples = %d reads = %d zero_reads =%d samples per read = %d\n",
          //      stats->packet_count_, num_reads, zero_reads, 
          //      stats->packet_count_ /num_reads));

          //    ACE_ERROR((LM_ERROR,"ERROR: samples rejected = %d lost =%d read =%d total = %d !!!!!!\n",
          //      rejected.total_count, 
          //      lost.total_count, samples_recvd,
          //      rejected.total_count + lost.total_count + samples_recvd));
          //    ACE_OS::sleep(2);
          //    ACE_OS::exit (7);
          //  }




        }
    }

  ACE_DEBUG((LM_DEBUG,"samples = %d reads = %d zero_reads =%d samples per read = %d\n",
    stats->expected_packets_, num_reads, zero_reads, 
    stats->expected_packets_ /num_reads));

  return ::DDS::RETCODE_OK;
}









Reader::Reader(::DDS::Subscriber_ptr subscriber,
               ::DDS::DataReader_ptr reader, 
               int num_publishers,
               int num_samples, 
               int data_size)
: subscriber_ (::DDS::Subscriber::_duplicate(subscriber)),
  reader_ (::DDS::DataReader::_duplicate(reader)),
  num_publishers_(num_publishers),
  num_samples_ (num_samples),
  data_size_ (data_size),
  num_floats_per_sample_ (1 << data_size),
  finished_sending_ (false)
{
}

void 
Reader::start ()
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) Reader::start \n")));
  if (activate (THR_NEW_LWP | THR_JOINABLE, 1) == -1) 
  {
    ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) Reader::start, ")
                ACE_TEXT ("%p."), 
                "activate")); 
    throw TestException ();
  }
}

void 
Reader::end () 
{
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Reader::end \n")));
  wait ();
}


int 
Reader::svc ()
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT(" Reader::svc begins \n")));

  int svc_status = 0;

  stats_.init(num_publishers_, num_samples_, data_size_, num_floats_per_sample_*4 + 8);
  // The above last parameter needs to be adjusted for the header

  DDS::ReturnCode_t status;

  ACE_TRY_NEW_ENV
  {

    switch ( num_floats_per_sample_ )
    {

    case 128:
      {
        status = read < ::Mine::Pt128Seq,
                       ::Mine::Pt128DataReader,
                       ::Mine::Pt128DataReader_var,
                       ::Mine::Pt128DataReader_ptr,
                       ::Mine::Pt128DataReaderImpl>
                         (
                          &stats_,
                          subscriber_.in (),
                          reader_.in ());
        if (::DDS::RETCODE_OK != status)
          return status;
      }
      break;

    case 512:
      {
        status = read < ::Mine::Pt512Seq,
                       ::Mine::Pt512DataReader,
                       ::Mine::Pt512DataReader_var,
                       ::Mine::Pt512DataReader_ptr,
                       ::Mine::Pt512DataReaderImpl>
                         (
                          &stats_,
                          subscriber_.in (),
                          reader_.in ());
        if (::DDS::RETCODE_OK != status)
          return status;
      }
      break;

    case 2048:
      {
        status = read < ::Mine::Pt2048Seq,
                       ::Mine::Pt2048DataReader,
                       ::Mine::Pt2048DataReader_var,
                       ::Mine::Pt2048DataReader_ptr,
                       ::Mine::Pt2048DataReaderImpl>
                         (
                          &stats_,
                          subscriber_.in (),
                          reader_.in ());
        if (::DDS::RETCODE_OK != status)
          return status;
      }
      break;

    case 8192:
      {
        status = read < ::Mine::Pt8192Seq,
                       ::Mine::Pt8192DataReader,
                       ::Mine::Pt8192DataReader_var,
                       ::Mine::Pt8192DataReader_ptr,
                       ::Mine::Pt8192DataReaderImpl>
                         (
                          &stats_,
                          subscriber_.in (),
                          reader_.in ());
        if (::DDS::RETCODE_OK != status)
          return status;
      }
      break;

    default:
      ACE_DEBUG((LM_ERROR,"ERROR: bad data size %d\n", data_size_));
      return 1;
      break;
    };

  }
  ACE_CATCHANY
  {
    ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
      "Exception caught in svc:");
    return 1;
  }
  ACE_ENDTRY;

  finished_sending_ = true;

  ACE_DEBUG((LM_DEBUG,
              ACE_TEXT(" Reader::svc finished.\n")));

  stats_.dump();

  return svc_status;
}

int
Reader::wait_for_data (::DDS::Subscriber_ptr sub,
                       int timeout_sec)
{
  const int factor = 1000;
  ACE_Time_Value small(0,1000000/factor);
  int timeout_loops = timeout_sec * factor;

  ::DDS::DataReaderSeq_var discard = new ::DDS::DataReaderSeq(10);
  while (timeout_loops-- > 0)
    {
      sub->get_datareaders (
                    discard.out (),
                    ::DDS::NOT_READ_SAMPLE_STATE,
                    ::DDS::ANY_VIEW_STATE,
                    ::DDS::ANY_INSTANCE_STATE );
      if (discard->length () > 0)
        return 1;

      ACE_OS::sleep (small);
    }
  return 0;
} 


bool
Reader::is_finished () const
{
  return finished_sending_;
}

