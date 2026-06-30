# FilePlugin 遍历的访问上限兜底

## 现象 / 背景

`@` 文件搜索按内置方案**递归遍历用户目录**（桌面/文档/下载/图片）。遍历已挪到
工作线程异步执行（`FilePlugin::runsAsync()` 返回 true，见
[查询防抖与异步接缝](query-debounce-async-seam.md)），故不再阻塞 UI；但快速连打时
会派发多次后台遍历，若对一个很大的目录树做无界遍历，仍会让线程池堆积无谓工作。

## 做法

`FilePlugin::query` 用 `QDirIterator(..., Subdirectories)` 遍历，并加两道硬上限：

- `kMaxVisit`（8000）——访问条目数封顶，超出立即 `return` 已有结果，避免大树卡死；
- `kMaxResults`（200）——命中数封顶（MainWindow 之后还会再截到 `kMaxItems`）。

搜索根来自 `QStandardPaths`（`DesktopLocation`/`DocumentsLocation`/`DownloadLocation`/
`PicturesLocation`），跨平台且自动去重。文件名用 `Matcher::score` 打分，与其它插件口径一致。

## 限制 / 后续

- `kMaxVisit` 是**广度兜底而非完整性保证**：超大目录下排在遍历后段的文件可能扫不到。
- 遍历已异步（不阻塞 UI），但仍是**每次查询全量重扫**、无索引；若日后要支持更大范围或
  更快响应，可考虑后台建索引 + 增量更新，再让 `query` 查索引而非遍历磁盘。
- 若要扩大/调整搜索范围，改 `FilePlugin::searchRoots()` 即可，勿在 `query` 里写死路径。
- `query` 在工作线程执行：**不要**在其中创建 `QIcon`/`QPixmap`（仅限 GUI 线程），
  图标在主线程 `FilePlugin::decorate()` 里补——这是当前异步实现的关键约束。
