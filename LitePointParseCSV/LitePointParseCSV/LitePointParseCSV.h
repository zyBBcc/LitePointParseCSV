#ifndef LITEPOINT_CSV_PARSE_H
#define LITEPOINT_CSV_PARSE_H


#ifdef __cplusplus
extern "C" {
#endif

	// 回调函数类型定义
	typedef void(*ErrorCallback)(int lineNo, const char* msg, void* userData);

	// 输出项结构体 - C风格
	typedef struct {
		char segment[256];   // [TX^CAL^2G^11N^ANT0^20BW^2412FREQ^MCS7]
		char key[128];       // ClockError / MeasurePower / EVM_-25dB ...
		char status[16];     // PASS / FAIL / "" (info only)
		double value;
		char lower[32];      // 规格下限
		char upper[32];      // 规格上限
	} CalItem_C;

	

	/* ---------- 核心解析接口 ---------- */
	int __stdcall ParseLitePointCsv(
		const char* filePath,
		CalItem_C** items,          // 输出：动态分配的数组
		int* itemCount,            // 输出：数组大小
		ErrorCallback onError,     // 错误回调
		void* userData             // 回调用户数据
	);

	/* ---------- 工具函数 ---------- */
	void __stdcall FreeParseResult(CalItem_C* items);

	/* ---------- 线损计算 ---------- */
	int __stdcall CalculatePathLoss(
		const char* powerTagCsv,    // 金版文件路径
		const char* measPowerCsv,   // 实测功率文件路径
		const char* outLossCsv,     // 输出线损文件路径
		ErrorCallback onError,      // 错误回调
		void* userData              // 回调用户数据
	);

	/* ---------- 从测试CSV中提取测量功率 ---------- */
	int __stdcall ExtractMeasPowerFromCsv(
		const char* testCsvPath,    // 测试CSV文件路径
		const char* outCsvPath,     // 输出功率CSV路径
		ErrorCallback onError,      // 错误回调
		void* userData              // 回调用户数据
	);

	/* ---------- 多文件功率平均值计算 ---------- */
	int __stdcall CalcAvgPowerFromSet(
		const char* const* filePathSet,   // CSV 文件路径数组（以 nullptr 结尾）
		const char* outAvgCsvPath,       // 输出平均值 CSV 路径
		ErrorCallback onError,            // 错误回调
		void* userData            // 回调用户数据
	);

#ifdef __cplusplus
}
#endif

#endif // LITEPOINT_CSV_PARSE_H