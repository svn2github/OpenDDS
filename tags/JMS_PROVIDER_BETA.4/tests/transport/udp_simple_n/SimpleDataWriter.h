// -*- C++ -*-
//
// $Id$
#ifndef SIMPLEDATAWRITER_H
#define SIMPLEDATAWRITER_H

#include "dds/DCPS/transport/framework/TransportSendListener.h"
#include "dds/DCPS/Definitions.h"

class SimplePublisher;


class SimpleDataWriter : public OpenDDS::DCPS::TransportSendListener
{
  public:

    SimpleDataWriter();
    ~SimpleDataWriter();

    void init(OpenDDS::DCPS::RepoId pub_id);
    int  run(SimplePublisher* publisher, unsigned num_messages);

    // This means that the TransportImpl has been shutdown, making the
    // transport_interface sent to the run() method no longer valid.
    // This method should effectively block until the run() is notified,
    // and promises to stop using the transport_interface object
    // anymore.  This would really only mean something if our run() is
    // is running in a thread other than the thread that told the
    // TransportImpl to shutdown.  For now, this will just report a log
    // message.
    void transport_lost();

    virtual void data_delivered(OpenDDS::DCPS::DataSampleListElement* sample);
    virtual void data_dropped(OpenDDS::DCPS::DataSampleListElement* sample,
                              bool dropped_by_transport = false);

    int delivered_test_message();


  private:

    OpenDDS::DCPS::RepoId pub_id_;
    int num_messages_sent_;
    int num_messages_delivered_;
};

#endif  /* SIMPLEDATAWRITER_H */
