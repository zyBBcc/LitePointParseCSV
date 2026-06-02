#include <iostream>
#include <string>
#include <vector>
#include "LitePointParseCSV.h"

using namespace std;

void onError(int lineNo, const char* msg, void* userData) {
    cerr << "[ERROR] line " << lineNo << ": " << msg << endl;
}

string csvPath(const string& filename) {
    return "../../CSV/" + filename;
}

int main() {
    cout << "=== LitePointParseCSV Test Suite ===" << endl << endl;
    int passed = 0, failed = 0;

    // ========================================
    // Test 1: ParseLitePointCsv
    // ========================================
    /*cout << "--- Test 1: ParseLitePointCsv ---" << endl;
    {
        CalItem_C* items = nullptr;
        int count = 0;
        int ret = ParseLitePointCsv(csvPath("12345678.csv").c_str(), &items, &count, onError, nullptr);
        if (ret == 1 && count > 0 && items != nullptr) {
            cout << "  PASS: parsed " << count << " calibration items" << endl;
            int show = count < 5 ? count : 5;
            for (int i = 0; i < show; ++i) {
                cout << "    [" << items[i].segment << "] "
                    << items[i].key << " = " << items[i].value
                    << " (" << items[i].status << ")" << endl;
            }
            if (count > 5) cout << "    ... (" << (count - 5) << " more)" << endl;
            passed++;
        } else {
            cerr << "  FAIL: ParseLitePointCsv returned " << ret << ", count=" << count << endl;
            failed++;
        }
        FreeParseResult(items);
    }*/

    // ========================================
    // Test 2: ExtractMeasPowerFromCsv
    // ========================================
    cout << "\n--- Test 2: ExtractMeasPowerFromCsv ---" << endl;
    {
        string outFile = csvPath("_test_output_power.csv");
        int ret = ExtractMeasPowerFromCsv(csvPath("12345678.csv").c_str(), outFile.c_str(), onError, nullptr);
        if (ret == 1) {
            cout << "  PASS: output written to " << outFile << endl;
            passed++;
        } else {
            cerr << "  FAIL: ExtractMeasPowerFromCsv returned " << ret << endl;
            failed++;
        }

        // also test with dfs_RSSI.csv (contains Rx Reciever DFS section)
        string outFile2 = csvPath("_test_output_dfs.csv");
        ret = ExtractMeasPowerFromCsv(csvPath("dfs_RSSI.csv").c_str(), outFile2.c_str(), onError, nullptr);
        if (ret == 1) {
            cout << "  PASS (DFS): output written to " << outFile2 << endl;
            passed++;
        } else {
            cerr << "  FAIL (DFS): ExtractMeasPowerFromCsv returned " << ret << endl;
            failed++;
        }

		string lossFile = csvPath("_test_loss.csv");
		ret = CalculatePathLoss(outFile.c_str(), outFile2.c_str(), lossFile.c_str(), onError, nullptr);
		if (ret == 1) {
			cout << "  PASS: loss output written to " << lossFile << endl;
			passed++;
		}
		else {
			cerr << "  FAIL: CalculatePathLoss returned " << ret << endl;
			failed++;
		}
    }

    // ========================================
    // Test 3: CalculatePathLoss
    // ========================================
    /*cout << "\n--- Test 3: CalculatePathLoss ---" << endl;
    {
        string pwrFile = csvPath("_test_pwr.csv");
        string lossFile = csvPath("_test_loss.csv");
        ExtractMeasPowerFromCsv(csvPath("12345678.csv").c_str(), pwrFile.c_str(), onError, nullptr);

        int ret = CalculatePathLoss(pwrFile.c_str(), pwrFile.c_str(), lossFile.c_str(), onError, nullptr);
        if (ret == 1) {
            cout << "  PASS: loss output written to " << lossFile << endl;
            passed++;
        } else {
            cerr << "  FAIL: CalculatePathLoss returned " << ret << endl;
            failed++;
        }
    }*/

    // ========================================
    // Test 4: CalcAvgPowerFromSet
    // ========================================
    /*cout << "\n--- Test 4: CalcAvgPowerFromSet ---" << endl;
    {
        string pwrFile = csvPath("_test_pwr.csv");
        string avgFile = csvPath("_test_avg.csv");
        const char* fileSet[] = { pwrFile.c_str(), pwrFile.c_str(), nullptr };
        int ret = CalcAvgPowerFromSet(fileSet, avgFile.c_str(), onError, nullptr);
        if (ret == 1) {
            cout << "  PASS: avg output written to " << avgFile << endl;
            passed++;
        } else {
            cerr << "  FAIL: CalcAvgPowerFromSet returned " << ret << endl;
            failed++;
        }
    }*/

    // ========================================
    // Summary
    // ========================================
    cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===" << endl;
    return failed > 0 ? 1 : 0;
}
