// -*- C++ -*-
//
// $Id$

using namespace TAO::DCPS ;


ACE_INLINE
::DDS::DataReader_ptr 
DataReaderImpl::get_dr_obj_ref() { 
#if 1
  //TBD this may be faster than servant_to_reference - ?significantly?
  return ::TAO::DCPS::DataReaderRemote::_duplicate (dr_remote_objref_.in ()) ;
#else
#if 0
  // we don't really need the RemoteDataReader OR a DataReder OR will do.
  return this->get_datareaderremote_obj_ref ();
#else
  ::DDS::DataReader_ptr reader_obj 
      = ::TAO::DCPS::servant_to_reference< ::DDS::DataReader, 
                                           TAO::DCPS::DataReaderImpl, 
                                           ::DDS::DataReader_ptr> 
            (this ACE_ENV_ARG_PARAMETER);
  ACE_CHECK_RETURN (::DDS::DataReader::_nil());
  // TAO::DCPS::servant_to_reference narrowed so it has been dup'd
  return reader_obj;
#endif
#endif
}


ACE_INLINE
int 
WriterInfo::received_activity (const ACE_Time_Value& when)
{ 
  last_liveliness_activity_time_ = when;
  if (!is_alive_)
    {
      is_alive_ = 1;
      reader_->writer_became_alive (writer_id_, when);
      return 0;
    }
  //TBD - is the "was alive" return value used?
  return 1;
}


