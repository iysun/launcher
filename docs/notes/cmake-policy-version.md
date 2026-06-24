# CMake 4.x 需要传 `CMAKE_POLICY_VERSION_MINIMUM`

**现象：** CMake 4.x 配置时报错，提示 QHotkey 子目录的 `cmake_minimum_required` 版本过低。

**原因：** CMake 4.0 移除了对 `cmake_minimum_required(VERSION < 3.5)` 的向后兼容支持，QHotkey 的 CMakeLists.txt 未及时更新。

**正确做法：** 配置时追加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`。
