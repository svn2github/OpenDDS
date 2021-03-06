// -*- C++ -*-
//
// $Id$

#include "Writer.h"
#include "dds/DCPS/Service_Participant.h"

#include <dds/DCPS/Qos_Helper.h>

#include <ace/OS_NS_unistd.h>
#include <ace/streams.h>

using namespace Messenger;

static int const num_messages = 10;
static ACE_Time_Value write_interval(0, 500000);
extern ACE_Time_Value SLEPP_DURATION;

Writer::Writer(::DDS::DataWriter_ptr writer, 
               CORBA::Long key,
               ACE_Time_Value sleep_duration)
: writer_ (::DDS::DataWriter::_duplicate (writer)),
  condition_ (this->lock_),
  associated_ (false),
  dwl_servant_ (0),
  instance_handle_ (::DDS::HANDLE_NIL),
  key_ (key),
  sleep_duration_ (sleep_duration)
{
  ::DDS::DataWriterListener_var dwl = writer->get_listener();
  dwl_servant_ =
    dynamic_cast<DataWriterListenerImpl*> (dwl.in());
}

void
Writer::start ()
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t)%T Writer::start \n")));
  // Launch threads.
  if (activate (THR_NEW_LWP | THR_JOINABLE, 1) == -1)
  {
    cerr << "Writer::start(): activate failed" << endl;
    exit(1);
  }
}

void
Writer::end ()
{
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Writer::end \n")));
  wait ();
}


int
Writer::svc ()
{
  ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t) Writer::svc begins.\n")));
  
  try
  {
    while (1)
    {
      ::DDS::InstanceHandleSeq handles;
      this->writer_->get_matched_subscriptions (handles);
      if (handles.length () == 2)
        break;
      else
        ACE_OS::sleep(ACE_Time_Value(0,250000));
    }

    {
      GuardType guard (this->lock_);
      this->associated_ = true;
      this->condition_.signal ();
    }

    Messenger::MessageDataWriter_var message_dw =
      Messenger::MessageDataWriter::_narrow(writer_.in());
    if (CORBA::is_nil (message_dw.in ())) {
      cerr << "Data Writer could not be narrowed"<< endl;
      exit(1);
    }

    Messenger::Message message;
    message.subject_id = this->key_;
    this->instance_handle_ = message_dw->register_instance(message);

    message.from       = CORBA::string_dup("Comic Book Guy");
    message.subject    = CORBA::string_dup("Review");
    message.text       = CORBA::string_dup("Worst. Movie. Ever.");
    message.count      = 0;
    
    ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t)%T Writer::svc sleep for %d seconds.\n"), 
              this->sleep_duration_.sec()));

    ACE_OS::sleep (this->sleep_duration_);

    for (int i = 0; i< num_messages; i ++)
    {
      ++message.count;

      ::DDS::ReturnCode_t const ret = message_dw->write (message, this->instance_handle_);

      if (ret != ::DDS::RETCODE_OK)
      {
        ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t)ERROR  Writer::svc, ")
                    ACE_TEXT ("%dth write() returned %d.\n"),
                    i,
                    -1));
      }

      // Sleep for half a second between writes to allow some deadline
      // periods to expire.  Missed deadline should not occur since
      // the time between writes should be less than the offered
      // deadline period.
      ACE_OS::sleep (write_interval);
    }
  }
  catch (CORBA::Exception& e)
  {
    cerr << "Exception caught in svc:" << endl
         << e << endl;
  }

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t)%T Writer::svc finished.\n")));

  return 0;
}


::DDS::InstanceHandle_t 
Writer::get_instance_handle()
{
  return this->instance_handle_;
}


bool Writer::wait_for_start ()
{
  GuardType guard (this->lock_);

  if (! associated_)
  {
    ACE_Time_Value abs = ACE_OS::gettimeofday () + ACE_Time_Value (10);
    if (this->condition_.wait (&abs) == -1)
    {
      return false;
    }
  }
  return true;
}
