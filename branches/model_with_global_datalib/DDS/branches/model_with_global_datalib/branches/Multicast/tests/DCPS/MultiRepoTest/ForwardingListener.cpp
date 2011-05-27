// -*- C++ -*-
//
// $Id$
#include "ForwardingListener.h"
#include "TestException.h"
#include "tests/DCPS/FooType5/FooDefTypeSupportC.h"
#include "tests/DCPS/FooType5/FooDefTypeSupportImpl.h"
#include "dds/DCPS/Service_Participant.h"

// Only for Microsoft VC6
#if defined (_MSC_VER) && (_MSC_VER >= 1200) && (_MSC_VER < 1300)

// Added unused arguments with default value to work around with vc6
// bug on template function instantiation.
template <class DT, class DT_seq, class DR, class DR_ptr, class DR_var, class DR_impl>
int read (::DDS::DataReader_ptr reader, ::DDS::DataWriter_ptr writer,
          DT* dt = 0, DR* dr = 0, DR_ptr dr_ptr = 0, DR_var* dr_var = 0, DR_impl* dr_impl = 0)
{
  ACE_UNUSED_ARG (dt);
  ACE_UNUSED_ARG (dr);
  ACE_UNUSED_ARG (dr_ptr);
  ACE_UNUSED_ARG (dr_var);
  ACE_UNUSED_ARG (dr_impl);

#else

template <class DT, class DT_seq, class DR, class DR_ptr, class DR_var, class DR_impl>
int read (::DDS::DataReader_ptr reader, DT& foo, bool& valid_data)
{

#endif

  try
  {
    DR_var foo_dr
      = DR::_narrow(reader);
    if (CORBA::is_nil (foo_dr.in ()))
    {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::read - _narrow failed.\n")));
      throw BadReaderException() ;
    }

    DR_impl* dr_servant =
      dynamic_cast<DR_impl*> (foo_dr.in ());

    ::DDS::SampleInfo si ;

    DDS::ReturnCode_t status  ;

    status = dr_servant->read_next_sample(foo, si) ;

    if (status == ::DDS::RETCODE_OK)
    {
      valid_data = si.valid_data;
      if (si.valid_data == 1)
      {
        ACE_DEBUG((LM_DEBUG,
          ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::read %X foo.x = %f foo.y = %f, foo.data_source = %d\n"),
          reader, foo.x, foo.y, foo.data_source));
      }
      else if (si.instance_state == DDS::NOT_ALIVE_DISPOSED_INSTANCE_STATE)
      {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t)instance is disposed\n")));
      }
      else if (si.instance_state == DDS::NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
      {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t)instance is unregistered\n")));
      }
      else
      {
        ACE_ERROR ((LM_ERROR, "(%P|%t)ForwardingListenerImpl::read:"
          " received unknown instance state %d\n", si.instance_state));
      }
    }
    else if (status == ::DDS::RETCODE_NO_DATA)
    {
      ACE_ERROR_RETURN ((LM_ERROR,
        ACE_TEXT("%T (%P|%t) ERROR: ForwardingListenerImpl::reader received ::DDS::RETCODE_NO_DATA!\n")),
        -1);
    }
    else
    {
      ACE_ERROR_RETURN ((LM_ERROR,
        ACE_TEXT("%T (%P|%t) ERROR: ForwardingListenerImpl::read status==%d\n"), status),
        -1);
    }
  }
  catch (const CORBA::Exception& ex)
  {
    ex._tao_print_exception ("%T (%P|%t) ForwardingListenerImpl::read - ");
    return -1;
  }

  return 0;
}


// Implementation skeleton constructor
ForwardingListenerImpl::ForwardingListenerImpl(
  OpenDDS::DCPS::Service_Participant::RepoKey repo
) : samples_( 0),
    condition_( this->lock_),
    complete_ (false),
    repo_( repo)
{
  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::ForwardingListenerImpl Repo[ %d]\n"),
    this->repo_
  ));
}

// Implementation skeleton destructor
ForwardingListenerImpl::~ForwardingListenerImpl (void)
  {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::~ForwardingListenerImpl Repo[ %d] ")
      ACE_TEXT("after %d samples\n"),
      this->repo_,
      this->samples_
    ));
  }

/// Writer to forward data on.
void ForwardingListenerImpl::dataWriter(
  ::DDS::DataWriter_ptr writer
)
{
  this->dataWriter_ = ::DDS::DataWriter::_duplicate( writer);
}

void
ForwardingListenerImpl::waitForCompletion()
{
  ACE_GUARD (ACE_SYNCH_MUTEX, g, this->lock_);
  while (!this->complete_)
    {
      this->condition_.wait();
    }
}

void ForwardingListenerImpl::on_requested_deadline_missed (
    ::DDS::DataReader_ptr reader,
    const ::DDS::RequestedDeadlineMissedStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader);
    ACE_UNUSED_ARG(status);

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_requested_deadline_missed Repo[ %d]\n"),
      this->repo_
    ));
  }

void ForwardingListenerImpl::on_requested_incompatible_qos (
    ::DDS::DataReader_ptr reader,
    const ::DDS::RequestedIncompatibleQosStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader);
    ACE_UNUSED_ARG(status);

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_requested_incompatible_qos Repo[ %d]\n"),
      this->repo_
    ));
  }

void ForwardingListenerImpl::on_liveliness_changed (
    ::DDS::DataReader_ptr reader,
    const ::DDS::LivelinessChangedStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader);
    ACE_UNUSED_ARG(status);

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_liveliness_changed Repo[ %d]\n"),
      this->repo_
    ));
  }

void ForwardingListenerImpl::on_subscription_matched (
    ::DDS::DataReader_ptr reader,
    const ::DDS::SubscriptionMatchedStatus & status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader) ;
    ACE_UNUSED_ARG(status) ;

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_subscription_matched Repo[ %d] \n"),
      this->repo_
    ));
  }

  void ForwardingListenerImpl::on_sample_rejected(
    ::DDS::DataReader_ptr reader,
    const DDS::SampleRejectedStatus& status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader) ;
    ACE_UNUSED_ARG(status) ;

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_sample_rejected Repo[ %d] \n"),
      this->repo_
    ));
  }

  void ForwardingListenerImpl::on_data_available(
    ::DDS::DataReader_ptr reader
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    //ACE_DEBUG((LM_DEBUG,
    //  ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_data_available %d\n"), num_reads.value ()));

    //num_reads ++;

    ::Xyz::FooNoKey foo;
    bool valid_data = false;
    int ret = read <Xyz::FooNoKey,
        ::Xyz::FooNoKeySeq,
        ::Xyz::FooNoKeyDataReader,
        ::Xyz::FooNoKeyDataReader_ptr,
        ::Xyz::FooNoKeyDataReader_var,
        ::Xyz::FooNoKeyDataReaderImpl> (reader, foo, valid_data);

    if (ret != 0)
    {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_data_available Repo[ %d] read failed.\n"),
        this->repo_
      ));

    } else if( CORBA::is_nil( this->dataWriter_.in())) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("%T (%P|%t) ForwardingListenerImpl Repo[ %d] - bit bucket reached. \n"),
        this->repo_
      ));
      // The bit bucket is done processing when the answer is received.
      if(valid_data && foo.data_source == 42) {
        ACE_GUARD (ACE_SYNCH_MUTEX, g, this->lock_);
        this->complete_ = true;
        this->condition_.signal();
      }

    } else if(valid_data && foo.data_source == 30) {
      // Signal that we are done once we receive a disconnect message.
      // We use the data_source member as a command value.
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("%T (%P|%t) ForwardingListenerImpl Repo[ %d] - termination command received. \n"),
        this->repo_
      ));
      ACE_GUARD (ACE_SYNCH_MUTEX, g, this->lock_);
      this->complete_ = true;
      this->condition_.signal();

    } else {
      // This is narrowed to forward each sample to avoid hoisting the
      // type up into the header and creating more dependencies.  If this
      // were a performance application, that might be a better option.
      ::Xyz::FooNoKeyDataWriter_var fooWriter
        = ::Xyz::FooNoKeyDataWriter::_narrow( this->dataWriter_.in());
      if( CORBA::is_nil( fooWriter.in())) {
        ACE_ERROR((LM_ERROR,
          ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_data_available Repo[ %d] ")
          ACE_TEXT("failed to narrow writer to forward a sample with.\n"),
          this->repo_
        ));

      } else if (valid_data)
      {
        // Modify the data as it passes, just to prove it has been here.
        foo.x += 1.0;
        foo.y += 2.0;

        // Go ahead and forward the data.
        if( ::DDS::RETCODE_OK != fooWriter->write( foo, ::DDS::HANDLE_NIL)) {
          ACE_ERROR((LM_ERROR,
            ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_data_available Repo[ %d] ")
            ACE_TEXT("failed to forward a sample.\n"),
            this->repo_
          ));
        }
      }
    }
  }

  void ForwardingListenerImpl::on_sample_lost(
    ::DDS::DataReader_ptr reader,
    const DDS::SampleLostStatus& status
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
  {
    ACE_UNUSED_ARG(reader) ;
    ACE_UNUSED_ARG(status) ;

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) ForwardingListenerImpl::on_sample_lost Repo[ %d] \n"),
      this->repo_
    ));
  }
