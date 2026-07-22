# 数据库引论 (Introduction to Database Systems) - Project II: MyJQL

本仓库为复旦大学《数据库引论》荣誉课课程的 Project II 实验实现。项目从底层物理存储、缓冲池管理、块文件管理到 B-树索引进行分层设计与构建，最终实现了一个基于类 Redis 接口的嵌入式变长键值（Key-Value）数据库系统——**MyJQL**。

## 🛠️ 核心工具与语言
- **开发语言**：C 语言
- **开发与调试工具**：Linux, GCC, GDB, GNU Make

## 📂 数据库底层架构与模块设计

MyJQL 支持变长字符串键值的 `get`、`set`、`del` 基本操作，其底层采用模块化分层设计：

- **缓冲池与文件 IO (`file_io.h`, `buffer_pool.h`)**
  - **核心内容**：基于固定页大小（`PAGE_SIZE = 128`）实现磁盘与内存之间的数据交互。
  - **关键点**：设计并实现 `BufferPool`，支持按地址 `get_page` 锁定与 `release` 换出，为上层提供统一且高效的内存页缓存与写回机制。

- **块结构与记录管理 (`block.h`, `table.h`)**
  - **核心内容**：实现物理块（Block）内部的定长与变长 Item 管理，以及基于记录 ID（`RID: (block_addr, idx)`）的数据表读写。
  - **关键点**：使用块头部 `head_ptr` 和 `tail_ptr` 管理可用空间，ItemID 作为索引映射尾部变长 Item，支持变长键值对（KVP）与数据表的插入、读取与删除。

- **空闲空间映射表 (`hash_map.h`)**
  - **核心内容**：在物理存储（`*.fsm` 文件）中维护数据块的空闲空间信息。
  - **关键点**：通过控制块（HashMapControlBlock）、目录块（DirectoryBlock）与数据块链表组织的哈希表，实现对符合指定空闲大小的物理块的快速检索（`hash_table_pop_lower_bound`）与插入/更新。

- **变长长字符串存储 (`str.h`)**
  - **核心内容**：突破单页大小限制，实现超长变长字符串的分块存储。
  - **关键点**：将长字符串切分成 Chunk 并通过链表形式跨页保存，结合 `StringRecord` 提供流式字符读取（`has_next_char` / `next_char`）与管理能力。

- **B-树磁盘索引机制 (`b_tree.h`)**
  - **核心内容**：在 `*.idx` 文件中持久化组织 B-树索引，大幅提升 Key 的检索效率。
  - **关键点**：实现磁盘 B-树节点的分裂、合并与查找（`b_tree_search`, `b_tree_insert`, `b_tree_delete`），支持结合自定义比较函数进行高效键值检索。

- **KV 数据库顶层封装 (`myjql.h`)**
  - **核心内容**：集成底层 Table、String 与 B-Tree 索引模块，向上提供简洁的键值存储 API。
  - **关键点**：实现 `myjql_get`、`myjql_set` 和 `myjql_del` 的完整数据流转与持久化落盘策略。