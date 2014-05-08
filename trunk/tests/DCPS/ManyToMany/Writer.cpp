/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>
#include <ace/OS_NS_unistd.h>

#include <dds/DdsDcpsPublicationC.h>
#include <dds/DCPS/WaitSet.h>

#include "tests/DCPS/LargeSample/MessengerTypeSupportC.h"
#include "Writer.h"

const int num_messages = 10;

Writer::Writer(const Options& options, Writers& writers)
  : options_(options)
  , writers_(writers)
{
}

namespace {
  void wait_for_match(DDS::DataWriter_ptr writer, const unsigned int count)
  {
    DDS::StatusCondition_var condition = writer->get_statuscondition();
    condition->set_enabled_statuses(DDS::PUBLICATION_MATCHED_STATUS);

    DDS::WaitSet_var ws = new DDS::WaitSet;
    ws->attach_condition(condition);

    DDS::Duration_t timeout =
      { DDS::DURATION_INFINITE_SEC, DDS::DURATION_INFINITE_NSEC };

    DDS::ConditionSeq conditions;
    DDS::PublicationMatchedStatus matches = {0, 0, 0, 0, 0};

    while (true) {
      if (writer->get_publication_matched_status(matches) != ::DDS::RETCODE_OK) {
        ACE_ERROR((LM_ERROR,
                   ACE_TEXT("%N:%l: wait_for_match()")
                   ACE_TEXT(" ERROR: get_publication_matched_status failed!\n")));
        ACE_OS::exit(-1);
      }

      if (matches.current_count < (int)count) {
        if (ws->wait(conditions, timeout) != DDS::RETCODE_OK) {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("%N:%l: wait_for_match()")
                     ACE_TEXT(" ERROR: wait failed!\n")));
          ACE_OS::exit(-1);
        }
      } else {
        break;
      }
    }
    ws->detach_condition(condition);
  }
}

void
Writer::write()
{
  try {
    typedef std::vector<DDS::InstanceHandle_t> Handles;
    Handles handles;
    const unsigned int subscribers = options_.num_sub_processes *
      options_.num_sub_participants * options_.num_readers;
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("%T (%P|%t) Writers wait for %d subscribers\n"),
               subscribers));

    // Block until Subscriber is available
    for (Writers::const_iterator writer = writers_.begin();
         writer != writers_.end();
         ++writer) {
      wait_for_match(writer->writer, subscribers);

      // we already have a ref count, no need to take another
      Messenger::MessageDataWriter_ptr message_dw =
        dynamic_cast<Messenger::MessageDataWriter*>(writer->writer.in());

      if (CORBA::is_nil(message_dw)) {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("%N:%l: svc()")
                     ACE_TEXT(" ERROR: _narrow dw1 failed!\n")));
          ACE_OS::exit(-1);
      }

      handles.push_back(message_dw->register_instance(writer->message));
    }

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) Writers matched\n")));

    const ACE_Time_Value delay(options_.delay_msec / 1000,
                               (options_.delay_msec % 1000) * 1000);

    for (unsigned int i = 0; i < options_.num_samples; i++) {
      Handles::iterator handle = handles.begin();
      for (Writers::iterator writer = writers_.begin();
           writer != writers_.end();
           ++writer, ++handle) {
        // we already have a ref count, no need to take another
        // Write samples
        Messenger::MessageDataWriter_ptr message_dw =
          dynamic_cast<Messenger::MessageDataWriter*>(writer->writer.in());

        ++writer->message.count;

        DDS::ReturnCode_t error = message_dw->write(writer->message, *handle);

        if (error != DDS::RETCODE_OK) {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("%N:%l: svc()")
                     ACE_TEXT(" ERROR: writer returned %d!\n"), error));

        }
        ACE_OS::sleep(delay);
      }
    }

    // Let readers disconnect first, once they either get the data or
    // give up and time-out.  This allows the writer to be alive while
    // processing requests for retransmission from the readers.
    for (Writers::const_iterator writer = writers_.begin();
         writer != writers_.end();
         ++writer) {
      wait_for_match(writer->writer, 0);
    }
  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in svc():");
  }
}
