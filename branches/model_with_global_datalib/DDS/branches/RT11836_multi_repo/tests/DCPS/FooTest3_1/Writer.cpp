// -*- C++ -*-
//
// $Id$
// -*- C++ -*-
//
// $Id$
#include "Writer.h"
#include "PubDriver.h"
#include "TestException.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/DataWriterImpl.h"
#include "tests/DCPS/FooType3/FooTypeSupportC.h"
#include "tests/DCPS/common/TestSupport.h"

const int default_key = 101010;


Writer::Writer(PubDriver*            pubdriver,
               ::DDS::DataWriter_ptr writer,
               int num_thread_to_write,
               int num_writes_per_thread,
               int multiple_instances,
               int writer_id,
               int have_key,
               int write_delay_msec,
               int check_data_dropped)
: writer_ (::DDS::DataWriter::_duplicate (writer)),
  writer_servant_ (0),
  num_thread_to_write_ (num_thread_to_write),
  num_writes_per_thread_ (num_writes_per_thread),
  multiple_instances_ (multiple_instances),
  writer_id_ (writer_id),
  has_key_ (have_key),
  write_delay_msec_ (write_delay_msec),
  check_data_dropped_ (check_data_dropped),
  pubdriver_ (pubdriver)
{
  writer_servant_
    = OpenDDS::DCPS::reference_to_servant<OpenDDS::DCPS::DataWriterImpl> (writer_.in ());
}

void
Writer::start ()
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) Writer::start \n")));
  if (activate (THR_NEW_LWP | THR_JOINABLE, num_thread_to_write_) == -1)
  {
    ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) Writer::start, ")
                ACE_TEXT ("%p."),
                "activate"));
    throw TestException ();
  }
}

void
Writer::end ()
{
  wait ();


  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) Writer::end \n")));
}


int
Writer::svc ()
{
  ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t) Writer::svc \n")));

  try
  {
    ::Xyz::Foo foo;
    foo.sample_sequence = -1;
    foo.handle_value = -1;
    foo.writer_id = writer_id_;

    if (multiple_instances_ == 1)
    {
      // Use the thread id as the instance key.
      foo.a_long_value = (CORBA::Long) (ACE_OS::thr_self ());
    }
    else
    {
      foo.a_long_value = default_key;
    }

    ::Xyz::FooDataWriter_var foo_dw
      = ::Xyz::FooDataWriter::_narrow(writer_.in ());

    TEST_CHECK (! CORBA::is_nil (foo_dw.in ()));

    for (int i = 0; i< num_writes_per_thread_; i ++)
    {
      ::DDS::InstanceHandle_t handle
        = foo_dw->_cxx_register (foo);

      foo.handle_value = handle;

      // The sequence number will be increased after the insert.
      TEST_CHECK (data_map_.insert (handle, foo) == 0);

      ::DDS::ReturnCode_t ret = ::DDS::RETCODE_OK;

      if (has_key_ == 1)
      {
        ::Xyz::Foo key_holder;
        ret = foo_dw->get_key_value(key_holder, handle);

        TEST_CHECK(ret == ::DDS::RETCODE_OK);
        // check for equality
        TEST_CHECK (foo.a_long_value == key_holder.a_long_value); // It is the instance key.
      }

      ret = foo_dw->write(foo,
                          handle);
      TEST_CHECK (ret == ::DDS::RETCODE_OK);

      if (write_delay_msec_ > 0)
      {
        ACE_Time_Value delay (write_delay_msec_/1000, write_delay_msec_%1000 * 1000);
        ACE_OS::sleep (delay);
      }
    }
  }
  catch (const CORBA::Exception& ex)
  {
    ex._tao_print_exception ("Exception caught in svc:");
  }

  if (check_data_dropped_ == 1 && writer_servant_->data_dropped_count_ > 0)
  {
    while (writer_servant_->data_delivered_count_ + writer_servant_->data_dropped_count_
    < num_writes_per_thread_ * num_thread_to_write_)
    {
      ACE_OS::sleep (1);
    }

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Writer::svc data_delivered_count=%d data_dropped_count=%d\n"),
      writer_servant_->data_delivered_count_, writer_servant_->data_dropped_count_));

    if (writer_servant_->data_dropped_count_ > 0)
    {
      pubdriver_->shutdown_ = 1;
    }
  }
  return 0;
}

long
Writer::writer_id () const
{
  return writer_id_;
}


InstanceDataMap&
Writer::data_map ()
{
  return data_map_;
}

