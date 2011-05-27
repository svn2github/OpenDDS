/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "ace/OS_main.h"

#include "dds/DCPS/DataSampleHeader.h"

#include "../common/TestSupport.h"

using namespace OpenDDS::DCPS;

struct Fragments {
  ACE_Message_Block* head_;
  ACE_Message_Block* tail_;

  Fragments() : head_(0), tail_(0) {}

  ~Fragments()
  {
    ACE_Message_Block::release(head_);
    ACE_Message_Block::release(tail_);
  }
};

int ACE_TMAIN(int, ACE_TCHAR*[])
{
  DataSampleHeader dsh;
  const size_t N = 300;
  dsh.message_length_ = N;
  ACE_Message_Block header_mb(DataSampleHeader::max_marshaled_size());
  header_mb << dsh;

  { // simple case: no content-filter, data in a single messageblock
    ACE_Message_Block data(N);
    data.wr_ptr(N);
    header_mb.cont(&data);
    Fragments f;
    DataSampleHeader::split(header_mb, N / 2 + header_mb.length(),
      f.head_, f.tail_);

    DataSampleHeader header1(*f.head_);
    TEST_CHECK(header1.more_fragments_);
    TEST_CHECK(header1.message_length_ == N / 2);
    TEST_CHECK(f.head_->cont());
    TEST_CHECK(f.head_->cont()->length() == N / 2);
    TEST_CHECK(!f.head_->cont()->cont());

    DataSampleHeader header2(*f.tail_);
    TEST_CHECK(!header2.more_fragments_);
    TEST_CHECK(header2.message_length_ == N / 2);
    TEST_CHECK(f.tail_->cont());
    TEST_CHECK(f.tail_->cont()->length() == N / 2);
    TEST_CHECK(!f.tail_->cont()->cont());

    DataSampleHeader reassembled;
    TEST_CHECK(DataSampleHeader::join(header1, header2, reassembled));
    TEST_CHECK(reassembled.sequence_ == header1.sequence_);
    TEST_CHECK(reassembled.sequence_ == header2.sequence_);
    TEST_CHECK(!reassembled.more_fragments_);
    TEST_CHECK(reassembled.message_length_ == N);
  }
  { // data split into 3 messageblocks, fragment in the middle of block #2
    ACE_Message_Block data1(N / 3), data2(N / 3), data3(N / 3);
    data1.wr_ptr(N / 3);
    data2.wr_ptr(N / 3);
    data3.wr_ptr(N / 3);
    header_mb.cont(&data1);
    data1.cont(&data2);
    data2.cont(&data3);
    Fragments f;
    DataSampleHeader::split(header_mb, N / 2 + header_mb.length(),
      f.head_, f.tail_);

    DataSampleHeader header(*f.head_);
    TEST_CHECK(header.more_fragments_);
    TEST_CHECK(header.message_length_ == N / 2);
    TEST_CHECK(f.head_->cont());
    TEST_CHECK(f.head_->cont()->length() == N / 3);
    TEST_CHECK(f.head_->cont()->cont()->length() == N / 6);
    TEST_CHECK(!f.head_->cont()->cont()->cont());

    header = *f.tail_;
    TEST_CHECK(!header.more_fragments_);
    TEST_CHECK(header.message_length_ == N / 2);
    TEST_CHECK(f.tail_->cont());
    TEST_CHECK(f.tail_->cont()->length() == N / 6);
    TEST_CHECK(f.tail_->cont()->cont()->length() == N / 3);
    TEST_CHECK(!f.tail_->cont()->cont()->cont());
  }
  { // data split into 4 messageblocks, fragment at the block division
    ACE_Message_Block data1(N / 4), data2(N / 4), data3(N / 4), data4(N / 4);
    data1.wr_ptr(N / 4);
    data2.wr_ptr(N / 4);
    data3.wr_ptr(N / 4);
    data4.wr_ptr(N / 4);
    header_mb.cont(&data1);
    data1.cont(&data2);
    data2.cont(&data3);
    data3.cont(&data4);
    Fragments f;
    DataSampleHeader::split(header_mb, N / 2 + header_mb.length(),
      f.head_, f.tail_);

    DataSampleHeader header(*f.head_);
    TEST_CHECK(header.more_fragments_);
    TEST_CHECK(header.message_length_ == N / 2);
    TEST_CHECK(f.head_->cont());
    TEST_CHECK(f.head_->cont()->length() == N / 4);
    TEST_CHECK(f.head_->cont()->cont()->length() == N / 4);
    TEST_CHECK(!f.head_->cont()->cont()->cont());

    header = *f.tail_;
    TEST_CHECK(!header.more_fragments_);
    TEST_CHECK(header.message_length_ == N / 2);
    TEST_CHECK(f.tail_->cont());
    TEST_CHECK(f.tail_->cont()->length() == N / 4);
    TEST_CHECK(f.tail_->cont()->cont()->length() == N / 4);
    TEST_CHECK(!f.tail_->cont()->cont()->cont());
  }
  { // content filtering flag with no "entries" (adds another MB to the chain)
    ACE_Message_Block data(N);
    data.wr_ptr(N);
    header_mb.cont(&data);
    DataSampleHeader::set_flag(CONTENT_FILTER_FLAG, &header_mb);
    DataSampleHeader::add_cfentries(0, &header_mb);
    Fragments f;
    const size_t FRAG = 100; // arbitrary split at 100 bytes
    DataSampleHeader::split(header_mb, FRAG, f.head_, f.tail_);

    DataSampleHeader header1(*f.head_);
    TEST_CHECK(header1.more_fragments_);
    TEST_CHECK(header1.content_filter_);
    TEST_CHECK(header1.content_filter_entries_.length() == 0);
    const size_t hdr_len = header_mb.length() + header_mb.cont()->length();
    TEST_CHECK(header1.message_length_ == FRAG - hdr_len);
    TEST_CHECK(f.head_->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.head_->cont());
    TEST_CHECK(f.head_->cont()->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.head_->cont()->cont());
    TEST_CHECK(f.head_->cont()->cont()->length() == header1.message_length_);
    TEST_CHECK(!f.head_->cont()->cont()->cont());

    DataSampleHeader header2(*f.tail_);
    TEST_CHECK(!header2.more_fragments_);
    TEST_CHECK(!header2.content_filter_);
    TEST_CHECK(header2.message_length_ == N - (FRAG - hdr_len));
    TEST_CHECK(f.tail_->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.tail_->cont());
    TEST_CHECK(f.tail_->cont()->length() == header2.message_length_);
    TEST_CHECK(!f.tail_->cont()->cont());

    DataSampleHeader reassembled;
    TEST_CHECK(DataSampleHeader::join(header1, header2, reassembled));
    TEST_CHECK(reassembled.sequence_ == header1.sequence_);
    TEST_CHECK(reassembled.sequence_ == header2.sequence_);
    TEST_CHECK(!reassembled.more_fragments_);
    TEST_CHECK(reassembled.message_length_ == N);
    TEST_CHECK(reassembled.content_filter_);
  }
  { // content filtering with some "entries" that all fit in the fragment
    ACE_Message_Block data(N);
    data.wr_ptr(N);
    header_mb.cont(&data);
    DataSampleHeader::set_flag(CONTENT_FILTER_FLAG, &header_mb);
    const size_t CF_ENTRIES = 6; // serializes to 100 bytes
    GUIDSeq entries(CF_ENTRIES);
    entries.length(CF_ENTRIES);
    DataSampleHeader::add_cfentries(&entries, &header_mb);
    Fragments f;
    const size_t FRAG = 200;
    DataSampleHeader::split(header_mb, FRAG, f.head_, f.tail_);

    DataSampleHeader header1(*f.head_);
    TEST_CHECK(header1.more_fragments_);
    TEST_CHECK(header1.content_filter_);
    TEST_CHECK(header1.content_filter_entries_.length() == CF_ENTRIES);
    TEST_CHECK(header1.message_length_ > 0);
    TEST_CHECK(f.head_->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.head_->cont());
    TEST_CHECK(f.head_->cont()->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.head_->cont()->cont());
    const size_t frag_payload = f.head_->cont()->cont()->length();
    TEST_CHECK(frag_payload > 0);
    TEST_CHECK(!f.head_->cont()->cont()->cont());

    DataSampleHeader header2(*f.tail_);
    TEST_CHECK(!header2.more_fragments_);
    TEST_CHECK(!header2.content_filter_);
    TEST_CHECK(header2.message_length_ + frag_payload == N);
    TEST_CHECK(f.tail_->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.tail_->cont());
    TEST_CHECK(f.tail_->cont()->length() == header2.message_length_);
    TEST_CHECK(!f.tail_->cont()->cont());

    DataSampleHeader reassembled;
    TEST_CHECK(DataSampleHeader::join(header1, header2, reassembled));
    TEST_CHECK(reassembled.sequence_ == header1.sequence_);
    TEST_CHECK(reassembled.sequence_ == header2.sequence_);
    TEST_CHECK(!reassembled.more_fragments_);
    TEST_CHECK(reassembled.message_length_ == N);
    TEST_CHECK(reassembled.content_filter_);
    TEST_CHECK(reassembled.content_filter_entries_.length() == CF_ENTRIES);
  }
  { // content filtering with some "entries", split inside the entires
    ACE_Message_Block data(N);
    data.wr_ptr(N);
    header_mb.cont(&data);
    DataSampleHeader::set_flag(CONTENT_FILTER_FLAG, &header_mb);
    const size_t CF_ENTRIES = 6; // serializes to 100 bytes
    GUIDSeq entries(CF_ENTRIES);
    entries.length(CF_ENTRIES);
    DataSampleHeader::add_cfentries(&entries, &header_mb);
    Fragments f;
    const size_t FRAG = 68;
    DataSampleHeader::split(header_mb, FRAG, f.head_, f.tail_);

    DataSampleHeader header1(*f.head_);
    TEST_CHECK(header1.more_fragments_);
    TEST_CHECK(header1.content_filter_);
    const size_t entries_in_header = header1.content_filter_entries_.length();
    TEST_CHECK(entries_in_header > 0);
    TEST_CHECK(header1.message_length_ == 0);
    TEST_CHECK(f.head_->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.head_->cont());
    TEST_CHECK(f.head_->cont()->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(!f.head_->cont()->cont());

    DataSampleHeader header2(*f.tail_);
    TEST_CHECK(!header2.more_fragments_);
    TEST_CHECK(header2.content_filter_);
    TEST_CHECK(header2.content_filter_entries_.length()
               + entries_in_header == CF_ENTRIES);
    TEST_CHECK(header2.message_length_ == N);
    TEST_CHECK(f.tail_->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.tail_->cont());
    TEST_CHECK(f.tail_->cont()->length() == 0); // consumed by DataSampleHeader
    TEST_CHECK(f.tail_->cont()->cont());
    TEST_CHECK(f.tail_->cont()->cont()->length() == N);
    TEST_CHECK(!f.tail_->cont()->cont()->cont());

    DataSampleHeader reassembled;
    TEST_CHECK(DataSampleHeader::join(header1, header2, reassembled));
    TEST_CHECK(reassembled.sequence_ == header1.sequence_);
    TEST_CHECK(reassembled.sequence_ == header2.sequence_);
    TEST_CHECK(!reassembled.more_fragments_);
    TEST_CHECK(reassembled.message_length_ == N);
    TEST_CHECK(reassembled.content_filter_);
    TEST_CHECK(reassembled.content_filter_entries_.length() == CF_ENTRIES);
  }
  return 0;
}
