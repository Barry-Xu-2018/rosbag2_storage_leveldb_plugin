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

#ifndef LEVELDB_EXCEPTION_HPP_
#define LEVELDB_EXCEPTION_HPP_

#include <stdexcept>
#include <string>

#include "visibility_control.hpp"

// ignore incorrect warning when deriving from standard library types
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4275)
#endif

namespace rosbag2_storage_plugins
{

class ROSBAG2_STORAGE_PLUGINS_PUBLIC LeveldbException : public std::runtime_error
{
public:
  explicit LeveldbException(const std::string & message)
  : runtime_error(message) {}
};

}  // namespace rosbag2_storage_plugins

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // LEVELDB_EXCEPTION_HPP_
