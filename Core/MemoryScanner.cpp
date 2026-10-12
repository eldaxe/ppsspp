// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <algorithm>
#include <utility>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "Core/MemoryScanner.h"
#include "Core/MemMap.h"
#include "Core/System.h"
#include "Common/StringUtils.h"

MemoryScannerManager g_MemoryScanner;

struct FloatRange {
	float lower = 0.0f;
	float upper = 0.0f;
	bool valid = false;

	[[nodiscard]] bool Match(float val) const {
		if (!valid) return false;
		if (lower <= upper) {
			return val >= lower && val < upper;
		} else {
			// Negative range: e.g. lower = -12, upper = -13
			return val <= lower && val > upper;
		}
	}
};

static FloatRange ParseFloatRange(const std::string &filter) {
	FloatRange range;
	if (filter.size() < 2 || filter.substr(filter.size() - 2) != ".*")
		return range;
	std::string prefix = filter.substr(0, filter.size() - 2);
	if (prefix.empty() || prefix == "-")
		return range;
	try {
		size_t parsed = 0;
		range.lower = std::stof(prefix, &parsed);
		if (parsed != prefix.size() || !std::isfinite(range.lower))
			return FloatRange{};
		float inc = 1.0f;
		size_t dotPos = prefix.find('.');
		if (dotPos != std::string::npos) {
			size_t digits = prefix.size() - dotPos - 1;
			inc = powf(0.1f, (float)digits);
		}
		range.upper = prefix[0] == '-' ? range.lower - inc : range.lower + inc;
		range.valid = true;
	} catch (...) {
	}
	return range;
}

static bool ParseScanValue(ScanValueType type, const std::string &filter, uint32_t *value, float *floatValue, FloatRange *range, bool *wildcard) {
	*wildcard = false;
	try {
		if (type == ScanValueType::FLOAT) {
			if (filter.size() >= 2 && filter.substr(filter.size() - 2) == ".*") {
				*range = ParseFloatRange(filter);
				if (!range->valid) return false;
				*wildcard = true;
			} else {
				size_t parsed = 0;
				*floatValue = std::stof(filter, &parsed);
				if (parsed != filter.size() || !std::isfinite(*floatValue)) return false;
			}
		} else {
			size_t parsed = 0;
			unsigned long long parsedValue = std::stoull(filter, &parsed, 0);
			if (parsed != filter.size()) return false;
			uint64_t maxValue = type == ScanValueType::U8 ? UINT8_MAX : type == ScanValueType::U16 ? UINT16_MAX : UINT32_MAX;
			if (parsedValue > maxValue) return false;
			*value = (uint32_t)parsedValue;
		}
	} catch (...) {
		return false;
	}
	return true;
}

static uint32_t ReadScanBits(uint32_t address, ScanValueType type) {
	switch (type) {
	case ScanValueType::U8: return Memory::ReadUnchecked_U8(address);
	case ScanValueType::U16: return Memory::ReadUnchecked_U16(address);
	case ScanValueType::U32: return Memory::ReadUnchecked_U32(address);
	case ScanValueType::FLOAT: {
		uint32_t bits;
		float value = Memory::ReadUnchecked_Float(address);
		memcpy(&bits, &value, sizeof(bits));
		return bits;
	}
	}
	return 0;
}

static bool IsValidScanRange(uint32_t address, ScanValueType type, uint32_t start, uint32_t end) {
	uint32_t size = type == ScanValueType::U8 ? 1 : type == ScanValueType::U16 ? 2 : 4;
	return address >= start && address <= end && size <= end - address;
}

void MemoryScanner::FirstScan(ScanValueType type, const std::string &filter) {
	uint32_t val = 0;
	float fval = 0.0f;
	FloatRange frange;
	bool isWildcard = false;
	if (!ParseScanValue(type, filter, &val, &fval, &frange, &isWildcard))
		return;

	const uint32_t startAddr = 0x08000000;
	const uint32_t endAddr = startAddr + Memory::g_MemorySize;
	std::vector<ScanResult> newResults;
	newResults.reserve(1000);

	for (uint32_t addr = startAddr; IsValidScanRange(addr, type, startAddr, endAddr); ++addr) {
		bool match = false;
		if (type == ScanValueType::FLOAT) {
			float current = Memory::ReadUnchecked_Float(addr);
			match = isWildcard ? frange.Match(current) : current == fval;
		} else {
			uint32_t current = ReadScanBits(addr, type);
			uint32_t mask = type == ScanValueType::U8 ? UINT8_MAX : type == ScanValueType::U16 ? UINT16_MAX : UINT32_MAX;
			match = current == (val & mask);
		}
		if (match) {
			ScanResult result(addr, type);
			result.previousValue = ReadScanBits(addr, type);
			newResults.push_back(result);
			if (newResults.size() >= 1000)
				break;
		}
	}
	results_ = std::move(newResults);
	valueType = type;
	firstScanDone_ = true;
}

void MemoryScanner::NextScan(ScanValueType type, const std::string &filter, ScanFilterMode mode) {
	uint32_t val = 0;
	float fval = 0.0f;
	FloatRange frange;
	bool isWildcard = false;
	if (mode == ScanFilterMode::Exact && !ParseScanValue(type, filter, &val, &fval, &frange, &isWildcard))
		return;

	std::vector<ScanResult> nextResults;
	nextResults.reserve(results_.size());
	for (auto res : results_) {
		// Keep each result's original type; changing the type selector must not
		// reinterpret an existing address as a different value width.
		if (mode == ScanFilterMode::Exact && res.type != type)
			continue;
		uint32_t currentBits = ReadScanBits(res.address, res.type);
		bool match = false;
		if (mode == ScanFilterMode::Exact) {
			if (type == ScanValueType::FLOAT) {
				float current = Memory::ReadUnchecked_Float(res.address);
				match = isWildcard ? frange.Match(current) : current == fval;
			} else {
				uint32_t mask = type == ScanValueType::U8 ? UINT8_MAX : type == ScanValueType::U16 ? UINT16_MAX : UINT32_MAX;
				match = currentBits == (val & mask);
			}
		} else if (type == ScanValueType::FLOAT) {
			float current, previous;
			memcpy(&current, &currentBits, sizeof(current));
			memcpy(&previous, &res.previousValue, sizeof(previous));
			switch (mode) {
			case ScanFilterMode::Changed: match = current != previous; break;
			case ScanFilterMode::Unchanged: match = current == previous; break;
			case ScanFilterMode::Increased: match = current > previous; break;
			case ScanFilterMode::Decreased: match = current < previous; break;
			case ScanFilterMode::Exact: break;
			}
		} else {
			uint32_t mask = type == ScanValueType::U8 ? UINT8_MAX : type == ScanValueType::U16 ? UINT16_MAX : UINT32_MAX;
			uint32_t current = currentBits & mask;
			uint32_t previous = res.previousValue & mask;
			switch (mode) {
			case ScanFilterMode::Changed: match = current != previous; break;
			case ScanFilterMode::Unchanged: match = current == previous; break;
			case ScanFilterMode::Increased: match = current > previous; break;
			case ScanFilterMode::Decreased: match = current < previous; break;
			case ScanFilterMode::Exact: break;
			}
		}
		// Snapshot current value even for matches that remain, for the next
		// relative filter operation.
		res.previousValue = currentBits;
		if (match)
			nextResults.push_back(res);
	}
	results_ = std::move(nextResults);
	valueType = type;
}
void MemoryScanner::Clear() {
	results_.clear();
	firstScanDone_ = false;
}

void MemoryScanner::Update() {
	for (auto &res : results_) {
		if (res.locked) {
			switch (res.type) {
			case ScanValueType::U8:
				Memory::WriteUnchecked_U8((uint8_t)res.lockValue, res.address);
				break;
			case ScanValueType::U16:
				Memory::WriteUnchecked_U16((uint16_t)res.lockValue, res.address);
				break;
			case ScanValueType::U32:
				Memory::WriteUnchecked_U32(res.lockValue, res.address);
				break;
			case ScanValueType::FLOAT:
				{
					float f;
					memcpy(&f, &res.lockValue, 4);
					Memory::WriteUnchecked_Float(f, res.address);
				}
				break;
			}
		}
	}
}

bool MemoryScanner::SetLocked(size_t index, bool locked) {
	if (index < results_.size()) {
		if (locked) {
			if (g_MemoryScanner.HasLockedAddress(results_[index].address, this, index)) {
				results_[index].locked = false;
				return false;
			}
			// Capture current value to lock it to.
			switch (results_[index].type) {
			case ScanValueType::U8:
				results_[index].lockValue = Memory::ReadUnchecked_U8(results_[index].address);
				break;
			case ScanValueType::U16:
				results_[index].lockValue = Memory::ReadUnchecked_U16(results_[index].address);
				break;
			case ScanValueType::U32:
				results_[index].lockValue = Memory::ReadUnchecked_U32(results_[index].address);
				break;
			case ScanValueType::FLOAT:
				{
					float f = Memory::ReadUnchecked_Float(results_[index].address);
					memcpy(&results_[index].lockValue, &f, 4);
				}
				break;
			}
		}
		results_[index].locked = locked;
		return true;
	}
	return false;
}

MemoryScannerManager::MemoryScannerManager() {
	scanners_.push_back(new MemoryScanner());
}

void MemoryScannerManager::Update() {
	for (auto scanner : scanners_) {
		scanner->Update();
	}
}

void MemoryScannerManager::SetActiveIndex(int index) {
	if (index >= 0 && index < (int)scanners_.size()) {
		activeScanner_ = index;
	}
}

bool MemoryScannerManager::HasLockedAddress(uint32_t addr, const MemoryScanner *excludeScanner, size_t excludeIndex) const {
	for (auto scanner : scanners_) {
		const auto &results = scanner->GetResults();
		for (size_t i = 0; i < results.size(); ++i) {
			if (scanner == excludeScanner && i == excludeIndex)
				continue;
			if (results[i].locked && results[i].address == addr)
				return true;
		}
	}
	return false;
}

void MemoryScannerManager::AddScanner() {
	scanners_.push_back(new MemoryScanner());
	activeScanner_ = (int)scanners_.size() - 1;
}

void MemoryScannerManager::RemoveScanner(int index) {
	if (scanners_.size() <= 1) {
		return;
	}
	if (index >= 0 && index < (int)scanners_.size()) {
		delete scanners_[index];
		scanners_.erase(scanners_.begin() + index);
		if (activeScanner_ >= (int)scanners_.size()) {
			activeScanner_ = (int)scanners_.size() - 1;
		}
	}
}
