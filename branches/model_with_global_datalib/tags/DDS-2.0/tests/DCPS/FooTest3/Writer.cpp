// -*- C++ -*-
//
// $Id$
// -*- C++ -*-
//
// $Id$
#include "Writer.h"
#include "TestException.h"
#include "tests/DCPS/common/TestSupport.h"
#include "tests/DCPS/FooType3/FooDefTypeSupportC.h"

const int default_key = 101010;


Writer::Writer(::DDS::DataWriter_ptr writer,
               int num_thread_to_write,
               int num_writes_per_thread,
               int multiple_instances_,
               int writer_id)
: writer_ (::DDS::DataWriter::_duplicate (writer)),
  num_thread_to_write_ (num_thread_to_write),
  num_writes_per_thread_ (num_writes_per_thread),
  multiple_instances_ (multiple_instances_),
  writer_id_ (writer_id)
{
}

void
Writer::start ()
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("(%P|%t) Writer::start \n")));
  if (activate (THR_NEW_LWP | THR_JOINABLE, num_thread_to_write_) == -1)
  {
    ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) Writer::start, %p.\n"),
                ACE_TEXT("activate")));
    throw TestException ();
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
        = foo_dw->register_instance(foo);

      foo.handle_value = handle;

      // The sequence number will be increased after the insert.
      TEST_CHECK (data_map_.insert (handle, foo) == 0);

      ::Xyz::Foo key_holder;
      ::DDS::ReturnCode_t ret
        = foo_dw->get_key_value(key_holder, handle);

      TEST_CHECK(ret == ::DDS::RETCODE_OK);
      TEST_CHECK(key_holder.sample_sequence == -1);
      TEST_CHECK(key_holder.handle_value == -1);
      // check for equality
      TEST_CHECK (foo.a_long_value == key_holder.a_long_value); // It is the instance key.

      foo_dw->write(foo,
                    handle);
    }
  }
  catch (const CORBA::Exception& ex)
  {
    ex._tao_print_exception ("Exception caught in svc:");
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

