# Overview

This repository provides LevelDB storage plugin for rosbag2.

# How to Build

Precondition: Please setup ROS2 environment by `source` command.
```shell
$ mkdir -p leveldb_ws/src && cd leveldb
$ git -C src clone https://github.com/Barry-Xu-2018/rosbag2_storage_leveldb_plugin.git
$ colcon build --symlink-install --merge-install --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
```

# How to use

e.g.

```shell
$ source /PATH/TO/leveldb_ws/install/setup.bash
$ ros2 bag record -s leveldb -o /tmp/leveldb_bag /topic1
$ ros2 bag info -s leveldb /tmp/leveldb_bag
$ ros2 bag play -s leveldb /tmp/leveldb_bag
```

# Notice

1. Not suggest to use '-d' (maximum size in bytes before the bagfile will be split) while execute recording command.

    leveldb automatically split database files. This has avoid the big size for bagfile.

2. Not support `-compression-mode file` while executing recording command.

    This parameter is only supported while database is a file such as sqlite3. But for leveldb, database is a directory.

3. The format of Yaml configuration file for `--storage-config-file STORAGE_CONFIG_FILE`

    e.g.
		```
        open_options:
            write_buffer_size: 4194304
            max_open_files: 1000
            block_size: 4096
            max_file_size: 2097152
		```

    These parameters are used to open options while opening leveldb.

    | Parameter name | Description |
    | :-- | :-- |
    | `write_buffer_size` | Amount of data to build up in memory (backed by an unsorted log on disk) before converting to a sorted on-disk file. |
    | `max_open_files` | Number of open files that can be used by the DB. |
    | `block_size` | Approximate size of user data packed per block. |
    | `max_file_size` | Leveldb will write up to this amount of bytes to a file before switching to a new one. |

    According to your requirement, you can set one or more above. `0` means use default value.

    Above parameters are corresponding to open option of leveldb. For the detail, please refer to [leveldb/options.h](https://github.com/google/leveldb/blob/v1.20/include/leveldb/options.h)
