# FilePlugin 同步遍历的访问上限兜底

## 现象 / 背景

`@` 文件搜索按内置方案**递归遍历用户目录**（桌面/文档/下载/图片）。`runQuery()`
目前仍在 **UI 线程同步执行**（见 [查询防抖与异步接缝](query-debounce-async-seam.md)），
若对一个很大的目录树做无界遍历，每次防抖查询都会卡住搜索框。

## 做法

`FilePlugin::query` 用 `QDirIterator(..., Subdirectories)` 遍历，并加两道硬上限：

- `kMaxVisit`（8000）——访问条目数封顶，超出立即 `return` 已有结果，避免大树卡死；
- `kMaxResults`（200）——命中数封顶（MainWindow 之后还会再截到 `kMaxItems`）。

搜索根来自 `QStandardPaths`（`DesktopLocation`/`DocumentsLocation`/`DownloadLocation`/
`PicturesLocation`），跨平台且自动去重。文件名用 `Matcher::score` 打分，与其它插件口径一致。

## 限制 / 后续

- `kMaxVisit` 是**广度兜底而非完整性保证**：超大目录下排在遍历后段的文件可能扫不到。
- 真正的解法是把慢插件挪到工作线程（QtConcurrent + 失效代次），这部分仍未做，
  按 [查询防抖与异步接缝](query-debounce-async-seam.md) 的规划在目录规模成为实际痛点时再上。
- 若要扩大/调整搜索范围，改 `FilePlugin::searchRoots()` 即可，勿在 `query` 里写死路径。
