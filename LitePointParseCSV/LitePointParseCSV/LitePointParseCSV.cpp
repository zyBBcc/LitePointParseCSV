#include "pch.h"
#include "LitePointParseCSV.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <optional>
#include <regex>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <cstring>
#include <windows.h>

#include "csv2/csv2.hpp"

using namespace std;

// --------------- internal struct ---------------
struct CalItem {
	std::string segment;
	std::string key;
	std::string status;
	double value = 0;
	std::string lower;
	std::string upper;
};

struct RowEx {
	int lineNo;
	std::vector<std::string> cells;
};

// --------------- str -> double ---------------
double toDouble(const std::string& s) {
	try { return stod(s); }
	catch (...) { return 0; }
}

// --------------- get cell by column name ---------------
std::string getCell(const std::vector<std::string>& row,
	const std::vector<std::string>& hdr,
	const std::string& colName) {
	for (size_t i = 0; i < hdr.size(); ++i)
		if (hdr[i] == colName) return i < row.size() ? row[i] : "";
	return "";
}

// --------------- push item to result ---------------
void pushItem(std::vector<CalItem>& out,
	const std::string& seg,
	const std::string& key,
	const std::string& st,
	double val,
	const std::string& lo = "",
	const std::string& hi = "") {
	out.push_back({ seg, key, st, val, lo, hi });
}

// --------------- error report helper ---------------
void reportError(ErrorCallback onError, void* userData, int lineNo, const std::string& msg) {
	if (onError) {
		onError(lineNo, msg.c_str(), userData);
	}
}

// --------------- internal parser functions ---------------
// extract and adapt from original code

bool extractPowerEvmLimitStrings(const std::string& hdr,
	std::string& pwrMinStr,
	std::string& evmLimStr)
{
	// capture groups include +/- sign
	std::regex re(R"(\[>=((?:\+|-)?\d+(?:\.\d+)?)dBm\].*\[<=(\-?\d+(?:\.\d+)?)dB\])",
		std::regex_constants::icase);
	/*std::regex re(R"(\[>=([+-]?\d+(?:\.\d+)?)dBm\].*\[<=-([+-]?\d+(?:\.\d+)?)dB\])",
		std::regex_constants::icase);*/
	std::smatch m;
	if (std::regex_search(hdr, m, re))
	{
		pwrMinStr = m[1];   // with sign, e.g. "23" or "-23"
		evmLimStr = m[2];   // with sign, e.g. "-25"
		return true;
	}
	return false;
}


void parseEVMPairs(const std::vector<std::string>& row,
	const std::vector<std::string>& hdr,
	const std::string& seg,
	std::vector<CalItem>& out)
{
	for (size_t i = 0; i + 1 < hdr.size(); ++i)
	{
		std::string p, e;
		if (!extractPowerEvmLimitStrings(hdr[i], p, e)) {
			continue;
		}

		double measuredPwr = toDouble(row[i - 2]);   // Power @ EVM column
		double maxPwrEvm = toDouble(row[i - 1]);       // Max Power Evm or UEvm column

		std::string stPwr = (measuredPwr >= toDouble(p)) ? "PASS" : "FAIL";
		std::string stMax = (maxPwrEvm <= toDouble(e)) ? "PASS" : "FAIL";

		if (hdr[i - 1] == "UEvm") {
			pushItem(out, seg, "UEvm", stMax, maxPwrEvm, "", e);
		}
		else {
			pushItem(out, seg, "Max_Power_Evm", stMax, maxPwrEvm, "", e);
		}

		pushItem(out, seg, "Power_EVM" + e + "dB", stPwr, measuredPwr, p, "");
	}
}

bool extractLimitRange(const std::string& s,
	std::string& lo,
	std::string& hi)
{
	
	std::regex re(R"(\(\s*([+-]?\d+(?:\.\d+)?)\s*-\s*([+-]?\d+(?:\.\d+)?)\s*\))");
	std::smatch m;
	if (std::regex_match(s, m, re))
	{
		lo = m[1];
		hi = m[2];
		return true;
	}
	lo = "";
	hi = "";
	return false;
}

void getLimitRange(const std::vector<std::string>& row,
	const std::vector<std::string>& hdr,
	const std::string& col,
	std::string& lo,
	std::string& hi)
{
	std::string cont = getCell(row, hdr, col);
	extractLimitRange(cont, lo, hi);
}

bool parseTxCal(const std::vector<std::string>& hdr,
	const std::vector<RowEx>& data,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData)
{
	const std::string segPrefix = "TX_CAL";

	for (size_t r = 0; r < data.size(); ++r)
	{
		const auto& row = data[r].cells;
		if (row.empty()) continue;

		/* ---------- 1.  ---------- */
		auto chk = [&](const char* col) -> std::string
		{
			std::string v = getCell(row, hdr, col);
			return v;
		};

		std::string band = chk("Band");
		std::string mode = chk("Tx Mode");
		std::string ant = chk("Ant");
		std::string bw = chk("BW");
		std::string freq = chk("Frequency");
		std::string mcs = chk("Rate");
		std::string plStr = chk("Power Limit");

		if (band.empty() || mode.empty() || ant.empty() ||
			bw.empty() || freq.empty() || mcs.empty())
		{
			reportError(onError, userData, data[r].lineNo, "band/mode/ant/bw/freq/mcs empty val");
			continue;
		}

		/* ---------- 2. build segment name ---------- */
		std::string seg = segPrefix + "_" + band + "_" + mode + "_ANT" + ant +
			"_" + bw.substr(0, bw.find("MHz")) + "BW^" + freq + "FREQ^" + mcs;

		/* ---------- 3. Power Limit range must be valid ---------- */
		std::string powerLo, powerHi;
		extractLimitRange(plStr, powerLo, powerHi);

		/* ---------- 5. fill data ---------- */
		pushItem(out, seg, "ClockError", "", toDouble(getCell(row, hdr, "Clock Error")));
		pushItem(out, seg, "Poweroffset", "", toDouble(getCell(row, hdr, "Power offset")));

		double mp = toDouble(getCell(row, hdr, "Measure Power"));
		std::string mpSt = getCell(row, hdr, "Power Pass/Fail");
		pushItem(out, seg, "MeasurePower", mpSt, mp, powerLo, powerHi);

		parseEVMPairs(row, hdr, seg, out);

		/* ---------- 6. non-critical columns, no error ---------- */
		auto safeDouble = [&](const char* col) { return toDouble(getCell(row, hdr, col)); };
		pushItem(out, seg, "S2D_GAIN_Reg1", "", safeDouble("S2D GAIN Reg1"));
		pushItem(out, seg, "S2D_OFFSET_Reg1", "", safeDouble("S2D OFFSET Reg1"));
		pushItem(out, seg, "COEFF_A_Reg1", "", safeDouble("COEFF A Reg1"));
		pushItem(out, seg, "COEFF_B_Reg1", "", safeDouble("COEFF B Reg1"));
		pushItem(out, seg, "Error_Reg1", "", safeDouble("Error Reg1"));
		pushItem(out, seg, "Temp", "", safeDouble("Temp"));
	}

	return true;
}

void parseGeneral(const std::vector<std::string>& hdr,
	const std::vector<RowEx>& data,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData)
{
	const std::string segPrefix = "LOTINFO";
	for (const auto& row : data) {
		std::string iqdvt_version = getCell(row.cells, hdr, "IQDVT Version");
		std::string cv_version = getCell(row.cells, hdr, "CV VERSION");
		std::string dut_version = getCell(row.cells, hdr, "DUT VERSION");
		if (iqdvt_version.empty() || cv_version.empty() || dut_version.empty()) {
			reportError(onError, userData, row.lineNo, "iqdvt_version/cv_version/dut_version empty val");
			continue;
		}

		std::string seg = segPrefix;
		//pushItem(out, seg, "TestMsg", "", toDouble(getCell(row, hdr, "Temperature")));
	}
}


void parseXtalCal(const std::vector<std::string>& hdr,
	const std::vector<RowEx>& data,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData)
{
	const std::string segPrefix = "XTAL_CAL";
	for (const auto& row : data) {
		if (row.cells.empty()) continue;
		std::string band = getCell(row.cells, hdr, "Band");
		if (band.empty()) { // required column missing
			reportError(onError, userData, row.lineNo, "Band empty val");
			continue;
		}
		std::string seg = segPrefix + "^" + band + "^";

		pushItem(out, seg, "OffsetWord", "", toDouble(getCell(row.cells, hdr, "OffsetWord")));
		pushItem(out, seg, "FreqOffset_PPM", getCell(row.cells, hdr, "Pass/Fail"),
			toDouble(getCell(row.cells, hdr, "Freq Offset[PPM]")),
			"", "");   // Limit 
		pushItem(out, seg, "Temperature", "", toDouble(getCell(row.cells, hdr, "Temperature")));
	}
}

// =========================  LNA Gain =========================
void parseLnaGainDynamic(const std::vector<std::string>& hdr,
	const std::vector<std::string>& row,
	const std::string& seg,
	std::vector<CalItem>& out)
{
	// LNA GAIN 
	std::regex lnaRe(R"(LNA GAIN (\d+))", std::regex_constants::icase);
	// Pass/Fail\([+-]?\d+\+-\/(\d+)\)  
	std::regex specRe(R"(\(\s*([+-]?\d+)\s*\+\-\s*\/\s*(\d+)\s*\))", std::regex_constants::icase);

	for (size_t i = 0; i < hdr.size(); ++i) {
		std::smatch m;
		if (!std::regex_match(hdr[i], m, lnaRe)) continue;
		int idx = std::stoi(m[1].str());          // Gain 

		double g = toDouble(row[i]); // measured value
		// next column: Pass/Fail(xxx+-/n)
		std::string specCol = hdr[i] + " Pass/Fail";
		if (i + 1 >= hdr.size()) continue;
		std::string specCell = hdr[i + 1];
		std::smatch sm;
		double center = 0.0, tol = 0.0;
		if (std::regex_search(specCell, sm, specRe)) {
			center = std::stod(sm[1].str());
			tol = std::stod(sm[2].str());
		}
		std::string st = (std::abs(g - center) <= tol) ? "PASS" : "FAIL";
		pushItem(out, seg, "LNA_Gain_" + std::to_string(idx), st, g,
			std::to_string(center - tol), std::to_string(center + tol));
	}
}

// =========================  Flatness =========================
void parseFlatnessDynamic(const std::vector<std::string>& hdr,
	const std::vector<std::string>& row,
	const std::string& seg,
	std::vector<CalItem>& out,
	const std::string& prefix)   // "Flatness High Gain " or "Flatness Bypass "
{
	// Flatness Xxxx yyyy
	std::regex flatRe(prefix + R"((\d+))", std::regex_constants::icase);
	// limit column: prefix + "Pass/Fail(n)"
	std::string limitCol = prefix + "Pass/Fail";
	double limit = 3.0; // default
	
	for (const auto& h : hdr) {
		if (h.find(limitCol) == std::string::npos) continue;
		//Flatness High Gain Pass/Fail(+-4)
		std::regex limRe(R"(\(([+-]{1,2})\s*(\d+(?:\.\d+)?)\s*\))", std::regex_constants::icase);
		std::smatch lm;
		if (std::regex_search(h, lm, limRe)) limit = std::stod(lm[2].str());
		break;
	}

	for (size_t i = 0; i < hdr.size(); ++i) {
		std::smatch m;
		if (!std::regex_match(hdr[i], m, flatRe)) continue;
		std::string freq = m[1].str();
		double v = toDouble(row[i]);
		std::string st = (std::abs(v) <= limit) ? "PASS" : "FAIL";
		pushItem(out, seg, prefix + freq, st, v,
			std::to_string(-limit), std::to_string(limit));
	}
}


void parseRxCal(const std::vector<std::string>& hdr,
	const std::vector<RowEx>& data,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData)
{
	const std::string segPrefix = "RX_CAL";
	for (const auto& row : data) {
		if (row.cells.empty()) continue;
		std::string band = getCell(row.cells, hdr, "Band");
		std::string ant = getCell(row.cells, hdr, "Ant");
		if (band.empty() || ant.empty()) {
			reportError(onError, userData, row.lineNo, "Band/Ant empty val");
			continue;
		}
		std::string seg = segPrefix + "^" + band + "^ANT" + ant;

		// ----------  LNA ----------
		parseLnaGainDynamic(hdr, row.cells, seg, out);

		// ----------  Flatness ----------
		parseFlatnessDynamic(hdr, row.cells, seg, out, "Flatness High Gain ");
		parseFlatnessDynamic(hdr, row.cells, seg, out, "Flatness Bypass ");

		pushItem(out, seg, "Temp", "", toDouble(getCell(row.cells, hdr, "Temp")));
	}
}

//
void pushTxperfCol(const std::vector<std::string>& row, const std::vector<std::string>& hdr, const std::string& colname, const std::string& seg, std::vector<CalItem>& out)
{
	double tmpval = 0;
	std::string lo, hi, res = "FAIL";
	std::string collimitname = "Limit[" + colname + "]";
	getLimitRange(row, hdr, collimitname, lo, hi);
	tmpval = toDouble(getCell(row, hdr, colname));
	if ((tmpval >= toDouble(lo)) && (tmpval <= toDouble(hi))) {
		res = "PASS";
	}

	std::string col_ = colname;
	std::replace(col_.begin(), col_.end(), ' ', '_');
	pushItem(out, seg, col_, res, tmpval, lo, hi);
}

//[3.87 3.85 3.89 3.95 ]
std::vector<double> parseBracketList(const std::string& src)
{
	std::vector<double> vec;
	// 1. 
	std::regex  bracketRe(R"(\[([^\]]+)\])");
	std::smatch m;
	if (!std::regex_search(src, m, bracketRe)) return vec;

	// 2. 
	std::regex numRe(R"([-+]?\d+(?:\.\d+)?)");
	auto begin = std::sregex_iterator(m[1].first, m[1].second, numRe);
	auto end = std::sregex_iterator();
	for (auto it = begin; it != end; ++it)
		vec.push_back(std::stod(it->str()));
	return vec;
}

void parseTxPerf(const std::vector<std::string>& hdr,
	const std::vector<RowEx>& data,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData)
{
	const std::string segPrefix = "TX_PERF";
	//for (const auto& row : data) {
	for (int r = 0; r < data.size(); r++) {
		const auto& row = data[r].cells;
		if (row.empty()) continue;
		std::string band = getCell(row, hdr, "Band");
		std::string mode = getCell(row, hdr, "Tx Mode");
		std::string ant = getCell(row, hdr, "Ant");
		std::string signal_bw = getCell(row, hdr, "Signal BW");
		std::string spectrum_bw = getCell(row, hdr, "Spectrum BW");
		std::string mcs = getCell(row, hdr, "Rate");
		std::string freq = getCell(row, hdr, "Frequency");
		/* ---------- 1.  ---------- */
		if (band.empty() || mode.empty() || ant.empty() || mcs.empty() || freq.empty() || signal_bw.empty() || spectrum_bw.empty()) {
			reportError(onError, userData, data[r].lineNo, "Band/Mode/Ant/Rate/Freq empty val");
			continue;
		}


		std::string seg = segPrefix + "_" + band + "_" + mode + "_ANT" + ant +
			"_" + signal_bw.substr(0, signal_bw.find("MHz")) + "SIGNALBW_" + spectrum_bw.substr(0, spectrum_bw.find("MHz")) + "SPECTRUMBW^" + freq + "FREQ^" + mcs;

		double tmpval = 0;
		std::string lo, hi, res = "FAIL";
		//Clock Error S1
		pushTxperfCol(row, hdr, "Power Error", seg, out);
		getLimitRange(row, hdr, "Limit[Clock]", lo, hi);
		tmpval = toDouble(getCell(row, hdr, "Clock Error S1"));
		if ((tmpval >= toDouble(lo)) && (tmpval <= toDouble(hi))) {
			res = "PASS";
		}
		else {
			res = "FAIL";
		}
		pushItem(out, seg, "ClockError_S1", res, tmpval, lo, hi);

		//Temperature
		pushItem(out, seg, "Temperature", "", toDouble(getCell(row, hdr, "Temperature")));
		//DUT Transmit Power
		pushItem(out, seg, "DUT_Transmit_Power", "", toDouble(getCell(row, hdr, "DUT Transmit Power")));
		//Power
		pushItem(out, seg, "Power", "", toDouble(getCell(row, hdr, "Power")));

		//Power Error
		pushTxperfCol(row, hdr, "Power Error", seg, out);

		//EVM
		pushTxperfCol(row, hdr, "EVM", seg, out);

		//Lo Leakage
		pushTxperfCol(row, hdr, "Lo Leakage", seg, out);

		//Lo Leakage Margin
		pushTxperfCol(row, hdr, "Lo Leakage Margin", seg, out);
		//Mask Violation
		pushTxperfCol(row, hdr, "EVM", seg, out);
		//Flatness Margin  [3.87 3.85 3.89 3.95 ]
		std::string fm = getCell(row, hdr, "Flatness Margin");
		std::vector<double> vecfm = parseBracketList(fm);
		for (int i = 0; i < vecfm.size(); i++)
		{
			std::string fmname = "Flatness_Margin_" + std::to_string(i);
			pushItem(out, seg, fmname, "", vecfm[i]);
		}
		//Volt
		pushItem(out, seg, "Volt", "", toDouble(getCell(row, hdr, "Volt")));
		//Spectral Mask Margin
		std::string smm = getCell(row, hdr, "Spectral Mask Margin");
		std::vector<double> vecsmm = parseBracketList(smm);
		for (int i = 0; i < vecsmm.size(); i++)
		{
			std::string name = "Spectral_Mask_Margin_" + std::to_string(i);
			pushItem(out, seg, name, "", vecsmm[i]);
		}
		//Frequency Error Hz
		pushItem(out, seg, "Frequency_Error_Hz", "", toDouble(getCell(row, hdr, "Frequency Error Hz")));

		//Frequency Error ppm
		getLimitRange(row, hdr, "Limit[Freq]", lo, hi);
		tmpval = toDouble(getCell(row, hdr, "Frequency Error ppm"));
		if ((tmpval >= toDouble(lo)) && (tmpval <= toDouble(hi))) {
			res = "PASS";
		}
		else {
			res = "FAIL";
		}
		pushItem(out, seg, "Frequency_Error_ppm", res, tmpval, lo, hi);
	}
}

void parseRxFullSweep(const std::vector<std::string>& hdr,
	const std::vector<RowEx>& data,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData)
{
	const std::string segPrefix = "RX_SWEEP";
	//for (const auto& row : data) {
	for (size_t r = 0; r < data.size(); ++r) {
		const auto& row = data[r].cells;
		if (row.empty()) continue;
		std::string band = getCell(row, hdr, "Band");
		std::string mode = getCell(row, hdr, "Rx Mode");
		std::string ant = getCell(row, hdr, "Ant");
		std::string signal_bw = getCell(row, hdr, "Signal BW");
		std::string spectrum_bw = getCell(row, hdr, "Spectrum BW");
		std::string freq = getCell(row, hdr, "Frequency");
		std::string mcs = getCell(row, hdr, "Rate");
		if (band.empty() || mode.empty() || ant.empty() || mcs.empty() || freq.empty() || signal_bw.empty() || spectrum_bw.empty()) {
			reportError(onError, userData, data[r].lineNo, "Band/Mode/Ant/Freq/Rate empty val");
			continue;
		}

		std::string seg = segPrefix + "_" + band + "_" + mode + "_ANT" + ant +
			"_" + signal_bw.substr(0, signal_bw.find("MHz")) + "SIGNALBW_" + spectrum_bw.substr(0, spectrum_bw.find("MHz")) + "SPECTRUMBW^" + freq + "FREQ^" + mcs;

		//Power Level
		pushItem(out, seg, "Power_Level", "", toDouble(getCell(row, hdr, "Power Level")));
		//PER
		pushItem(out, seg, "PER", getCell(row, hdr, "RER PASS/FAIL"), toDouble(getCell(row, hdr, "PER")));
		//RSSI
		std::string rssi;
		rssi = "RSSI" + std::to_string(atoi(ant.c_str()) + 1);
		pushItem(out, seg, "RSSI", getCell(row, hdr, "RSSI PASS/FAIL"), toDouble(getCell(row, hdr, rssi)));
		//temp
		pushItem(out, seg, "Temp", "", toDouble(getCell(row, hdr, "Temp")));
	}
}

void parseRxRecieverDFS(const std::vector<std::string>& hdr,
	const std::vector<RowEx>& data,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData)
{
	const std::string segPrefix = "RX_Reciever_DFS";
	//for (const auto& row : data) {
	for (size_t r = 0; r < data.size(); ++r) {
		const auto& row = data[r].cells;
		if (row.empty()) continue;
		std::string ant = getCell(row, hdr, "Ant");
		std::string freq = getCell(row, hdr, "Frequency");
		if (ant.empty() || freq.empty()) {
			reportError(onError, userData, data[r].lineNo, "Ant/Freq val");
			continue;
		}

		std::string seg = segPrefix + "^ANT" + ant + "^" +freq + "FREQ";

		//Power Level
		pushItem(out, seg, "DUT_Transmit_Power", "", toDouble(getCell(row, hdr, "DUT Transmit Power")));
		//RSSI
		pushItem(out, seg, "RSSI", getCell(row, hdr, "PASS/FAIL"), toDouble(getCell(row, hdr, "RSSI")));
		//temp
		pushItem(out, seg, "Temp", "", toDouble(getCell(row, hdr, "Temp")));
	}
}


// --------------- CSV ---------------
static std::vector<RowEx> LoadWholeCsv(const std::string& filePath, ErrorCallback onError, void* userData) {
	std::vector<RowEx> ret;
	csv2::Reader<csv2::delimiter<','>,
		csv2::quote_character<'"'>,
		csv2::first_row_is_header<false>> reader;

	if (!reader.mmap(filePath)) {
		reportError(onError, userData, 0, "Cannot open file: " + filePath);
		return ret;
	}

	int ln = 0;
	for (auto row : reader) {
		++ln;
		RowEx rx;
		rx.lineNo = ln;
		for (auto cell : row) {
			std::string v;
			cell.read_value(v);
			rx.cells.emplace_back(std::move(v));
		}
		if (!rx.cells.empty()) ret.emplace_back(std::move(rx));
	}

	return ret;
}

// ---------------  ---------------
enum class TestSection {
	SECTION_General = 1,
	SECTION_XTAL_Calibration,
	SECTION_TX_Calibration,
	SECTION_RX_Calibration,
	SECTION_TX_Perfomance,
	SECTION_RX_Full_Sweep,
	SECTION_RX_Reciever_DFS,
	SECTION_UNKNOWN
};

static TestSection GetSectionRow(const std::vector<std::string>& cells) {
	if (cells.size() != 1) return TestSection::SECTION_UNKNOWN;
	const std::string& ctx = cells[0];
	if (ctx.find("General") != std::string::npos)
		return TestSection::SECTION_General;
	if (ctx.find("XTAL Calibration") != std::string::npos)
		return TestSection::SECTION_XTAL_Calibration;
	if (ctx.find("TX Calibration") != std::string::npos)
		return TestSection::SECTION_TX_Calibration;
	if (ctx.find("RX Calibration") != std::string::npos)
		return TestSection::SECTION_RX_Calibration;
	if (ctx.find("Tx Performance") != std::string::npos)
		return TestSection::SECTION_TX_Perfomance;
	if (ctx.find("Rx Full Sweep") != std::string::npos)
		return TestSection::SECTION_RX_Full_Sweep;
	if (ctx.find("Rx Reciever DFS") != std::string::npos)
		return TestSection::SECTION_RX_Reciever_DFS;
	return TestSection::SECTION_UNKNOWN;
}

// ---------------  ---------------
bool ParseLitePointCsvInternal(const std::string& filePath,
	std::vector<CalItem>& out,
	ErrorCallback onError,
	void* userData) {
	const auto rawData = LoadWholeCsv(filePath, onError, userData);
	if (rawData.empty()) {
		reportError(onError, userData, 0, "Empty file or read error: " + filePath);
		return false;
	}

	std::vector<std::string> header;
	std::vector<RowEx> tableData;
	TestSection curSection = TestSection::SECTION_UNKNOWN;

	auto parseCurBlock = [&]() {
		if (curSection == TestSection::SECTION_UNKNOWN || tableData.empty()) return;

		try {
			if (curSection == TestSection::SECTION_General) {
				parseGeneral(header, tableData, out, onError, userData);
			}
			else if(curSection == TestSection::SECTION_XTAL_Calibration) {
				parseXtalCal(header, tableData, out, onError, userData);
			}
			else if (curSection == TestSection::SECTION_TX_Calibration) {
				parseTxCal(header, tableData, out, onError, userData);
			}
			else if (curSection == TestSection::SECTION_RX_Calibration) {
				parseRxCal(header, tableData, out, onError, userData);
			}
			else if (curSection == TestSection::SECTION_TX_Perfomance) {
				parseTxPerf(header, tableData, out, onError, userData);
			}
			else if (curSection == TestSection::SECTION_RX_Full_Sweep) {
				parseRxFullSweep(header, tableData, out, onError, userData);
			}
			else if (curSection == TestSection::SECTION_RX_Reciever_DFS) {
				parseRxRecieverDFS(header, tableData, out, onError, userData);
			}
		}
		catch (const std::exception & e) {
			reportError(onError, userData, 0, std::string("Parse error: ") + e.what());
		}
	};

	for (const auto& cells : rawData) {
		// Section
		if (cells.cells.size() == 1) {
			parseCurBlock();
			tableData.clear();
			header.clear();
			curSection = GetSectionRow(cells.cells);
			continue;
		}

		
		if (!cells.cells.empty() && (cells.cells[0] == "Test" || cells.cells[0] == "Serial Number")) {
			header = cells.cells;
			continue;
		}

		
		tableData.push_back(cells);
	}

	parseCurBlock();
	return true;
}

// ---------------  ---------------
int __stdcall ParseLitePointCsv(
	const char* filePath,
	CalItem_C** items,
	int* itemCount,
	ErrorCallback onError,
	void* userData) {

	if (!filePath || !items || !itemCount) {
		reportError(onError, userData, 0, "Invalid parameters");
		return 0;
	}

	std::vector<CalItem> internalItems;

	try {
		if (!ParseLitePointCsvInternal(filePath, internalItems, onError, userData)) {
			return 0;
		}
	}
	catch (const std::exception & e) {
		reportError(onError, userData, 0, std::string("Exception: ") + e.what());
		return 0;
	}
	catch (...) {
		reportError(onError, userData, 0, "Unknown exception");
		return 0;
	}

	
	*itemCount = static_cast<int>(internalItems.size());
	*items = new CalItem_C[*itemCount];

	for (int i = 0; i < *itemCount; ++i) {
		const auto& src = internalItems[i];
		CalItem_C& dst = (*items)[i];

		
		strncpy_s(dst.segment, sizeof(dst.segment), src.segment.c_str(), _TRUNCATE);
		strncpy_s(dst.key, sizeof(dst.key), src.key.c_str(), _TRUNCATE);
		strncpy_s(dst.status, sizeof(dst.status), src.status.empty() ? "PASS" : src.status.c_str(), _TRUNCATE);
		dst.value = src.value;
		strncpy_s(dst.lower, sizeof(dst.lower), src.lower.c_str(), _TRUNCATE);
		strncpy_s(dst.upper, sizeof(dst.upper), src.upper.c_str(), _TRUNCATE);
	}

	return 1;
}

void __stdcall FreeParseResult(CalItem_C* items) {
	if (items) {
		delete[] items;
	}
}

// ---------------  ---------------
bool fileExist(const std::string& path) {
	return ::GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// ---------------  ---------------
int __stdcall CalculatePathLoss(
	const char* powerTagCsv,
	const char* measPowerCsv,
	const char* outLossCsv,
	ErrorCallback onError,
	void* userData) {

	if (!powerTagCsv || !measPowerCsv || !outLossCsv) {
		reportError(onError, userData, 0, "Invalid parameters");
		return 0;
	}

	if (!fileExist(powerTagCsv)) {
		reportError(onError, userData, 0, std::string("File not exist: ") + powerTagCsv);
		return 0;
	}

	if (!fileExist(measPowerCsv)) {
		reportError(onError, userData, 0, std::string("File not exist: ") + measPowerCsv);
		return 0;
	}

	try {
		// CSV
		auto readCsv = [onError, userData](const std::string& file) -> std::map<int, std::vector<std::string>> {
			std::map<int, std::vector<std::string>> mp;
			csv2::Reader<csv2::delimiter<','>, csv2::quote_character<'"'>, csv2::first_row_is_header<false>> reader;

			if (!reader.mmap(file)) {
				reportError(onError, userData, 0, "Cannot read CSV: " + file);
				return mp;
			}

			for (auto r : reader) {
				std::vector<std::string> row;
				for (auto c : r) {
					std::string v;
					c.read_value(v);
					row.push_back(v);
				}
				if (row.empty()) continue;

				try {
					int freq = std::stoi(row[0]);
					mp[freq] = std::move(row);
				}
				catch (...) {
					reportError(onError, userData, 0, "Invalid frequency in CSV: " + file);
				}
			}
			return mp;
		};

		auto A = readCsv(powerTagCsv);
		auto B = readCsv(measPowerCsv);

		std::ofstream ofs(outLossCsv);
		if (!ofs) {
			reportError(onError, userData, 0, std::string("Cannot create file: ") + outLossCsv);
			return 0;
		}

		
		for (const auto& data : B) {
			auto it = A.find(data.first);
			if (it == A.end()) {
				std::string err = "Missing freq " + std::to_string(data.first) + " in file: " + powerTagCsv;
				reportError(onError, userData, 0, err);
				continue;
			}

			const auto& rowA = it->second;
			ofs << data.first;

			for (size_t i = 1; i < data.second.size(); ++i) {
				double vb = toDouble(data.second[i]);
				double va = (i < rowA.size()) ? toDouble(rowA[i]) : 0.0;
				ofs << ',' << (va - vb);
			}
			ofs << '\n';
		}

		return 1;
	}
	catch (const std::exception & e) {
		reportError(onError, userData, 0, std::string("Exception in CalculatePathLoss: ") + e.what());
		return 0;
	}
}

// ---------------  ---------------
int __stdcall ExtractMeasPowerFromCsv(
	const char* testCsvPath,
	const char* outCsvPath,
	ErrorCallback onError,
	void* userData) {
	
	bool bflag = false;
	if (!testCsvPath || !outCsvPath) {
		reportError(onError, userData, 0, "Invalid parameters");
		return 0;
	}

	if (!fileExist(testCsvPath)) {
		reportError(onError, userData, 0, std::string("File not exist: ") + testCsvPath);
		return 0;
	}

	try {
		const auto rawData = LoadWholeCsv(testCsvPath, onError, userData);
		if (rawData.empty()) {
			return 0;
		}

		std::unordered_map<int, std::vector<double>> freqAntData;
		bool inTxPerf = false;
		bool inRxDFS = false;
		std::vector<std::string> hdr;

		for (size_t r = 0; r < rawData.size(); ++r) {
			const auto& row = rawData[r].cells;
			if (row.empty()) continue;

			if (row.size() == 1) {
				inTxPerf = (row[0].find("Tx Performance") != std::string::npos);
				inRxDFS  = (row[0].find("Rx Reciever DFS") != std::string::npos);
				continue;
			}

			if (!inTxPerf && !inRxDFS) continue;

			if (row[0] == "Test") {
				hdr = row;
				continue;
			}

			// Tx Performance:  PowerRx Reciever DFS:  RSSI
			if (inTxPerf) {
				try {
					int freq = std::stoi(getCell(row, hdr, "Frequency"));
					int ant = std::stoi(getCell(row, hdr, "Ant"));
					double power = std::stod(getCell(row, hdr, "Power"));

					auto& vec = freqAntData[freq];
					if (vec.size() < 5) vec.resize(5, 0.0);
					if (ant >= 0 && ant < 5) vec[ant] = power;

					bflag = true;
				}
				catch (...) {
				}
			}

			if (inRxDFS) {
				try {
					int freq = std::stoi(getCell(row, hdr, "Frequency"));
					int ant = std::stoi(getCell(row, hdr, "Ant"));
					double rssi = std::stod(getCell(row, hdr, "RSSI"));

					auto& vec = freqAntData[freq];
					if (vec.size() < 5) vec.resize(5, 0.0);
					if (ant >= 0 && ant < 5) vec[ant] = rssi;

					bflag = true;
				}
				catch (...) {
				}
			}
		}

		
		std::vector<int> freqs;
		freqs.reserve(freqAntData.size());
		for (auto& p : freqAntData) freqs.push_back(p.first);
		std::sort(freqs.begin(), freqs.end());

		std::ofstream ofs(outCsvPath);
		if (!ofs) {
			reportError(onError, userData, 0, std::string("Cannot create file: ") + outCsvPath);
			return 0;
		}
		ofs << std::fixed << std::setprecision(2);

		for (int f : freqs) {
			const auto& v = freqAntData[f];
			ofs << f;
			for (size_t i = 0; i < 5; ++i) {
				ofs << ',' << v[i];
			}
			ofs << '\n';
		}

		if (!bflag)
		{
			reportError(onError, userData, 0, std::string("Data invalid: ") + outCsvPath);
			return 0;
		}

		return 1;
	}
	catch (const std::exception & e) {
		reportError(onError, userData, 0, std::string("Exception in ExtractMeasPowerFromCsv: ") + e.what());
		return 0;
	}

}

// ------------------------------------------------------------------

int __stdcall CalcAvgPowerFromSet(
	const char* const* filePathSet,   // nullptr terminated 
	const char* outAvgCsvPath,       // output CSV path 
	ErrorCallback onError,            
	void* userData)           
{
	if (!filePathSet || !outAvgCsvPath) {
		reportError(onError, userData, 0, "Invalid parameters");
		return 0;
	}

	// 1. 
	std::vector<std::string> files;
	for (const char* const* p = filePathSet; *p; ++p) {
		if (!fileExist(*p)) {
			reportError(onError, userData, 0, std::string("File not exist: ") + *p);
			return 0;
		}
		files.emplace_back(*p);
	}
	if (files.empty()) {
		reportError(onError, userData, 0, "No input files");
		return 0;
	}

	// 2.  1  + N 
	std::map<int, std::vector<double>> sumMap;   // freq -> column power sum vector
	std::map<int, int> cntMap;                   // freq -> file occurrence count 

	for (const auto& file : files) {
		auto rows = LoadWholeCsv(file, onError, userData);
		if (rows.empty()) return 0;

		for (const auto& row : rows) {
			if (row.cells.size() < 2) continue;
			try {
				int freq = std::stoi(row.cells[0]);
				int ports = static_cast<int>(row.cells.size()) - 1; // power column count

				// init vector (first occurrence)
				if (sumMap.find(freq) == sumMap.end()) {
					sumMap[freq].resize(ports, 0.0);
					cntMap[freq] = 0;
				}

				
				for (int col = 0; col < ports; ++col) {
					sumMap[freq][col] += std::stod(row.cells[col + 1]);
				}
				cntMap[freq] += 1; // this freq appears once
			}
			catch (...) {
				
			}
		}
	}

	std::ofstream ofs(outAvgCsvPath);
	if (!ofs) {
		reportError(onError, userData, 0, std::string("Cannot create file: ") + outAvgCsvPath);
		return 0;
	}
	ofs << std::fixed << std::setprecision(2);

	for (const auto& m : sumMap) {
		int freq = m.first;
		std::vector<double> colSum = m.second;
		int fileCnt = cntMap[freq];
		ofs << freq;
		for (double s : colSum) {
			ofs << ',' << (s / fileCnt);   
		}
		ofs << '\n';
	}

	return 1;
}