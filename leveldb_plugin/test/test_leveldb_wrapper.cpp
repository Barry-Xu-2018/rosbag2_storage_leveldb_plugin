// Copyright 2020 Sony Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <thread>

#include "rcpputils/filesystem_helper.hpp"
#include "rcutils/filesystem.h"
#include "rcutils/logging_macros.h"

#include "leveldb_wrapper.hpp"

#include "rosbag2_test_common/temporary_directory_fixture.hpp"

using namespace ::testing; // NOLINT
using namespace rosbag2_test_common; // NOLINT

class LeveldbWrapperTestFixture : public TemporaryDirectoryFixture
{
public:
  LeveldbWrapperTestFixture()
  : TemporaryDirectoryFixture()
  {
    test_dir_ = (rcpputils::fs::path(temporary_dir_path_) / "ldb").string();
  }
  std::string test_dir_;
  rosbag2_storage_plugins::leveldb_open_options_t open_options_;
};

TEST_F(LeveldbWrapperTestFixture, test_construct_parameters) {
  // related path must be specifed for write mode
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb("", "/topic", "_topic", true, open_options_);
    EXPECT_THROW(
      ldb.init_ldb(),
      rosbag2_storage_plugins::LeveldbException
    );
  }

  // related path must be specifed for read mode
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb("", "/topic", "_topic", false, open_options_);
    EXPECT_THROW(
      ldb.init_ldb(),
      rosbag2_storage_plugins::LeveldbException
    );
  }

  // directory name must be specifed for write mode
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb(test_dir_, "/topic", "", true, open_options_);
    EXPECT_THROW(
      ldb.init_ldb(),
      rosbag2_storage_plugins::LeveldbException
    );
  }

  // directory name must be specifed for read mode
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb(test_dir_, "/topic", "", false, open_options_);
    EXPECT_THROW(
      ldb.init_ldb(),
      rosbag2_storage_plugins::LeveldbException
    );
  }

  // topic name must be specifed for write mode
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb(test_dir_, "", "_topic", true, open_options_);
    EXPECT_THROW(
      ldb.init_ldb(),
      rosbag2_storage_plugins::LeveldbException
    );
  }

  // topic name should be ignored for read mode
  {
    rosbag2_storage::TopicMetadata in = {"/topic", "string", "cdr", "abcd"};
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic", "_topic", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    ASSERT_NO_THROW(ldb_w.write_metadata(in));
  }
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb(test_dir_, "", "_topic", false, open_options_);
    EXPECT_NO_THROW(
      ldb.init_ldb()
    );
  }
}

TEST_F(LeveldbWrapperTestFixture, check_write_and_read_metadata) {
  rosbag2_storage::TopicMetadata in = {"/topic1", "string", "cdr", "abcd"};

  // Write metadata
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic1", "_topic1", true, open_options_);

    ASSERT_NO_THROW(ldb_w.init_ldb());

    EXPECT_NO_THROW(ldb_w.write_metadata(in));
  }

  // Read metadata
  std::shared_ptr<rosbag2_storage::TopicMetadata> out;
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic1", false, open_options_);

    ASSERT_NO_THROW(ldb_r.init_ldb());

    EXPECT_NO_THROW(out = ldb_r.read_all_metadata());
  }

  EXPECT_EQ(*out, in);
}

TEST_F(LeveldbWrapperTestFixture, check_get_topic_name) {
  // Open with write mode
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic2", "_topic2", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    EXPECT_EQ("/topic2", ldb_w.get_topic_name());
    rosbag2_storage::TopicMetadata in = {"/topic2", "string", "cdr", "abcd"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));
  }

  // open with read mode
  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic2", false, open_options_);
    ASSERT_NO_THROW(ldb_r.init_ldb());
    EXPECT_EQ("/topic2", ldb_r.get_topic_name());
  }
}


std::shared_ptr<rosbag2_storage::SerializedBagMessage> make_fake_serialized_message(
  std::string msg,
  rcutils_time_point_value_t ts,
  std::string topic)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  int msg_size = msg.length();

  rcutils_uint8_array_t * message = new rcutils_uint8_array_t;
  if (message == nullptr) {
    throw std::runtime_error("Failed to allocate rcutils_uint8_array_t !");
  }

  *message = rcutils_get_zero_initialized_uint8_array();

  if (rcutils_uint8_array_init(message, msg_size, &allocator) != RCUTILS_RET_OK) {
    throw std::runtime_error("Failed to call rcutils_uint8_array_init !");
  }

  std::shared_ptr<rcutils_uint8_array_t> fake_serialized_data(
    message,
    [](rcutils_uint8_array_t * msg) {
      int error = rcutils_uint8_array_fini(msg);
      delete msg;
      if (error != RCUTILS_RET_OK) {
        RCUTILS_LOG_ERROR_NAMED(
          "test_leveldb_wrapper", "Failed to call rcutils_uint8_array_fini !");
      }
    });

  fake_serialized_data->buffer_length = msg_size;
  memcpy(fake_serialized_data->buffer, msg.data(), msg_size);

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> serialized_bag_msg =
    std::make_shared<rosbag2_storage::SerializedBagMessage>();

  serialized_bag_msg->serialized_data = fake_serialized_data;
  serialized_bag_msg->time_stamp = ts;
  serialized_bag_msg->topic_name = topic;

  return serialized_bag_msg;
}

TEST_F(LeveldbWrapperTestFixture, check_get_topic_ldb_size) {
  uint64_t expect_size = 0;
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  {
    // Prepare metadata and message
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic3", "_topic3", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    rosbag2_storage::TopicMetadata in = {"/topic3", "string", "cdr", "abcdefg"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg =
      make_fake_serialized_message("1234567890abcdef", 1e9, "/topic3");

    ASSERT_NO_THROW(ldb_w.write_message(msg));

    // Get expected size
    ASSERT_EQ(
      rcutils_calculate_directory_size_with_recursion(
        test_dir_.c_str(),
        3,
        &expect_size,
        allocator),
      RCUTILS_RET_OK);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);

    // Compare size
    EXPECT_EQ(ldb_w.get_topic_ldb_size(), expect_size);
  }

  {
    // Re-open with read mode to check
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic3", false, open_options_);
    ASSERT_NO_THROW(ldb_r.init_ldb());

    ASSERT_EQ(
      rcutils_calculate_directory_size_with_recursion(
        test_dir_.c_str(),
        3,
        &expect_size,
        allocator),
      RCUTILS_RET_OK);

    // Compare size
    EXPECT_EQ(ldb_r.get_topic_ldb_size(), expect_size);
  }
}

TEST_F(LeveldbWrapperTestFixture, check_read_write_message) {
  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg1 =
    make_fake_serialized_message("1234567890abcdef", 1, "/topic4");

  {
    // Prepare metadata and message
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic4", "_topic4", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    rosbag2_storage::TopicMetadata in = {"/topic4", "string", "cdr", "abcdefg"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));

    EXPECT_NO_THROW(ldb_w.write_message(msg1));
  }

  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic4", false, open_options_);
    ASSERT_NO_THROW(ldb_r.init_ldb());

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_msg1, read_msg2;
    EXPECT_TRUE(ldb_r.has_next());
    EXPECT_NO_THROW(read_msg1 = ldb_r.read_next());

    EXPECT_EQ(msg1->time_stamp, read_msg1->time_stamp);
    EXPECT_EQ(msg1->topic_name, read_msg1->topic_name);
    EXPECT_EQ(
      memcmp(
        msg1->serialized_data->buffer,
        read_msg1->serialized_data->buffer,
        msg1->serialized_data->buffer_length),
      0);
  }
}


TEST_F(LeveldbWrapperTestFixture, check_read_message_by_timestamp_sequence) {
  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg1 =
    make_fake_serialized_message("1234567890abcdef", 3, "/topic5");

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg2 =
    make_fake_serialized_message("11223344556677889900", 1, "/topic5");

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg3 =
    make_fake_serialized_message("aabbccddeeff", 2, "/topic5");

  {
    // Prepare metadata and message
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic5", "_topic5", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    rosbag2_storage::TopicMetadata in = {"/topic5", "string", "cdr", "abcdefg"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));

    ASSERT_NO_THROW(ldb_w.write_message(msg1));
    ASSERT_NO_THROW(ldb_w.write_message(msg2));
    ASSERT_NO_THROW(ldb_w.write_message(msg3));
  }

  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic5", false, open_options_);
    ASSERT_NO_THROW(ldb_r.init_ldb());

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_msg1, read_msg2, read_msg3;
    ASSERT_TRUE(ldb_r.has_next());
    ASSERT_NO_THROW(read_msg1 = ldb_r.read_next());
    ASSERT_TRUE(ldb_r.has_next());
    ASSERT_NO_THROW(read_msg2 = ldb_r.read_next());
    ASSERT_TRUE(ldb_r.has_next());
    ASSERT_NO_THROW(read_msg3 = ldb_r.read_next());

    // Check if read message is ordered by timestamp
    EXPECT_EQ(read_msg1->time_stamp, 1);
    EXPECT_EQ(read_msg2->time_stamp, 2);
    EXPECT_EQ(read_msg3->time_stamp, 3);
  }
}

TEST_F(LeveldbWrapperTestFixture, check_has_next) {
  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg1 =
    make_fake_serialized_message("1234567890abcdef", 1, "/topic6");

  {
    // Prepare metadata and message
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic6", "_topic6", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    rosbag2_storage::TopicMetadata in = {"/topic6", "string", "cdr", "abcdefg"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));

    ASSERT_NO_THROW(ldb_w.write_message(msg1));
  }

  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic6", false, open_options_);
    ASSERT_NO_THROW(ldb_r.init_ldb());

    std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_msg1;
    EXPECT_TRUE(ldb_r.has_next());
    ASSERT_NO_THROW(read_msg1 = ldb_r.read_next());
    EXPECT_FALSE(ldb_r.has_next());
  }
}

TEST_F(LeveldbWrapperTestFixture, check_remove_database) {
  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg1 =
    make_fake_serialized_message("1234567890abcdef", 1, "/topic7");

  {
    // Prepare metadata and message
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic7", "_topic7", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());

    rosbag2_storage::TopicMetadata in = {"/topic7", "string", "cdr", "abcdefg"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));

    ASSERT_NO_THROW(ldb_w.write_message(msg1));

    ldb_w.remove_database();
  }

  // Check if database has been removed
  uint64_t dir_size = 0;
  rcutils_allocator_t allocator = rcutils_get_default_allocator();

  ASSERT_EQ(
    rcutils_calculate_directory_size_with_recursion((test_dir_).c_str(), 3, &dir_size, allocator),
    RCUTILS_RET_OK);

  EXPECT_EQ(dir_size, static_cast<uint64_t>(0));
}

TEST_F(LeveldbWrapperTestFixture, check_get_message_count) {
  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg1 =
    make_fake_serialized_message("1234567890abcdef", 1, "/topic7");

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg2 =
    make_fake_serialized_message("11223344556677889900", 2, "/topic7");

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg3 =
    make_fake_serialized_message("aabbccddeeff", 3, "/topic7");

  {
    // Prepare metadata and message
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic7", "_topic7", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    rosbag2_storage::TopicMetadata in = {"/topic7", "string", "cdr", "abcdefg"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));

    ASSERT_NO_THROW(ldb_w.write_message(msg1));
    ASSERT_NO_THROW(ldb_w.write_message(msg2));
    ASSERT_NO_THROW(ldb_w.write_message(msg3));

    EXPECT_EQ(ldb_w.get_message_count(), static_cast<size_t>(3));
  }

  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic7", false, open_options_);
    ASSERT_NO_THROW(ldb_r.init_ldb());

    EXPECT_EQ(ldb_r.get_message_count(), static_cast<size_t>(3));
  }
}

TEST_F(LeveldbWrapperTestFixture, check_get_min_max_timestamp) {
  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg1 =
    make_fake_serialized_message("1234567890abcdef", 3, "/topic8");

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg2 =
    make_fake_serialized_message("11223344556677889900", 1, "/topic8");

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg3 =
    make_fake_serialized_message("aabbccddeeff", 4, "/topic8");

  std::shared_ptr<rosbag2_storage::SerializedBagMessage> msg4 =
    make_fake_serialized_message("998877665544332211", 2, "/topic8");

  {
    // Prepare metadata and message
    rosbag2_storage_plugins::LeveldbWrapper ldb_w(
      test_dir_, "/topic8", "_topic8", true, open_options_);
    ASSERT_NO_THROW(ldb_w.init_ldb());
    rosbag2_storage::TopicMetadata in = {"/topic8", "string", "cdr", "abcdefg"};
    ASSERT_NO_THROW(ldb_w.write_metadata(in));

    ASSERT_NO_THROW(ldb_w.write_message(msg1));
    ASSERT_NO_THROW(ldb_w.write_message(msg2));
    ASSERT_NO_THROW(ldb_w.write_message(msg3));
    ASSERT_NO_THROW(ldb_w.write_message(msg4));
  }

  {
    rosbag2_storage_plugins::LeveldbWrapper ldb_r(test_dir_, "", "_topic8", false, open_options_);
    ASSERT_NO_THROW(ldb_r.init_ldb());

    EXPECT_EQ(ldb_r.get_min_timestamp(), 1);

    EXPECT_EQ(ldb_r.get_max_timestamp(), 4);
  }
}
