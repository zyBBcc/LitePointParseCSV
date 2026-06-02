# CLAUDE.md

## 项目概述

`LitePointParseCSV` 是一个 Windows C++ 静态库，用于解析 LitePoint 无线测试设备生成的 CSV 测试报告。输出为 Visual Studio 2019 静态库（`.lib`），导出 C ABI（`__stdcall`），可被多种语言调用。

## 构建命令

```bash
# 使用 MSBuild 构建 (Release x64)
msbuild LitePointParseCSV/LitePointParseCSV.sln /p:Configuration=Release /p:Platform=x64

# 使用 MSBuild 构建 (Debug Win32)
msbuild LitePointParseCSV/LitePointParseCSV.sln /p:Configuration=Debug /p:Platform=Win32
```

输出路径：`LitePointParseCSV/bulid/<Configuration>/LitePointParseCSV.lib`

## 源码结构

| 文件 | 用途 |
|------|------|
| `LitePointParseCSV.h` | 公共 API 头文件 — 5 个导出函数 + `CalItem_C` 结构体 + `ErrorCallback` 类型 |
| `LitePointParseCSV.cpp` | 主实现 (~1012 行) — CSV 加载、测试段解析、线损/功率/平均计算 |
| `framework.h` | Windows `WIN32_LEAN_AND_MEAN` 预处理头 |
| `pch.h` / `pch.cpp` | 预编译头文件，包含 `framework.h` |
| `csv2/` | 第三方 header-only CSV 解析库（MIT 许可），基于内存映射文件 |

### 关键内部函数 (`LitePointParseCSV.cpp`)

| 函数 | 作用 |
|------|------|
| `ParseLitePointCsvInternal` | 主解析入口：遍历 CSV 行，识别段标识行、表头行、数据行，分发到对应段解析器 |
| `GetSectionRow` | 根据独立行内容识别当前测试段（单 cell 行匹配关键字） |
| `LoadWholeCsv` | 使用 csv2 Reader（内存映射）加载整个 CSV，返回 `vector<RowEx>` |
| `parseGeneral` | 解析 General 段（LOT INFO 元数据） |
| `parseXtalCal` | 解析 XTAL Calibration 段 |
| `parseTxCal` | 解析 TX Calibration 段 — 构造含 Band/Mode/Ant/BW/Freq/MCS 的段名；解析 EVM 对 |
| `parseRxCal` | 解析 RX Calibration 段 — 动态 LNA Gain + Flatness（高增益/旁路） |
| `parseTxPerf` | 解析 Tx Performance 段 — 功率误差、时钟误差、EVM、LO 泄漏、频谱掩码、平坦度、频偏 |
| `parseRxFullSweep` | 解析 Rx Full Sweep 段 — 功率电平、PER、RSSI |
| `parseRxRecieverDFS` | 解析 Rx Reciever DFS 段 — DUT 功率、RSSI |
| `extractPowerEvmLimitStrings` | 正则提取 EVM 列头中的功率/限值 (`[>=XdBm]...[<=YdB]`) |
| `extractLimitRange` | 正则提取 `(lo - hi)` 格式的范围字符串 |
| `CalculatePathLoss` | 逐频率线损计算（Tag - Measured） |
| `ExtractMeasPowerFromCsv` | 提取 Tx Performance 中的测量功率，按频率和天线端口组织 |
| `CalcAvgPowerFromSet` | 对多文件逐频率逐列取平均 |

### 测试段识别与解析流程

1. `LoadWholeCsv` 加载整个 CSV 为 `vector<RowEx>`
2. 循环遍历：遇到**独立行**（`cells.size() == 1`）→ 调用 `GetSectionRow` 识别段类型，触发上一段的数据解析，清空缓冲
3. 遇到表头行（首列为 `"Test"` 或 `"Serial Number"`）→ 暂存为当前表头
4. 遇到数据行 → 加入当前段数据缓冲
5. 循环结束后触发最后一段的解析

## 第三方库

- **csv2** (源码包含在 `csv2/` 中)：header-only CSV 读写库
  - `csv2::Reader` — 通过 `mmap()` 内存映射读取
  - `csv2::Writer` — 写入到 `std::ofstream`
  - 内部依赖 **mio** 库实现跨平台内存映射
  - 本项目设置 `first_row_is_header<false>`（自行管理表头）

## 编码约定

- **文件编码：所有含中文注释的源文件必须保存为 UTF-8 with BOM**，否则 MSVC 在代码页 936 下会报 C4819 警告。VS 中通过"文件 → 高级保存选项 → Unicode (UTF-8 with signature)"设置
- 语言标准：C++17（使用 `std::optional`、`std::string_view`、structured bindings）
- 命名：C++ 内部使用小驼峰（`parseTxCal`），导出 C 接口使用大驼峰（`ParseLitePointCsv`）
- 错误处理：通过 `ErrorCallback` 回调报告错误；无异常传播到 C 边界
- 字符串安全：使用 `strncpy_s` 复制到 C 结构体字符数组
- 内存管理：`ParseLitePointCsv` 分配 `new CalItem_C[]`，调用方必须调用 `FreeParseResult` 释放
- 正则表达式：`std::regex` 用于解析列头中的限值和关键字

## 注意事项

- 本项目是**静态库**，不是可执行文件 — 无法直接运行，需链接到宿主程序
- 仅支持 Windows 平台（使用 Windows API 检查文件存在、`INVALID_FILE_ATTRIBUTES` 等）
- csv2 Reader 使用 `mio::mmap_source`，依赖文件系统内存映射 — 文件必须存在且可读
- `CalItem_C` 中的字符串有固定缓冲区大小：`segment[256]`, `key[128]`, `status[16]`, `lower[32]`, `upper[32]`

## 测试

当前项目不含自动化测试。修改后验证方式：
1. 在 Visual Studio 中生成解决方案，确保无编译错误
2. 将输出的 `.lib` 链接到测试宿主程序，用实际 LitePoint CSV 文件验证解析结果
