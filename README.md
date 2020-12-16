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

    This only is supported while database is a file such as sqlite3. But for leveldb, database is a directory.
