# 查询防抖与异步接缝（为何现在不上线程）

## 现象 / 背景

`IPlugin::query` 在搜索框 `textChanged` 时被调用。若逐键同步查询，慢插件
（如规划中的 FilePlugin 遍历文件系统）会阻塞 UI 线程，搜索框卡顿。

## 做法

- **防抖**：`MainWindow` 用单次 `QTimer`（`kQueryDebounceMs`，当前 100ms）。
  每次输入只重启定时器，停顿到期后调用 `runQuery()` 执行一次查询，避免逐键查询。
- **接缝**：真正的查询逻辑集中在 `MainWindow::runQuery()`。这是将来把耗时插件
  挪到工作线程的**唯一改动点**。

## 为何暂不做线程化异步

当前唯一插件 AppPlugin 是内存级即时查询，引入 QtConcurrent 线程池属于
**无消费者的投机基建**，且会带来：

1. 线程安全契约：`query` 需可在工作线程重入，而 C11 的 `QFileSystemWatcher`
   会在主线程重载 `m_apps`，二者并发将产生数据竞争；
2. 增量合并 / 失效代次（generation）丢弃旧结果的复杂度，无法在没有慢插件时验证。

因此线程化推迟到 **FilePlugin 落地时**一并实现：届时在 `runQuery()` 内对慢插件
改为 `QtConcurrent::run` + `QFutureWatcher` 回调，并用代次计数器丢弃过期结果，
同时为可重入的插件约定线程安全契约。

## 更新：异步路径已落地（FilePlugin）

本节规划的方案已实现，`@` 文件搜索的遍历已挪到工作线程。三件套：

- **`IPlugin::runsAsync()`**：插件声明 query 需异步（FilePlugin 返回 true）。
  MainWindow 在 `runQuery()` 里据此把该插件的 `query` 用 `QtConcurrent::run`
  派发到线程池，同步插件（App/Command）仍即时执行、先行展示。
- **失效代次**：`m_queryGen` 每次查询自增，`QFutureWatcher::finished`（主线程）
  回调先比对代次与 `isVisible()`，过期/已隐藏直接丢弃，连打字不会让旧结果乱入。
- **`IPlugin::decorate()`（主线程装饰）**：`QFileIconProvider::icon()` 内部建
  `QPixmap`，**不能在工作线程调用**。故 query 在工作线程只产出无图标结果，图标由
  MainWindow 在主线程、且**只对最终展示的 ≤kMaxItems 条**调 `decorate` 补齐。

`kMaxVisit` 上限仍保留，作为后台遍历的兜底，见
[FilePlugin 同步遍历的访问上限兜底](fileplugin-bounded-scan.md)。

## 正确做法（给后续实现者）

- 新增慢插件时，不要在 `query` 里直接做耗时 IO 并期望同步返回；先在 `runQuery()`
  建立异步路径，再让慢插件走该路径。
- 任何对插件内部状态的后台读取，都要与主线程的状态修改（如重载）互斥。
