// -*- C++ -*-
//
// $Id$
#include "Writer.h"
#include "../common/TestException.h"
#include "../common/TestSupport.h"
#include "tests/DCPS/FooType4/FooTypeSupportC.h"
#include "ace/OS_NS_unistd.h"

const int default_key = 101010;


Writer::Writer(::DDS::DataWriter_ptr writer, 
               int num_thread_to_write,
               int num_writes_per_thread)
: writer_ (::DDS::DataWriter::_duplicate (writer)),
  num_thread_to_write_ (num_thread_to_write),
  num_writes_per_thread_ (num_writes_per_thread),
  finished_sending_ (false)
{
}

int 
Writer::run_test (int pass)
{
  ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t) Writer::run_test begins.\n")));

  ACE_TRY_NEW_ENV
  {
    finished_sending_ = false;

    ::Xyz::Foo foo;
    //foo.key set below.
    foo.x = -1;
    foo.y = -1;

    foo.key = default_key;
    
    ::Mine::FooDataWriter_var foo_dw 
      = ::Mine::FooDataWriter::_narrow(writer_.in () ACE_ENV_ARG_PARAMETER);
    TEST_CHECK (! CORBA::is_nil (foo_dw.in ()));

    ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("%T (%P|%t) Writer::run_test starting to write pass %d\n"),
              pass));

    ::DDS::InstanceHandle_t handle 
        = foo_dw->_cxx_register (foo ACE_ENV_ARG_PARAMETER);

    for (int i = 0; i< num_writes_per_thread_; i ++)
    {

      foo.x = (float)i;
      foo.y = (float)(-pass) ;

      foo_dw->write(foo, 
                    handle 
                    ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;
    }

    ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("%T (%P|%t) Writer::run_test done writing.\n")));

  }
  ACE_CATCHANY
  {
    ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
      "Exception caught in run_test:");
  }
  ACE_ENDTRY;

  finished_sending_ = true;

  ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t) Writer::run_test finished.\n")));
  return 0;
}


bool
Writer::is_finished () const
{
  return finished_sending_;
}

