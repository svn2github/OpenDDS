// -*- C++ -*-
//
// $Id$
#include "SubReader.h"
#include "dds/DCPS/transport/framework/ReceivedDataSample.h"
#include "dds/DCPS/GuidBuilder.h"


SubReader::SubReader()
  : sub_id_(OpenDDS::DCPS::GuidBuilder::create ()),
    data_size_(0),
    num_expected_(0),
    num_received_(0)
{
}


SubReader::~SubReader()
{
}


void
SubReader::set_num_to_receive(unsigned num_to_receive)
{
  this->num_expected_ = num_to_receive;
}


void
SubReader::set_data_size(char data_size)
{
  this->data_size_ = data_size;
}


void
SubReader::set_id(OpenDDS::DCPS::RepoId sub_id)
{
  this->sub_id_ = sub_id;
}


bool
SubReader::is_done() const
{
  return (this->num_expected_ == this->num_received_);
}


void
SubReader::transport_lost()
{
}


void
SubReader::data_received(const OpenDDS::DCPS::ReceivedDataSample& sample)
{
  ++this->num_received_;
  ACE_DEBUG((LM_DEBUG,
             "(%P|%t) Received message with sequence == %q\n",
             sample.header_.sequence_.getValue()));
}
