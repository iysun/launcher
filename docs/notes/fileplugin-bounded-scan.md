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
- 搜索范围现由 `AppSettings::fileSearchPaths()` 持久化、设置页（"文件搜索目录"区块）可
  编辑；默认值仍是四个标准目录（桌面/文档/下载/图片），在 `AppSettings` 构造函数里通过
  `defaultFileSearchPaths()` 计算。`FilePlugin::searchRoots()` 只在没有设置对象时
  （如未来单测直接构造 `FilePlugin`）作为兜底默认值使用。
- `fileSearchPaths()`/`setFileSearchPaths()` 内部加了 `QMutex`：这是 `AppSettings` 里
  唯一需要加锁的字段，因为它要跨"设置页保存（主线程）"与"文件搜索查询（`FilePlugin::query()`
  工作线程）"访问；其余设置字段（热键、自启动、插件启停、网页引擎顺序）只在主线程读写，无此需要。
- `kMaxVisit`/`kMaxResults` 是**所有搜索根共用的累计上限**，`query()` 按根目录列表顺序
  遍历，排在前面的根先消耗访问额度。目录范围可配置后，用户在设置页"添加"目录的先后顺序
  直接决定了扫描优先级（设置页目前只有添加/移除，没有排序功能），这点需要用户自行留意。
- 新增目录时只做 `QDir::cleanPath` 后的精确去重，不检查是否为已有目录的子目录——若用户
  同时添加了一个目录和它的子目录，子树会被重复遍历，白白消耗 `kMaxVisit` 额度。属于已知
  限制，不在本次范围内解决（不属于黑名单/排除规则的范畴）。
- `query` 在工作线程执行：**不要**在其中创建 `QIcon`/`QPixmap`（仅限 GUI 线程），
  图标在主线程 `FilePlugin::decorate()` 里补——这是当前异步实现的关键约束。
