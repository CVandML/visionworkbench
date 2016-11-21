// __BEGIN_LICENSE__
//  Copyright (c) 2006-2013, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NASA Vision Workbench is licensed under the Apache License,
//  Version 2.0 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__


#include <gtest/gtest_VW.h>
#include <test/Helpers.h>

#include <vw/Plate/Blob.h>
#include <vw/Plate/BlobManager.h>
#include <vw/Plate/Rpc.pb.h>

#include <boost/filesystem/operations.hpp>
namespace fs = boost::filesystem;

using namespace std;
using namespace vw;
using namespace vw::platefile;
using namespace vw::test;

namespace vw {
namespace platefile {

// assertion types need to have an ostream operator
std::ostream& operator<<(std::ostream& o, const vw::platefile::Blob::iterator& i) {
  o << "Blob::iterator<" << i.current_base_offset() << ">";
  return o;
}

}} // vw::platefile

class BlobIOTest : public ::testing::Test {
  protected:

  virtual void SetUp() {
    blob_path = UnlinkName("BlobIO");

    hdr.set_filetype("tif");
    hdr.set_col(0);
    hdr.set_row(0);
    hdr.set_level(0);
    hdr.set_transaction_id(1);
  }

  static const size_t data_size = 20;
  static const uint8 test_data[data_size];
  UnlinkName blob_path;

  TileHeader hdr;
};
const size_t BlobIOTest::data_size;
const uint8 BlobIOTest::test_data[BlobIOTest::data_size] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};

TEST_F(BlobIOTest, WriteThenRead) {

  // First test, creates a new blob file.
  int64 offset;
  {
    Blob blob(blob_path);

    // Write the data to the file.
    offset = blob.write(hdr, test_data, data_size);

    // Test reading of the data
    TileData verify_data = blob.read_data(offset);
    EXPECT_RANGE_EQ(test_data+0, test_data+data_size, verify_data->begin(), verify_data->end());

    // Test Reading of the header
    TileHeader hdr2 = blob.read_header(offset);
    EXPECT_EQ(hdr.filetype(), hdr2.filetype());
    EXPECT_EQ(hdr.col(), hdr2.col());
    EXPECT_EQ(hdr.row(), hdr2.row());
    EXPECT_EQ(hdr.level(), hdr2.level());
  }

  // Verify that a read-only blob does it right
  {
    ReadBlob blob(blob_path);

    // Test reading of the data
    TileData verify_data = blob.read_data(offset);
    EXPECT_RANGE_EQ(test_data+0, test_data+data_size, verify_data->begin(), verify_data->end());

    // Test Reading of the header
    TileHeader hdr2 = blob.read_header(offset);
    EXPECT_EQ(hdr.filetype(), hdr2.filetype());
    EXPECT_EQ(hdr.col(), hdr2.col());
    EXPECT_EQ(hdr.row(), hdr2.row());
    EXPECT_EQ(hdr.level(), hdr2.level());
  }

  // Second test, appends to a blob file.
  {
    Blob blob(blob_path);

    // Write the data to the file.
    offset = blob.write(hdr, test_data, data_size);

    // Read it back in.
    TileData verify_data = blob.read_data(offset);
    EXPECT_RANGE_EQ(test_data+0, test_data+data_size, verify_data->begin(), verify_data->end());
  }

  {
    ReadBlob blob(blob_path);
    TileData verify_data = blob.read_data(offset);
    EXPECT_RANGE_EQ(test_data+0, test_data+data_size, verify_data->begin(), verify_data->end());
  }
}

#if 0
// This test needs to be updated with some test material (and it should use the
// fixture, and not use hard-coded paths)
TEST_F(BlobIOTest, DISABLED_WriteFromFile) {

  const char* f1 = "/tmp/foo.blob";
  const char* f2 = "/tmp/foo3.blob";
  fs::remove(f2);

  Blob blob("/tmp/foo2.blob");

  // Do one loop through the blob file, placing f1 into the file,
  // and then reading it back out and saving it as f2.
  uint64 offset;
  blob.write_from_file(f1, hdr, offset);
  blob.read_to_file(f2, offset);

  // ----

  // Now, check to make sure it worked!!
  std::ifstream istr1(f1, ios::binary);
  std::ifstream istr2(f2, ios::binary);

  // Check to see if files are the same size
  istr1.seekg (0, std::ios::end);
  istr2.seekg (0, std::ios::end);
  int64 size1 = istr1.tellg();
  int64 size2 = istr2.tellg();
  EXPECT_EQ(size1, size2);

  if (size1 == size2) {
    istr1.seekg (0, std::ios::beg);
    istr2.seekg (0, std::ios::beg);

    boost::shared_array<char> data1(new char[size1]);
    boost::shared_array<char> data2(new char[size2]);

    istr1.read(data1.get(), size1);
    istr2.read(data2.get(), size2);

    for (int64 i=0; i < size1; ++i) {
      EXPECT_EQ(data1[i], data2[i]);
    }
  }

  istr1.close();
  istr2.close();

  fs::remove(f1);
  fs::remove(f2);
  fs::remove("/tmp/foo2.blob");
}
#endif

TEST_F(BlobIOTest, Iterator) {

  Blob blob(blob_path);

  // First, create some entries for the blob
  TileHeader dummy_header0;
  dummy_header0.set_col(0);
  dummy_header0.set_row(953);
  dummy_header0.set_level(3);
  dummy_header0.set_transaction_id(1024);
  dummy_header0.set_filetype("foo");

  TileHeader dummy_header1;
  dummy_header1.set_col(33);
  dummy_header1.set_row(91);
  dummy_header1.set_level(321);
  dummy_header1.set_transaction_id(1034);
  dummy_header1.set_filetype("bar");

  TileHeader dummy_header2;
  dummy_header2.set_col(22);
  dummy_header2.set_row(1);
  dummy_header2.set_level(322);
  dummy_header2.set_transaction_id(1054);
  dummy_header2.set_filetype("baz");

  // Write some dummy data to the file with various index entries
  int64 offset = blob.write(dummy_header0, test_data, data_size);
  offset = blob.write(dummy_header1, test_data, data_size);
  offset = blob.write(dummy_header2, test_data, data_size);

  // Create an iterator
  Blob::iterator iter = blob.begin();

  TileHeader result = iter->hdr;
  EXPECT_EQ( dummy_header0.col(), result.col() );
  EXPECT_EQ( dummy_header0.row(), result.row() );
  EXPECT_EQ( dummy_header0.level(), result.level() );
  EXPECT_EQ( dummy_header0.transaction_id(), result.transaction_id() );
  EXPECT_EQ( dummy_header0.filetype(), result.filetype() );
  ASSERT_NE( iter, blob.end() );

  ++iter;
  result = iter->hdr;
  EXPECT_EQ( dummy_header1.col(), result.col() );
  EXPECT_EQ( dummy_header1.row(), result.row() );
  EXPECT_EQ( dummy_header1.level(), result.level() );
  EXPECT_EQ( dummy_header1.transaction_id(), result.transaction_id() );
  EXPECT_EQ( dummy_header1.filetype(), result.filetype() );
  ASSERT_NE( blob.end(), iter );

  ++iter;
  result = iter->hdr;
  EXPECT_EQ( dummy_header2.col(), result.col() );
  EXPECT_EQ( dummy_header2.row(), result.row() );
  EXPECT_EQ( dummy_header2.level(), result.level() );
  EXPECT_EQ( dummy_header2.transaction_id(), result.transaction_id() );
  EXPECT_EQ( dummy_header2.filetype(), result.filetype() );
  ASSERT_NE( blob.end(), iter );

  ++iter;
  EXPECT_EQ( blob.end(), iter );
}
