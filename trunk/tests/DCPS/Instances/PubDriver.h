#ifndef PUBDRIVER_H
#define PUBDRIVER_H

#include "TestException.h"
#include "InstanceDataMap.h"
#include "dds/DdsDcpsC.h"
#include "dds/DCPS/Definitions.h"
#include "dds/DdsDcpsPublicationC.h"
#include "dds/DCPS/DataWriterImpl.h"
#include "ace/INET_Addr.h"
#include "ace/Task.h"
#include "ace/String_Base.h"
#include "ace/Atomic_Op_T.h"
#include "tests/DCPS/common/TestSupport.h"
#include "tests/Utils/DDSApp.h"
#include "tests/Utils/Options.h"
#include <string>
#include <vector>
#include <sstream>

const int default_key = 101010;
ACE_Atomic_Op<ACE_SYNCH_MUTEX, CORBA::Long> key(0);

template<typename TypeSupportImpl>
class Writer : public ACE_Task_Base
{
public:
  typedef typename TypeSupportImpl::message_type message_type;
  typedef typename TypeSupportImpl::datawriter_var datawriter_var;
  typedef typename TypeSupportImpl::datawriter_type datawriter_type;

  Writer(datawriter_var writer,
    bool keyed_data = true,
    long num_thread_to_write = 1,
    long num_writes_per_thread = 1,
    bool multiple_instances = false,
    long writer_id = -1,
    long write_delay_msec = 0,
    long check_data_dropped = 0)
  : writer_ (datawriter_type::_duplicate(writer)),
    writer_servant_ (0),
    keyed_data_ (keyed_data),
    num_thread_to_write_ (num_thread_to_write),
    num_writes_per_thread_ (num_writes_per_thread),
    multiple_instances_ (multiple_instances),
    writer_id_ (writer_id),
    write_delay_msec_ (write_delay_msec),
    check_data_dropped_ (check_data_dropped),
    finished_(false)
  {
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Writer::Writer \n")));
    writer_servant_ = dynamic_cast<OpenDDS::DCPS::DataWriterImpl*>(writer_.in());
  }

  void start ()
  {
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Writer::start \n")));
    if (activate (THR_NEW_LWP | THR_JOINABLE, num_thread_to_write_) == -1)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) Writer::start, %p.\n"),
                  ACE_TEXT("activate")));
      throw TestException ();
    }
  }

  void end ()
  {
    wait ();
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Writer::end \n")));
  }


  /** Lanch a thread to write. **/
  virtual int svc ()
  {
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Writer::svc \n")));

    // Wait for the subscriber to be ready...
    ::DDS::InstanceHandleSeq handles;

    while (1) {

      writer_->get_matched_subscriptions(handles);

      if (handles.length() != 0)
        break;
      else
        ACE_OS::sleep(ACE_Time_Value(0,200000));
    }

    try
    {
      message_type msg;
      msg.sample_sequence = -1;
      msg.handle_value    = -1;
      msg.writer_id       = writer_id_;

      if (true == multiple_instances_)
      {
        // Use the thread id as the instance key.
        msg.a_long_value = ++key;
      }
      else
      {
        msg.a_long_value = default_key;
      }

      datawriter_var msg_dw = datawriter_type::_narrow( writer_.in () );

      TEST_CHECK (! CORBA::is_nil (msg_dw.in ()));

      for (int i = 0; i< num_writes_per_thread_; i ++)
      {
        ::DDS::InstanceHandle_t handle = msg_dw->register_instance(msg);
        msg.handle_value = handle;
        msg.sample_sequence = i;

        // The sequence number will be increased after the insert.
        TEST_CHECK (data_map_.insert (handle, msg) == 0);

        ::DDS::ReturnCode_t ret = ::DDS::RETCODE_OK;

        if (true == keyed_data_)
        {
          message_type key_holder;
          ret = msg_dw->get_key_value(key_holder, handle);

          TEST_CHECK(ret == ::DDS::RETCODE_OK);

          // check for equality
          TEST_CHECK (msg.a_long_value == key_holder.a_long_value);
        }

        if (OpenDDS::DCPS::DCPS_debug_level > 0) {
          ACE_DEBUG ((LM_DEBUG,
                      ACE_TEXT("(%P|%t) write sample: %d \n"),
                      msg.sample_sequence));
        }

        ret = msg_dw->write(msg, handle);
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

      while ( writer_servant_->data_delivered_count_ +
              writer_servant_->data_dropped_count_  <
              num_writes_per_thread_ *
              num_thread_to_write_ )
      {
        ACE_OS::sleep (1);
      }

      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) Writer::svc data_delivered_count=%d ")
                 ACE_TEXT("data_dropped_count=%d\n"),
                 writer_servant_->data_delivered_count_,
                 writer_servant_->data_dropped_count_));
    }

    while (true) {
      writer_->get_matched_subscriptions(handles);
      if (handles.length() == 0)
        break;
      else
        ACE_OS::sleep(ACE_Time_Value(0,200000));
    }

    finished_ = true;

    return 0;
  }

  long writer_id () const
  {
    return writer_id_;
  }

  InstanceDataMap<TypeSupportImpl>& data_map ()
  {
    return data_map_;
  }

  bool finished() { return finished_; }

private:

  InstanceDataMap<TypeSupportImpl> data_map_;
  datawriter_var writer_;
  ::OpenDDS::DCPS::DataWriterImpl* writer_servant_;
  bool keyed_data_;
  long num_thread_to_write_;
  long num_writes_per_thread_;
  bool multiple_instances_;
  long writer_id_;
  long write_delay_msec_;
  long check_data_dropped_;
  bool finished_;
};

class SetHistoryDepthQOS {
public:
    SetHistoryDepthQOS(int depth) { depth_ = depth; }

    void operator()(DDS::TopicQos& qos)
    {
      qos.history.depth = depth_;
    }

    void operator()(DDS::DataWriterQos& qos)
    {
      qos.history.depth = depth_;
    }

  private:
    int depth_;
};

template<typename TypeSupportImpl>
class PubDriver
{
  public:

  typedef typename TypeSupportImpl::message_type message_type;
  typedef typename TypeSupportImpl::datawriter_var datawriter_var;
  typedef typename TypeSupportImpl::datawriter_type datawriter_type;
  typedef typename TypeSupportImpl::datawriterimpl_type datawriterimpl_type;

  PubDriver()
  : keyed_data_ (true),
    num_threads_to_write_ (0),
    multiple_instances_ (false),
    num_writes_per_thread_ (1),
    num_datawriters_ (1),
    max_samples_per_instance_(::DDS::LENGTH_UNLIMITED),
    history_depth_ (1),
    write_delay_msec_ (0),
    check_data_dropped_ (0)
  {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) PubDriver::PubDriver \n")));
  }

  virtual ~PubDriver()
  {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) PubDriver::~PubDriver \n")));

      for (int i = 0; i < num_datawriters_; i ++)
      {
          delete writers_[i];
      }
  }

  void parse_args(::TestUtils::Options& options)
  {

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) PubDriver::parse_args \n")));
    keyed_data_               = options.get<bool>("keyed_data");
    num_threads_to_write_     = options.get<long>("num_threads_to_write");
    multiple_instances_       = options.get<bool>("multiple_instances");
    num_writes_per_thread_    = options.get<long>("num_writes_per_thread");
    num_datawriters_          = options.get<long>("num_writers");
    max_samples_per_instance_ = options.get<long>("max_samples_per_instance");
    history_depth_            = options.get<long>("history_depth");
    write_delay_msec_         = options.get<long>("write_delay_msec");
    check_data_dropped_       = options.get<long>("data_dropped");
  }

  void run(::TestUtils::DDSApp& ddsApp, ::TestUtils::Options& options)
  {
      parse_args(options);
      run(ddsApp);
      end();
  }

  private:

  void run(::TestUtils::DDSApp& ddsApp)
  {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) PubDriver::run \n")));

    SetHistoryDepthQOS history_depth_qos(history_depth_);

    ::TestUtils::DDSTopicFacade< datawriterimpl_type> topic_facade =
      ddsApp.topic_facade< datawriterimpl_type, SetHistoryDepthQOS>
        ("bar", history_depth_qos);

    // Create one datawriter or multiple datawriters belong to the same
    // publisher.
    // Each Writer/DataWriter launch threads to write samples
    // to the same instance or multiple instances.
    // When writing to multiple instances, the instance key
    // identifies instances is the thread id.
    for (int i = 0; i < num_datawriters_; i++)
    {
      datawriters_.push_back(
        topic_facade.writer(history_depth_qos)
      );

      writers_.push_back(
        new Writer< TypeSupportImpl>( datawriters_[i].in (),
                                      keyed_data_,
                                      num_threads_to_write_,
                                      num_writes_per_thread_,
                                      multiple_instances_,
                                      i,
                                      write_delay_msec_,
                                      check_data_dropped_
                                    )
      );

      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) PubDriver::Starting Writer %d \n"), i));
      writers_[i]->start ();
    }

    bool finished = false;
    while (!finished) {
    finished = true;
    for (int i = 0; i < num_datawriters_; i ++) {
      if (!writers_[i]->finished()) {
        finished = false;
        break;
      }
    }
    ACE_OS::sleep(ACE_Time_Value(0,200000));
    }
  }

  void end()
  {
    ACE_DEBUG((LM_DEBUG, "(%P|%t) PubDriver::end \n"));
    // Record samples been written in the Writer's data map.
    // Verify the number of instances and the number of samples
    // written to the datawriter.
    for (int i = 0; i < num_datawriters_; i ++)
    {
      writers_[i]->end ();
      InstanceDataMap<TypeSupportImpl>& map = writers_[i]->data_map ();
      if (multiple_instances_ == false || keyed_data_ == false)
      {
        // One instance when data type has a key value and all instances
        // have the same key or has no key value.
        TEST_CHECK (map.num_instances() == 1);
      } else {
        // multiple instances test - an instance per thread
        TEST_CHECK (map.num_instances() == num_threads_to_write_);
      }
      TEST_CHECK (map.num_samples() == num_threads_to_write_ * num_writes_per_thread_);
    }
  }

  typedef std::vector< datawriter_var > DataWriterVector;
  typedef std::vector< Writer<TypeSupportImpl>* > WriterVector;

  DataWriterVector datawriters_;
  WriterVector writers_;

  bool  keyed_data_;
  long  num_threads_to_write_;
  bool  multiple_instances_;
  long  num_writes_per_thread_;
  long  num_datawriters_;
  long  max_samples_per_instance_;
  long  history_depth_;
  long  write_delay_msec_;
  long  check_data_dropped_;
};

#endif
