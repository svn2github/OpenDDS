// -*- C++ -*-
//
// $Id$
#include "Writer.h"
#include "common.h"
#include "../common/TestException.h"
#include "../common/TestSupport.h"

#include "ace/Atomic_Op_T.h"

#include "dds/DCPS/Service_Participant.h"
#include "model/Sync.h"

#include "../ManyTopicTypes/Foo1DefTypeSupportC.h"
#include "../ManyTopicTypes/Foo4DefTypeSupportC.h"

#include "ace/OS_NS_unistd.h"

ACE_Atomic_Op<ACE_SYNCH_MUTEX, CORBA::Long> key(0);

Writer::Writer(::DDS::DataWriter_ptr writer,
               int num_thread_to_write,
               int num_writes_per_thread)
  : writer_(::DDS::DataWriter::_duplicate(writer)),
    num_thread_to_write_(num_thread_to_write),
    num_writes_per_thread_(num_writes_per_thread),
    finished_sending_(false)
{
  ::DDS::DataWriterQos dw_qos;
  writer_->get_qos(dw_qos);
  max_wait_ = dw_qos.liveliness.lease_duration.sec / 2;
}

void
Writer::start()
{
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Writer::start \n")));
  if (activate(THR_NEW_LWP | THR_JOINABLE, num_thread_to_write_) == -1)
  {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) Writer::start, %p.\n"),
               ACE_TEXT("activate")));
    throw TestException();
  }
}

void
Writer::end()
{
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Writer::end \n")));
  wait();
}


int
Writer::svc()
{
  try
  {
    finished_sending_ = false;

    ::DDS::Topic_var topic = writer_->get_topic();

    ACE_DEBUG((LM_DEBUG,"(%P|%t) %C: Writer::svc begins.\n",
               topic->get_name()));

    OpenDDS::Model::WriterSync::wait_match(writer_);

    if (!ACE_OS::strcmp(topic->get_name(), MY_TOPIC1) ||
        !ACE_OS::strcmp(topic->get_name(), MY_TOPIC3) ||
        !ACE_OS::strcmp(topic->get_name(), MY_TOPIC4) ||
        !ACE_OS::strcmp(topic->get_name(), MY_TOPIC5) ||
        !ACE_OS::strcmp(topic->get_name(), MY_TOPIC6) ||
        !ACE_OS::strcmp(topic->get_name(), MY_TOPIC7))
    {
      ::T1::Foo1 foo;
      //foo.key set below.
      foo.x = -1;
      foo.y = -1;

      foo.key = ++key;

      ::T1::Foo1DataWriter_var foo_dw
          = ::T1::Foo1DataWriter::_narrow(writer_.in());
      TEST_CHECK(! CORBA::is_nil(foo_dw.in()));

      rsleep1();

      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) %T Writer::svc starting to write.\n")));

      ::DDS::InstanceHandle_t handle
          = foo_dw->register_instance(foo);

      for (int i = 0; i< num_writes_per_thread_; i ++)
      {
        rsleep();

        foo.x = (float)i;
        foo.c = 'A' + (i % 26);

        foo_dw->write(foo,
                      handle);
      }
    }
    else if (!ACE_OS::strcmp(topic->get_name(), MY_TOPIC2))
    {
      ::T4::Foo4 foo;

      foo.key = ++key;

      ::T4::Foo4DataWriter_var foo_dw
          = ::T4::Foo4DataWriter::_narrow(writer_.in());
      TEST_CHECK(! CORBA::is_nil(foo_dw.in()));

      rsleep1();

      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) %T Writer::svc starting to write.\n")));

      ::DDS::InstanceHandle_t handle
          = foo_dw->register_instance(foo);

      for (int i = 0; i< num_writes_per_thread_; i ++)
      {
        rsleep();

        const int sequence_length = 10;
        foo.values.length (sequence_length);

        for (int j = 0; j < sequence_length; j ++)
        {
          foo.values[j] = (float) (i * i - j);
        }

        foo_dw->write(foo,
                      handle);
      }
    }
  }
  catch (const CORBA::Exception& ex)
  {
    ex._tao_print_exception("Exception caught in svc:");
  }

  finished_sending_ = true;

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Writer::svc finished.\n")));
  return 0;
}


bool
Writer::is_finished() const
{
  return finished_sending_;
}


void Writer::rsleep(const int wait)
{
  int lwait = 1 + (ACE_OS::rand() % wait);
  ACE_OS::sleep(ACE_Time_Value(0, lwait));
}

void Writer::rsleep1()
{
  int wait = 2 + (ACE_OS::rand() % (max_wait_ - 1)); // 2 because we want at
                                                     // least 2 seconds
  ACE_OS::sleep(wait);
}
