# delegate 行高必须与列表 kItemH 同步

**现象：** 改了 `ResultDelegate::sizeHint` 高度但忘了同步 `mainwindow.cpp` 的 `kItemH`，弹窗高度按旧值算，结果被裁切或留白。

**原因：** 弹窗用 `m_list->setFixedHeight(shown * kItemH)` 计算高度，与 delegate 的 `sizeHint` 是两处独立常量。

**正确做法：** 二者保持一致（当前均为 56）。
