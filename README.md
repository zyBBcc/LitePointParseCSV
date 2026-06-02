# LitePointParseCSV

一个 Windows C++ 静态库，用于解析 **LitePoint RF 测试设备** 生成的 CSV 测试报告，并提供线损计算、功率提取和多文件平均等辅助工具。

## 核心功能

### 1. CSV 测试报告解析 (`ParseLitePointCsv`)

解析 LitePoint 无线测试平台导出的多段式 CSV 报告，将异构数据归一化为统一的 `CalItem` 数组。支持以下测试段：

| 测试段 | 关键字 | 解析内容 |
|--------|--------|----------|
| General | `General` | LOT INFO 元数据（IQDVT 版本、CV 版本、DUT 版本） |
| XTAL Calibration | `XTAL Calibration` | 晶振校准：OffsetWord、频偏(PPM)、温度 |
| TX Calibration | `TX Calibration` | 发射校准：时钟误差、功率偏移、测量功率、EVM 对、S2D 增益/偏移/温度 |
| RX Calibration | `RX Calibration` | 接收校准：LNA 增益、平坦度（高增益/旁路模式）、温度 |
| Tx Performance | `Tx Performance` | 发射性能：功率误差、时钟误差、EVM、LO 泄漏、频谱掩码余量、平坦度余量、频偏 |
| Rx Full Sweep | `Rx Full Sweep` | 接收全扫：功率电平、PER（误包率）、RSSI、温度 |
| Rx Reciever DFS | `Rx Reciever DFS` | 接收 DFS：DUT 发射功率、RSSI、温度 |

每条解析结果 (`CalItem`) 包含：
- **segment** — 层次化标识符（如 `TX_CAL_2G_11N_ANT0_20BW^2412FREQ^MCS7`）
- **key** — 测量项名称（如 `ClockError`、`MeasurePower`、`EVM_-25dB`）
- **status** — PASS / FAIL
- **value** — 测量值
- **lower / upper** — 规格上下限

### 2. 线损计算 (`CalculatePathLoss`)

根据标准功率（Tag）文件和实测功率文件，逐频率计算路径损耗，输出损耗 CSV。

```
Loss = Tag_Power - Measured_Power
```

### 3. 测量功率提取 (`ExtractMeasPowerFromCsv`)

从完整测试 CSV 中提取 Tx Performance 段的各频率、各天线端口功率，输出简化的 5 端口 CSV 格式。

### 4. 多文件平均 (`CalcAvgPowerFromSet`)

对多个 CSV 文件按频率、按列计算平均值，输出平均功率 CSV。

## API 概览

所有接口以 C ABI（`__stdcall`）导出，可被 C / C++ / C# / Python 等语言调用。

```c
// 解析 LitePoint 测试 CSV
int __stdcall ParseLitePointCsv(
    const char* filePath,
    CalItem_C** items,
    int* itemCount,
    ErrorCallback onError,
    void* userData
);

// 释放解析结果
void __stdcall FreeParseResult(CalItem_C* items);

// 计算路径损耗
int __stdcall CalculatePathLoss(
    const char* powerTagCsv,
    const char* measPowerCsv,
    const char* outLossCsv,
    ErrorCallback onError,
    void* userData
);

// 提取测量功率
int __stdcall ExtractMeasPowerFromCsv(
    const char* testCsvPath,
    const char* outCsvPath,
    ErrorCallback onError,
    void* userData
);

// 多文件平均
int __stdcall CalcAvgPowerFromSet(
    const char* const* filePathSet,  // nullptr 结尾的路径数组
    const char* outAvgCsvPath,
    ErrorCallback onError,
    void* userData
);
```

所有函数通过 `ErrorCallback` 回调报告错误，格式为 `void(int lineNo, const char* msg, void* userData)`。

## 构建

- **开发环境**: Visual Studio 2019 (v142 工具集)
- **目标平台**: Windows (Win32 / x64)
- **配置**: Debug / Release
- **输出**: 静态库 `LitePointParseCSV.lib`

打开 `LitePointParseCSV/LitePointParseCSV.sln` 并生成解决方案即可。

## 第三方依赖

- [csv2](https://github.com/p-ranav/csv2) — header-only、内存映射的 CSV 解析库（MIT 许可），已包含在项目中

无其他外部依赖，仅需 C++17 标准库和 Windows SDK。

## 项目结构

```
LitePointParseCSV/
├── README.md
├── CLAUDE.md
└── LitePointParseCSV/
    ├── LitePointParseCSV.sln
    └── LitePointParseCSV/
        ├── LitePointParseCSV.vcxproj
        ├── LitePointParseCSV.h          # 公共 API 头文件
        ├── LitePointParseCSV.cpp        # 主实现 (约 1012 行)
        ├── framework.h                  # Windows 预定义头
        ├── pch.h / pch.cpp              # 预编译头
        └── csv2/                        # 第三方 CSV 库
            ├── csv2.hpp
            ├── mio.hpp
            ├── parameters.hpp
            ├── reader.hpp
            └── writer.hpp
```
