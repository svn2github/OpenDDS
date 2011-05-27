// -*- C++ -*-
// $Id$

#include "dds/DdsDcpsDomainC.h"
#include "dds/DCPS/transport/framework/TransportImpl_rch.h"
#include "dds/DCPS/WaitSet.h"

#include <map>

namespace Test {

class Options;
class DataReaderListener;

class Subscriber {
  public:
    /// Construct with option information.
    Subscriber( const Options& options);

    /// Destructor.
    ~Subscriber();

    /// Execute the test.
    void run();

    /// Number of samples received during the test from each writer.
    const std::map< long, long>& counts() const;

    /// Number of payload bytes received during the test from each writer.
    const std::map< long, long>& bytes() const;

    /// Priority of  writers.
    const std::map< long, long>& priorities() const;

  private:
    /// Test options.
    const Options& options_;

    /// Test transport.
    OpenDDS::DCPS::TransportImpl_rch transport_;

    /// DomainParticipant.
    DDS::DomainParticipant_var participant_;

    /// Topic.
    DDS::Topic_var topic_;

    /// Subscriber.
    DDS::Subscriber_var subscriber_;

    /// Reader.
    DDS::DataReader_var reader_;

    /// Reader listener.
    DataReaderListener* listener_;

    /// Blocking object for test synchronization.
    DDS::WaitSet_var waiter_;

    /// Blocking condition for test synchronization.
    DDS::StatusCondition_var status_;
};

} // End of namespace Test

