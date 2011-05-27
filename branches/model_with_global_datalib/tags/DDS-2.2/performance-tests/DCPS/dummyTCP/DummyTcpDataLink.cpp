// -*- C++ -*-
//
// $Id$

#include "DummyTcp_pch.h"
#include "DummyTcpDataLink.h"
#include "DummyTcpReceiveStrategy.h"
#include "DummyTcpConfiguration.h"
#include "DummyTcpSendStrategy.h"
#include "dds/DCPS/transport/framework/TransportControlElement.h"
#include "dds/DCPS/DataSampleHeader.h"
#include "ace/Log_Msg.h"

#if !defined (__ACE_INLINE__)
#include "DummyTcpDataLink.inl"
#endif /* __ACE_INLINE__ */


OpenDDS::DCPS::DummyTcpDataLink::DummyTcpDataLink
                                        (const ACE_INET_Addr& remote_address,
                                         OpenDDS::DCPS::DummyTcpTransport*  transport_impl)
  : DataLink(transport_impl, 0, false, false), //priority=0, is_loopback=0, is_active=0
    remote_address_(remote_address),
    graceful_disconnect_sent_ (false)
{
  DBG_ENTRY_LVL("DummyTcpDataLink","DummyTcpDataLink",5);
  transport_impl->_add_ref ();
  this->transport_ = transport_impl;
}


OpenDDS::DCPS::DummyTcpDataLink::~DummyTcpDataLink()
{
  DBG_ENTRY_LVL("DummyTcpDataLink","~DummyTcpDataLink",5);
}


/// Called when the DataLink has been "stopped" for some reason.  It could
/// be called from the DataLink::transport_shutdown() method (when the
/// TransportImpl is handling a shutdown() call).  Or, it could be called
/// from the DataLink::release_reservations() method, when it discovers that
/// it has just released the last remaining reservations from the DataLink,
/// and the DataLink is in the process of "releasing" itself.
void
OpenDDS::DCPS::DummyTcpDataLink::stop_i()
{
  DBG_ENTRY_LVL("DummyTcpDataLink","stop_i",5);

  if (!this->connection_.is_nil())
    {
      // Tell the connection object to disconnect.
      this->connection_->disconnect();

      // Drop our reference to the connection object.
      this->connection_ = 0;
    }
}


void
OpenDDS::DCPS::DummyTcpDataLink::pre_stop_i()
{
  DBG_ENTRY_LVL("DummyTcpDataLink","pre_stop_i",5);

  DataLink::pre_stop_i();

  DummyTcpReceiveStrategy * rs
    = dynamic_cast <DummyTcpReceiveStrategy*> (this->receive_strategy_.in ());

  if (rs != NULL)
    {
      // If we received the GRACEFUL_DISCONNECT message from peer before we
      // initiate the disconnecting of the datalink, then we will not send
      // the GRACEFUL_DISCONNECT message to the peer.
      bool disconnected = rs->gracefully_disconnected ();

      if (!this->connection_.is_nil() && !this->graceful_disconnect_sent_
    && ! disconnected)
  {
    this->send_graceful_disconnect_message ();
    this->graceful_disconnect_sent_ = true;
  }
    }

  if (!this->connection_.is_nil())
    {
      this->connection_->shutdown ();
    }
}

/// The DummyTcpTransport calls this method when it has an established
/// connection object for us.  This call puts this DummyTcpDataLink into
/// the "connected" state.
int
OpenDDS::DCPS::DummyTcpDataLink::connect
                                 (DummyTcpConnection*      connection,
                                  TransportSendStrategy*    send_strategy,
                                  TransportReceiveStrategy* receive_strategy)
{
  DBG_ENTRY_LVL("DummyTcpDataLink","connect",5);

  // Sanity check - cannot connect() if we are already connected.
  if (!this->connection_.is_nil())
    {
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) ERROR: DummyTcpDataLink already connected.\n"),
                       -1);
    }

  // Keep a "copy" of the reference to the connection object for ourselves.
  connection->_add_ref();
  this->connection_ = connection;

  // Let connection know the datalink for callbacks upon reconnect failure.
  this->connection_->set_datalink (this);

  // And lastly, inform our base class (DataLink) that we are now "connected",
  // and it should start the strategy objects.
  if (this->start(send_strategy,receive_strategy) != 0)
    {
      // Our base (DataLink) class failed to start the strategy objects.
      // We need to "undo" some things here before we return -1 to indicate
      // that an error has taken place.

      // Drop our reference to the connection object.
      this->connection_ = 0;

      return -1;
    }

  return 0;
}


/// Associate the new connection object with this datalink object.
/// The states of the "old" connection object are copied to the new
/// connection object and the "old" connection object is replaced by
/// the new connection object.
int
OpenDDS::DCPS::DummyTcpDataLink::reconnect (DummyTcpConnection* connection)
{
  DBG_ENTRY_LVL("DummyTcpDataLink","reconnect",5);

  // Sanity check - the connection should exist already since we are reconnecting.
  if (this->connection_.is_nil())
    {
      VDBG_LVL((LM_ERROR,
                "(%P|%t) ERROR: DummyTcpDataLink::reconnect old connection is nil.\n")
               , 1);
      return -1;
    }

  // Keep a "copy" of the reference to the connection object for ourselves.
  connection->_add_ref();
  this->connection_->transfer (connection);
  this->connection_ = connection;

  DummyTcpReceiveStrategy* rs
    = dynamic_cast <DummyTcpReceiveStrategy*> (this->receive_strategy_.in ());

  if (rs == 0)
    {
      ACE_ERROR_RETURN((LM_ERROR,
        "(%P|%t) ERROR: DummyTcpDataLink::reconnect dynamic_cast failed\n"),
        -1);
    }
  // Associate the new connection object with the receiveing strategy and disassociate
  // the old connection object with the receiveing strategy.
  int rs_result = rs->reset (this->connection_.in ());

  DummyTcpSendStrategy* ss
    = dynamic_cast <DummyTcpSendStrategy*> (this->send_strategy_.in ());

  if (ss == 0)
    {
      ACE_ERROR_RETURN((LM_ERROR,
        "(%P|%t) ERROR: DummyTcpDataLink::reconnect dynamic_cast failed\n"),
        -1);
    }
  // Associate the new connection object with the sending strategy and disassociate
  // the old connection object with the sending strategy.
  int ss_result = ss->reset (this->connection_.in ());

  if (rs_result == 0 && ss_result == 0)
  {
    return 0;
  }

  return -1;
}



void
OpenDDS::DCPS::DummyTcpDataLink::send_graceful_disconnect_message ()
{
  DBG_ENTRY_LVL("DummyTcpDataLink","send_graceful_disconnect_message",5);

  // Will clear all queued messages but still let the disconnect message
  // sent.
  this->send_strategy_->terminate_send (true);

  DataSampleHeader header_data;
  // The message_id_ is the most important value for the DataSampleHeader.
  header_data.message_id_ = GRACEFUL_DISCONNECT;

  // Other data in the DataSampleHeader are not necessary set. The bogus values
  // can be used.

  //header_data.byte_order_
  //  = this->transport_->get_configuration()->swap_bytes() ? !TAO_ENCAP_BYTE_ORDER : TAO_ENCAP_BYTE_ORDER;
  //header_data.message_length_ = 0;
  //header_data.sequence_ = 0;
  //::DDS::Time_t source_timestamp
  //  = ::OpenDDS::DCPS::time_value_to_time (ACE_OS::gettimeofday ());
  //header_data.source_timestamp_sec_ = source_timestamp.sec;
  //header_data.source_timestamp_nanosec_ = source_timestamp.nanosec;
  //header_data.coherency_group_ = 0;
  //header_data.publication_id_ = 0;

  // TODO:
  // It seems a bug in the transport implementation that the receiving side can
  // not receive the message when the message has no sample data and is sent
  // in a single packet.

  // To work arround this problem, I have to add bogus data to chain with the
  // DataSampleHeader to make the receiving work.
  ACE_Message_Block* message;
  size_t max_marshaled_size = header_data.max_marshaled_size ();
  ACE_Message_Block* data = 0;
  ACE_NEW (data,
    ACE_Message_Block(20,
    ACE_Message_Block::MB_DATA,
    0, //cont
    0, //data
    0, //allocator_strategy
    0, //locking_strategy
    ACE_DEFAULT_MESSAGE_BLOCK_PRIORITY,
    ACE_Time_Value::zero,
    ACE_Time_Value::max_time,
    0,
    0));
  data->wr_ptr (20);

  header_data.message_length_ = data->length ();

  ACE_NEW (message,
    ACE_Message_Block(max_marshaled_size,
    ACE_Message_Block::MB_DATA,
    data, //cont
    0, //data
    0, //allocator_strategy
    0, //locking_strategy
    ACE_DEFAULT_MESSAGE_BLOCK_PRIORITY,
    ACE_Time_Value::zero,
    ACE_Time_Value::max_time,
    0,
    0));

  message << header_data;

  TransportControlElement* send_element = 0;

  ACE_NEW(send_element, TransportControlElement(message));

  // I don't want to rebuild a connection in order to send
  // a graceful disconnect message.
  this->send_i (send_element, false);
}


void
OpenDDS::DCPS::DummyTcpDataLink::fully_associated ()
{
  DBG_ENTRY_LVL("DummyTcpDataLink","fully_associated",5);

  while ( ! this->connection_->is_connected ())
  {
    ACE_Time_Value tv (0, 100000);
    ACE_OS::sleep (tv);
  }
  this->resume_send ();
  bool swap_byte = this->transport_->get_configuration()->swap_bytes_;
  DataSampleHeader header_data;
  // The message_id_ is the most important value for the DataSampleHeader.
  header_data.message_id_ = FULLY_ASSOCIATED;

  // Other data in the DataSampleHeader are not necessary set. The bogus values
  // can be used.

  header_data.byte_order_
    = swap_byte ? !TAO_ENCAP_BYTE_ORDER : TAO_ENCAP_BYTE_ORDER;
  //header_data.message_length_ = 0;
  //header_data.sequence_ = 0;
  //::DDS::Time_t source_timestamp
  //  = ::OpenDDS::DCPS::time_value_to_time (ACE_OS::gettimeofday ());
  //header_data.source_timestamp_sec_ = source_timestamp.sec;
  //header_data.source_timestamp_nanosec_ = source_timestamp.nanosec;
  //header_data.coherency_group_ = 0;
  //header_data.publication_id_ = 0;

  ACE_Message_Block* message;
  size_t max_marshaled_size = header_data.max_marshaled_size ();

  ACE_Message_Block* data = this->marshal_acks (swap_byte);

  header_data.message_length_ = data->length ();

  ACE_NEW (message,
    ACE_Message_Block(max_marshaled_size,
    ACE_Message_Block::MB_DATA,
    data, //cont
    0, //data
    0, //allocator_strategy
    0, //locking_strategy
    ACE_DEFAULT_MESSAGE_BLOCK_PRIORITY,
    ACE_Time_Value::zero,
    ACE_Time_Value::max_time,
    0,
    0));

  message << header_data;

  TransportControlElement* send_element = 0;

  ACE_NEW(send_element, TransportControlElement(message));

  this->send_i (send_element);
}
