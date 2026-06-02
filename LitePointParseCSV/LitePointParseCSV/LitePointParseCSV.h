#ifndef LITEPOINT_CSV_PARSE_H
#define LITEPOINT_CSV_PARSE_H


#ifdef __cplusplus
extern "C" {
#endif

    // error callback type
    typedef void(*ErrorCallback)(int lineNo, const char* msg, void* userData);

    // calibration item - C struct
    typedef struct {
        char segment[256];   // [TX^CAL^2G^11N^ANT0^20BW^2412FREQ^MCS7]
        char key[128];       // ClockError / MeasurePower / EVM_-25dB ...
        char status[16];     // PASS / FAIL / "" (info only)
        double value;
        char lower[32];      // lower limit
        char upper[32];      // upper limit
    } CalItem_C;



    /* ---------- core parse interface ---------- */
    int __stdcall ParseLitePointCsv(
        const char* filePath,
        CalItem_C** items,          // output array, dynamically allocated
        int* itemCount,             // output array size
        ErrorCallback onError,      // error callback
        void* userData              // callback user data
    );

    /* ---------- free result ---------- */
    void __stdcall FreeParseResult(CalItem_C* items);

    /* ---------- path loss calculation ---------- */
    int __stdcall CalculatePathLoss(
        const char* powerTagCsv,    // tag power file path
        const char* measPowerCsv,   // measured power file path
        const char* outLossCsv,     // output loss file path
        ErrorCallback onError,      // error callback
        void* userData              // callback user data
    );

    /* ---------- extract Tx Power + Rx DFS RSSI ---------- */
    int __stdcall ExtractMeasPowerFromCsv(
        const char* testCsvPath,    // test CSV file path
        const char* outCsvPath,     // output CSV path
        ErrorCallback onError,      // error callback
        void* userData              // callback user data
    );

    /* ---------- multi-file avg power ---------- */
    int __stdcall CalcAvgPowerFromSet(
        const char* const* filePathSet,   // CSV file path array (nullptr terminated)
        const char* outAvgCsvPath,        // output average CSV path
        ErrorCallback onError,            // error callback
        void* userData                    // callback user data
    );

#ifdef __cplusplus
}
#endif

#endif // LITEPOINT_CSV_PARSE_H
