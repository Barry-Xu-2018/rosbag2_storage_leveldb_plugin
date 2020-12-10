// Copyright 2018, Bosch Software Innovations GmbH.
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
//
// 2020/12/9 Modify code for testing leveldb plugin

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcpputils/filesystem_helper.hpp"

#include "rcutils/snprintf.h"

#include "rosbag2_storage/storage_filter.hpp"

#include "leveldb_plugin_test_fixture.hpp"

using namespace ::testing;  // NOLINT

namespace rosbag2_storage
{

bool operator!=(const TopicMetadata & lhs, const TopicMetadata & rhs)
{
  return !(lhs == rhs);
}

bool operator==(const TopicInformation & lhs, const TopicInformation & rhs)
{
  return lhs.topic_metadata == rhs.topic_metadata &&
         lhs.message_count == rhs.message_count;
}

bool operator!=(const TopicInformation & lhs, const TopicInformation & rhs)
{
  return !(lhs == rhs);
}

}  // namespace rosbag2_storage

constexpr static const char * const plugin_id = "leveldb";

TEST_F(LeveldbPluginTestFixture, string_messages_are_written_and_read_to_and_from_leveldb_storage) {
  std::vector<std::string> string_messages = {"first message", "second message", "third message"};
  std::vector<std::string> topics = {"topic1", "topic2", "topic3"};
  std::vector<std::string> rmw_formats = {"rmw1", "rmw2", "rmw3"};

  // message, timestamp, topic_name, type, serialization_format, offered_qos_profiles
  std::vector<std::tuple<std::string, int64_t, std::string, std::string, std::string>> messages =
  {std::make_tuple(string_messages[0], 1, topics[0], "type1", rmw_formats[0]),
    std::make_tuple(string_messages[1], 2, topics[1], "type2", rmw_formats[1]),
    std::make_tuple(string_messages[2], 3, topics[2], "type3", rmw_formats[2])};

  write_messages_to_leveldb(messages);
  auto read_messages = read_all_messages_from_sqlite();

  ASSERT_THAT(read_messages, SizeIs(3));
  for (size_t i = 0; i < 3; i++) {
    EXPECT_THAT(deserialize_message(read_messages[i]->serialized_data), Eq(string_messages[i]));
    EXPECT_THAT(read_messages[i]->time_stamp, Eq(std::get<1>(messages[i])));
    EXPECT_THAT(read_messages[i]->topic_name, Eq(topics[i]));
  }
}

TEST_F(LeveldbPluginTestFixture, has_next_return_false_if_there_are_no_more_messages) {
  std::vector<std::tuple<std::string, int64_t, std::string, std::string, std::string>>
  string_messages =
  {std::make_tuple("first message", 1, "/topic", "", ""),
    std::make_tuple("second message", 2, "/topic", "", "")};

  write_messages_to_leveldb(string_messages);
  std::unique_ptr<rosbag2_storage::storage_interfaces::ReadOnlyInterface> readable_storage =
    std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();

  auto db_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();
  readable_storage->open({db_filename, plugin_id});

  EXPECT_TRUE(readable_storage->has_next());
  readable_storage->read_next();
  EXPECT_TRUE(readable_storage->has_next());
  readable_storage->read_next();
  EXPECT_FALSE(readable_storage->has_next());
}

TEST_F(LeveldbPluginTestFixture, get_next_returns_messages_in_timestamp_order) {
  std::vector<std::tuple<std::string, int64_t, std::string, std::string, std::string>>
  string_messages =
  {std::make_tuple("first message", 2, "/topic", "", ""),
    std::make_tuple("second message", 6, "/topic", "", "")};

  write_messages_to_leveldb(string_messages);
  std::unique_ptr<rosbag2_storage::storage_interfaces::ReadOnlyInterface> readable_storage =
    std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();

  auto db_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();
  readable_storage->open({db_filename, plugin_id});

  EXPECT_TRUE(readable_storage->has_next());
  auto first_message = readable_storage->read_next();
  EXPECT_THAT(first_message->time_stamp, Eq(2));
  EXPECT_TRUE(readable_storage->has_next());
  auto second_message = readable_storage->read_next();
  EXPECT_THAT(second_message->time_stamp, Eq(6));
  EXPECT_FALSE(readable_storage->has_next());
}

TEST_F(LeveldbPluginTestFixture, read_next_returns_filtered_messages) {
  std::vector<std::tuple<std::string, int64_t, std::string, std::string, std::string>>
  string_messages =
  {std::make_tuple("topic1 message", 1, "topic1", "", ""),
    std::make_tuple("topic2 message", 2, "topic2", "", ""),
    std::make_tuple("topic3 message", 3, "topic3", "", "")};

  write_messages_to_leveldb(string_messages);

  auto db_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();
  rosbag2_storage::StorageFilter storage_filter;

  std::unique_ptr<rosbag2_storage::storage_interfaces::ReadOnlyInterface> readable_storage =
    std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();
  readable_storage->open({db_filename, plugin_id});
  storage_filter.topics.push_back("topic2");
  storage_filter.topics.push_back("topic3");
  readable_storage->set_filter(storage_filter);
  EXPECT_TRUE(readable_storage->has_next());
  auto first_message = readable_storage->read_next();
  EXPECT_THAT(first_message->topic_name, Eq("topic2"));
  EXPECT_TRUE(readable_storage->has_next());
  auto second_message = readable_storage->read_next();
  EXPECT_THAT(second_message->topic_name, Eq("topic3"));
  EXPECT_FALSE(readable_storage->has_next());

  readable_storage.reset();

  // Test reset filter
  std::unique_ptr<rosbag2_storage::storage_interfaces::ReadOnlyInterface> readable_storage2 =
    std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();

  readable_storage2->open({db_filename, plugin_id});
  readable_storage2->set_filter(storage_filter);
  readable_storage2->reset_filter();

  EXPECT_TRUE(readable_storage2->has_next());
  auto third_message = readable_storage2->read_next();
  EXPECT_THAT(third_message->topic_name, Eq("topic1"));
  EXPECT_TRUE(readable_storage2->has_next());
  auto fourth_message = readable_storage2->read_next();
  EXPECT_THAT(fourth_message->topic_name, Eq("topic2"));
  EXPECT_TRUE(readable_storage2->has_next());
  auto fifth_message = readable_storage2->read_next();
  EXPECT_THAT(fifth_message->topic_name, Eq("topic3"));
  EXPECT_FALSE(readable_storage2->has_next());
}

TEST_F(LeveldbPluginTestFixture, get_all_topics_and_types_returns_the_correct_vector) {
  std::unique_ptr<rosbag2_storage::storage_interfaces::ReadWriteInterface> writable_storage =
    std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();

  // extension is omitted since storage is being created; io_flag = READ_WRITE
  const auto read_write_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();

  writable_storage->open({read_write_filename, plugin_id});
  writable_storage->create_topic({"topic1", "type1", "rmw1", ""});
  writable_storage->create_topic({"topic2", "type2", "rmw2", ""});

  const auto read_only_filename = writable_storage->get_relative_file_path();

  writable_storage.reset();

  auto readable_storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();
  readable_storage->open(
    {read_only_filename, plugin_id},
    rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
  auto topics_and_types = readable_storage->get_all_topics_and_types();

  EXPECT_THAT(
    topics_and_types, ElementsAreArray(
  {
    rosbag2_storage::TopicMetadata{"topic1", "type1", "rmw1", ""},
    rosbag2_storage::TopicMetadata{"topic2", "type2", "rmw2", ""}
  }));
}

TEST_F(LeveldbPluginTestFixture, get_metadata_returns_correct_struct) {
  std::vector<std::string> string_messages = {"first message", "second message", "third message"};
  std::vector<std::string> topics = {"topic1", "topic2"};
  std::vector<std::tuple<std::string, int64_t, std::string, std::string, std::string>> messages =
  {std::make_tuple(
      string_messages[0], static_cast<int64_t>(1e9), topics[0], "type1", "rmw_format"),
    std::make_tuple(
      string_messages[1], static_cast<int64_t>(2e9), topics[0], "type1", "rmw_format"),
    std::make_tuple(
      string_messages[2], static_cast<int64_t>(3e9), topics[1], "type2", "rmw_format")};

  write_messages_to_leveldb(messages);

  const auto readable_storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();
  const auto db_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();

  readable_storage->open(
    {db_filename, plugin_id},
    rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
  const auto metadata = readable_storage->get_metadata();

  EXPECT_THAT(metadata.storage_identifier, Eq("leveldb"));
  EXPECT_THAT(metadata.relative_file_paths, ElementsAreArray({db_filename}));
  EXPECT_THAT(
    metadata.topics_with_message_count, ElementsAreArray(
  {
    rosbag2_storage::TopicInformation{rosbag2_storage::TopicMetadata{
        "topic1", "type1", "rmw_format", ""}, 2u},
    rosbag2_storage::TopicInformation{rosbag2_storage::TopicMetadata{
        "topic2", "type2", "rmw_format", ""}, 1u}
  }));
  EXPECT_THAT(metadata.message_count, Eq(3u));
  EXPECT_THAT(
    metadata.starting_time, Eq(
      std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::seconds(1))
  ));
  EXPECT_THAT(metadata.duration, Eq(std::chrono::seconds(2)));
}

TEST_F(LeveldbPluginTestFixture, get_metadata_returns_correct_struct_if_no_messages) {
  write_messages_to_leveldb({});

  const auto readable_storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();
  const auto db_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();

  readable_storage->open(
    {db_filename, plugin_id},
    rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
  const auto metadata = readable_storage->get_metadata();

  EXPECT_THAT(metadata.storage_identifier, Eq("leveldb"));
  EXPECT_THAT(metadata.relative_file_paths, ElementsAreArray({db_filename}));
  EXPECT_THAT(metadata.topics_with_message_count, IsEmpty());
  EXPECT_THAT(metadata.message_count, Eq(0u));
  EXPECT_THAT(
    metadata.starting_time, Eq(
      std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::seconds(0))
  ));
  EXPECT_THAT(metadata.duration, Eq(std::chrono::seconds(0)));
}

TEST_F(LeveldbPluginTestFixture, remove_topics_and_types_returns_the_empty_vector) {
  std::unique_ptr<rosbag2_storage::storage_interfaces::ReadWriteInterface> writable_storage =
    std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();

  // extension is omitted since storage is created; io_flag = READ_WRITE
  const auto read_write_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();

  writable_storage->open({read_write_filename, plugin_id});
  writable_storage->create_topic({"topic1", "type1", "rmw1", ""});
  writable_storage->remove_topic({"topic1", "type1", "rmw1", ""});

  const auto read_only_filename = writable_storage->get_relative_file_path();

  writable_storage.reset();

  // Remove topics
  auto readable_storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();

  readable_storage->open(
    {read_only_filename, plugin_id},
    rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
  auto topics_and_types = readable_storage->get_all_topics_and_types();

  EXPECT_THAT(topics_and_types, IsEmpty());
}

TEST_F(LeveldbPluginTestFixture, get_storage_identifier_returns_leveldb) {
  const auto storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();

  EXPECT_EQ(storage->get_storage_identifier(), "leveldb");
}

TEST_F(LeveldbPluginTestFixture, get_relative_file_path_returns_db_name_with_ext) {
  // check that storage::get_relative_file_path returns the relative path to the sqlite3 db
  // and that uri is handled properly when storage::open is called with different io_flags
  // READ_WRITE expects uri to not end in extension
  const auto read_write_filename = (rcpputils::fs::path(temporary_dir_path_) / "rosbag").string();
  const auto storage_path = read_write_filename;
  const auto read_write_storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();
  read_write_storage->open(
    {read_write_filename, plugin_id},
    rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE);
  EXPECT_EQ(read_write_storage->get_relative_file_path(), storage_path);

  // READ_ONLY expects uri to be the relative file path to the sqlite3 db.
  const auto & read_only_filename = storage_path;
  const auto read_only_storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();
  read_only_storage->open(
    {read_only_filename, plugin_id},
    rosbag2_storage::storage_interfaces::IOFlag::READ_ONLY);
  EXPECT_EQ(read_only_storage->get_relative_file_path(), storage_path);

  const auto & append_filename = storage_path;
  const auto append_storage = std::make_unique<rosbag2_storage_plugins::LeveldbStorage>();
  append_storage->open(
    {append_filename, plugin_id},
    rosbag2_storage::storage_interfaces::IOFlag::APPEND);
  EXPECT_EQ(append_storage->get_relative_file_path(), storage_path);
}

#if 0
TEST_F(LeveldbPluginTestFixture, loads_config_file) {
  // Check that storage opens with correct sqlite config file
  const auto valid_yaml = "write:\n  pragmas: [\"journal_mode = MEMORY\"]\n";
  const auto writable_storage = std::make_unique<rosbag2_storage_plugins::SqliteStorage>();
  EXPECT_NO_THROW(
    writable_storage->open(
      make_storage_options_with_config(valid_yaml, plugin_id),
      rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE));
}

TEST_F(LeveldbPluginTestFixture, throws_on_invalid_pragma_in_config_file) {
  // Check that storage throws on invalid pragma statement in sqlite config
  const auto invalid_yaml = "write:\n  pragmas: [\"unrecognized_pragma_name = 2\"]\n";
  const auto writable_storage = std::make_unique<rosbag2_storage_plugins::SqliteStorage>();

  EXPECT_THROW(
    writable_storage->open(
      make_storage_options_with_config(invalid_yaml, plugin_id),
      rosbag2_storage::storage_interfaces::IOFlag::READ_WRITE),
    std::runtime_error);
}
#endif