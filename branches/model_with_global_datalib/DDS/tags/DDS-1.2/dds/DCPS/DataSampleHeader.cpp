// -*- C++ -*-
//
// $Id$

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "DataSampleHeader.h"
#include "Serializer.h"

#include <iostream>

#if ! defined (__ACE_INLINE__)
#include "DataSampleHeader.inl"
#endif /* __ACE_INLINE__ */

void
OpenDDS::DCPS::DataSampleHeader::init (ACE_Message_Block* buffer)
{
  this->marshaled_size_ = 0;

  TAO::DCPS::Serializer reader( buffer )  ;
  // TODO: Now it's ok to serialize the message_id before flag byte
  // since the message_id_ is defined as char. If the message_id_ 
  // is changed to be defined as a type with multiple bytes then 
  // we need define it after the flag byte or serialize flag byte before
  // serializing the message_id_. I think the former approach is simpler
  // than the latter approach.
  reader >> this->message_id_ ;
  if( reader.good_bit() != true) return ;
  this->marshaled_size_ += sizeof( this->message_id_) ;

  // Extract the flag values.
  ACE_CDR::Octet byte ;
  reader >> ACE_InputCDR::to_octet(byte) ;
  if( reader.good_bit() != true) return ;
  this->marshaled_size_ += sizeof( byte) ;

  this->byte_order_  = ((byte & 0x01) != 0) ;
  this->last_sample_ = ((byte & 0x02) != 0) ;
  this->reserved_1   = ((byte & 0x04) != 0) ;
  this->reserved_2   = ((byte & 0x08) != 0) ;
  this->reserved_3   = ((byte & 0x10) != 0) ;
  this->reserved_4   = ((byte & 0x20) != 0) ;
  this->reserved_5   = ((byte & 0x40) != 0) ;
  this->reserved_6   = ((byte & 0x80) != 0) ;

  // Set swap_bytes flag to the Serializer if data sample from
  // the publisher is in different byte order.
  reader.swap_bytes (this->byte_order_ != TAO_ENCAP_BYTE_ORDER);

  reader >> this->message_length_ ;
  if( reader.good_bit() != true) return ;
  this->marshaled_size_ += sizeof( this->message_length_) ;

  reader >> this->sequence_ ;
  if( reader.good_bit() != true) return ;
  this->marshaled_size_ += sizeof( this->sequence_) ;

  reader >> this->source_timestamp_sec_ ;
  if( reader.good_bit() != true) return ;
  this->marshaled_size_ += sizeof( this->source_timestamp_sec_) ;

  reader >> this->source_timestamp_nanosec_ ;
  if( reader.good_bit() != true) return ;
  this->marshaled_size_ += sizeof( this->source_timestamp_nanosec_) ;

  reader >> this->coherency_group_ ;
  if( reader.good_bit() != true) return ;
  this->marshaled_size_ += sizeof( this->coherency_group_) ;

  // Publication ID is complex structure of bytes, but of known size -
  // just slurp the whole structure.
  this->publication_id_ = GUID_UNKNOWN;
  reader.read_octet_array(
    reinterpret_cast<ACE_CDR::Octet*>( &this->publication_id_),
    sizeof(this->publication_id_)
  );
  this->marshaled_size_ += sizeof( this->publication_id_) ;

}

ACE_CDR::Boolean
operator<< (ACE_Message_Block*& buffer, OpenDDS::DCPS::DataSampleHeader& value)
{
  TAO::DCPS::Serializer writer( buffer, value.byte_order_ != TAO_ENCAP_BYTE_ORDER) ;

  writer << value.message_id_ ;

  // Write the flags as a single byte.
  ACE_CDR::Octet flags = (value.byte_order_  << 0)
                       | (value.last_sample_ << 1)
                       | (value.reserved_1   << 2)
                       | (value.reserved_2   << 3)
                       | (value.reserved_3   << 4)
                       | (value.reserved_4   << 5)
                       | (value.reserved_5   << 6)
                       | (value.reserved_6   << 7)
                       ;
  writer << ACE_OutputCDR::from_octet (flags);
  writer << value.message_length_ ;
  writer << value.sequence_ ;
  writer << value.source_timestamp_sec_ ;
  writer << value.source_timestamp_nanosec_ ;
  writer << value.coherency_group_ ;

  writer.write_octet_array(
    reinterpret_cast<ACE_CDR::Octet*>( &value.publication_id_),
    sizeof(value.publication_id_)
  );

  return writer.good_bit() ;
}

/// Message Id enumarion insertion onto an ostream.
std::ostream& operator<<( std::ostream& str, const OpenDDS::DCPS::MessageId value)
{
  switch( value) {
    case OpenDDS::DCPS::SAMPLE_DATA:           return str << "SAMPLE_DATA";
    case OpenDDS::DCPS::DATAWRITER_LIVELINESS: return str << "DATAWRITER_LIVELINESS";
    case OpenDDS::DCPS::INSTANCE_REGISTRATION: return str << "INSTANCE_REGISTRATION";
    case OpenDDS::DCPS::UNREGISTER_INSTANCE:   return str << "UNREGISTER_INSTANCE";
    case OpenDDS::DCPS::DISPOSE_INSTANCE:      return str << "DISPOSE_INSTANCE";
    case OpenDDS::DCPS::GRACEFUL_DISCONNECT:   return str << "GRACEFUL_DISCONNECT";
    case OpenDDS::DCPS::FULLY_ASSOCIATED:      return str << "FULLY_ASSOCIATED";
    default:                                   return str << "UNSPECIFIED(" << int(value) << ")";
  }
}

/// Message header insertion onto an ostream.
extern OpenDDS_Dcps_Export
std::ostream& operator<<( std::ostream& str, const OpenDDS::DCPS::DataSampleHeader& value)
{
  str << "[";

  str << OpenDDS::DCPS::MessageId( value.message_id_) << ", ";
  if( value.last_sample_ == 1) {
    str << "last sample, ";
  }
  if( value.byte_order_ == 1) {
    str << "network order, ";
  } else {
    str << "little endian, ";
  }
  str << std::dec << value.message_length_ << ", ";
  str << "0x" << std::hex << value.sequence_ << ", ";
  str << "(" << std::dec << value.source_timestamp_sec_ << "/";
  str << std::dec << value.source_timestamp_nanosec_ << "), ";
  str << std::dec << value.coherency_group_ << ", ";

  long key = OpenDDS::DCPS::GuidConverter(
               const_cast< OpenDDS::DCPS::GUID_t*>( &value.publication_id_)
             );
  str << value.publication_id_ << "(" << std::hex << key << ")";

  str << "]";
  return str;
}

