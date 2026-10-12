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
#include <cstring>
#include <stdexcept>
#include "UI/MemoryScannerScreen.h"
#include "Common/UI/Context.h"
#include "Common/UI/UIScreen.h"
#include "Common/UI/View.h"
#include "Common/UI/ViewGroup.h"
#include "Common/UI/ScrollView.h"
#include "Common/UI/PopupScreens.h"
#include "Common/UI/TabHolder.h"
#include "Common/Data/Text/I18n.h"
#include "Core/MemMap.h"
#include "Core/System.h"
#include "UI/OnScreenDisplay.h"
#include "Common/StringUtils.h"

MemoryScannerScreen::MemoryScannerScreen(Path gamePath) : UIDialogScreen(), gamePath_(std::move(gamePath)), resultsList_(nullptr) {
}

MemoryScannerScreen::~MemoryScannerScreen() = default;

void MemoryScannerScreen::CreateViews() {
	using namespace UI;

	auto di = GetI18NCategory(I18NCat::DIALOG);
	auto se = GetI18NCategory(I18NCat::SEARCH);

	root_ = new LinearLayout(ORIENT_VERTICAL);

	root_->Add(new ItemHeader(se->T("Memory Scanner")));

	LinearLayout *tabRow = root_->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
	ChoiceStrip *tabs = tabRow->Add(new ChoiceStrip(ORIENT_HORIZONTAL, new LinearLayoutParams(1.0f)));
	for (int i = 0; i < g_MemoryScanner.GetCount(); ++i) {
		tabs->AddChoice(StringFromFormat("%d", i + 1));
	}
	// Make all choices in the ChoiceStrip expand equally.
	for (int i = 0; i < tabs->GetNumSubviews(); ++i) {
		View *v = tabs->GetViewByIndex(i);
		if (v) {
			v->ReplaceLayoutParams(new LinearLayoutParams(1.0f));
		}
	}
	tabs->SetSelection(g_MemoryScanner.GetActiveIndex(), false);
	tabs->OnChoice.Add([this](EventParams &e) {
		g_MemoryScanner.SetActiveIndex((int)e.a);
		RecreateViews();
	});

	tabRow->Add(new Button("+", new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)))->OnClick.Add([this](EventParams &e) {
		g_MemoryScanner.AddScanner();
		RecreateViews();
	});
	if (g_MemoryScanner.GetCount() > 1) {
		tabRow->Add(new Button("-", new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)))->OnClick.Add([this](EventParams &e) {
			g_MemoryScanner.RemoveScanner(g_MemoryScanner.GetActiveIndex());
			RecreateViews();
		});
	}

	MemoryScanner *scanner = g_MemoryScanner.GetActive();

	LinearLayout *scanValueRow = root_->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
	searchValueEdit_ = scanValueRow->Add(new PopupTextInputChoice(GetRequesterToken(), &scanner->searchValue, se->T("Scan Value"), se->T("Enter value"), -1, screenManager(), new LinearLayoutParams(1.0f)));
	scanValueRow->Add(new Button(se->T("First Scan"), new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)))->OnClick.Handle(this, &MemoryScannerScreen::OnFirstScan);

	LinearLayout *addressRow = root_->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
	addressRow->Add(new TextView(se->T("Scan Address"), new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)));
	addressSearchEdit_ = addressRow->Add(new PopupTextInputChoice(GetRequesterToken(), &addressSearch_, "", se->T("RAM address, e.g. 0x0974A618"), -1, screenManager(), new LinearLayoutParams(1.0f)));
	addressRow->Add(new Button(se->T("Scan"), new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)))->OnClick.Add([this](EventParams &) { SearchAddress(addressSearch_, false); });

	LinearLayout *cwcheatRow = root_->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
	cwcheatRow->Add(new TextView(se->T("Scan CWCheat"), new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)));
	cwcheatSearchEdit_ = cwcheatRow->Add(new PopupTextInputChoice(GetRequesterToken(), &cwcheatSearch_, "", se->T("CWCheat address, e.g. 0x00F4A618"), -1, screenManager(), new LinearLayoutParams(1.0f)));
	cwcheatRow->Add(new Button(se->T("Scan"), new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)))->OnClick.Add([this](EventParams &) { SearchAddress(cwcheatSearch_, true); });

	LinearLayout *typeRow = root_->Add(new LinearLayout(ORIENT_HORIZONTAL));
	typeRow->Add(new TextView(se->T("Value Type"), new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)));

	ChoiceStrip *typeStrip = typeRow->Add(new ChoiceStrip(ORIENT_HORIZONTAL, new LinearLayoutParams(WRAP_CONTENT, WRAP_CONTENT)));
	typeStrip->AddChoice(se->T("8-bit"));
	typeStrip->AddChoice(se->T("16-bit"));
	typeStrip->AddChoice(se->T("32-bit"));
	typeStrip->AddChoice(se->T("Float"));
	typeStrip->SetSelection((int)scanner->valueType, false);
	typeStrip->OnChoice.Add([scanner](EventParams &e) {
		scanner->valueType = (ScanValueType)e.a;
	});

	LinearLayout *buttonRow = root_->Add(new LinearLayout(ORIENT_HORIZONTAL));
	buttonRow->Add(new Button(se->T("Filter Scan")))->OnClick.Handle(this, &MemoryScannerScreen::OnNextScan);
	buttonRow->Add(new Button(di->T("Clear")))->OnClick.Handle(this, &MemoryScannerScreen::OnClear);

	countText_ = root_->Add(new TextView(se->T("Results: 0"), new LinearLayoutParams(Margins(10, 10))));
	countText_->SetShadow(true);
	countText_->SetBig(true);

	root_->Add(new ScrollView(ORIENT_VERTICAL, new LinearLayoutParams(FILL_PARENT, 0.0f, 1.0f)))
		->Add(resultsList_ = new LinearLayout(ORIENT_VERTICAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));

	root_->Add(new Button(di->T("Back")))->OnClick.Add([this](EventParams &) { TriggerFinish(DR_BACK); });

	if (scanner->HasScanDone()) {
		RebuildResultsList();
	}
}

void MemoryScannerScreen::update() {
	UIScreen::update();
	MemoryScanner *scanner = g_MemoryScanner.GetActive();
	if (countText_) {
		const auto &results = scanner->GetResults();
		auto se = GetI18NCategory(I18NCat::SEARCH);
		countText_->SetText(StringFromFormat(se->T_cstr("Results: %d"), (int)results.size()));
	}
	if (resultsList_) {
		resultsList_->SetVisibility(scanner->HasScanDone() ? UI::V_VISIBLE : UI::V_GONE);
	}

	// Update live values in the list.
	// Only update if the UI is in sync with the current scanner results.
	const auto &results = scanner->GetResults();
	if (valueChoices_.size() == std::min((size_t)100, results.size())) {
		for (size_t i = 0; i < valueChoices_.size(); ++i) {
			uint32_t addr = resultAddresses_[i];
			ScanValueType type = resultTypes_[i];
			std::string valStr;
			switch (type) {
			case ScanValueType::U8: valStr = StringFromFormat("%d", (int)Memory::ReadUnchecked_U8(addr)); break;
			case ScanValueType::U16: valStr = StringFromFormat("%d", (int)Memory::ReadUnchecked_U16(addr)); break;
			case ScanValueType::U32: valStr = StringFromFormat("%u", Memory::ReadUnchecked_U32(addr)); break;
			case ScanValueType::FLOAT: valStr = StringFromFormat("%f", Memory::ReadUnchecked_Float(addr)); break;
			}
			editValues_[i] = valStr;
		}
	} else if (scanner->HasScanDone()) {
		// If out of sync, trigger a rebuild.
		RebuildResultsList();
	}
}

void MemoryScannerScreen::RebuildResultsList() {
	if (!resultsList_)
		return;
	using namespace UI;
	auto se = GetI18NCategory(I18NCat::SEARCH);
	resultsList_->Clear();
	valueChoices_.clear();
	editValues_.clear();
	resultAddresses_.clear();
	resultTypes_.clear();

	MemoryScanner *scanner = g_MemoryScanner.GetActive();
	auto &results = scanner->GetResults();

	if (results.empty())
		return;

	LinearLayout *header = resultsList_->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
	auto *addrHeader = header->Add(new TextView(se->T("Address"), new LinearLayoutParams(200.0f, WRAP_CONTENT)));
	addrHeader->SetShadow(true);
	addrHeader->SetPadding(Margins(16, 0));
	auto *valHeader = header->Add(new TextView(se->T("Value"), new LinearLayoutParams(1.0f)));
	valHeader->SetShadow(true);
	valHeader->SetPadding(Margins(16, 0));
	auto *lockHeader = header->Add(new TextView(se->T("Lock"), new LinearLayoutParams(100.0f, WRAP_CONTENT)));
	lockHeader->SetShadow(true);
	lockHeader->SetPadding(Margins(16, 0));

	const size_t visibleResults = std::min((size_t)100, results.size());
	editValues_.reserve(visibleResults);
	for (size_t i = 0; i < visibleResults; ++i) {
		LinearLayout *row = resultsList_->Add(new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT)));
		uint32_t addr = results[i].address;
		ScanValueType type = results[i].type;

		row->Add(new Choice(StringFromFormat("0x%08X", addr), new LinearLayoutParams(112.0f, WRAP_CONTENT)));

		std::string valStr;
		switch (type) {
		case ScanValueType::U8: valStr = StringFromFormat("%d", (int)Memory::ReadUnchecked_U8(addr)); break;
		case ScanValueType::U16: valStr = StringFromFormat("%d", (int)Memory::ReadUnchecked_U16(addr)); break;
		case ScanValueType::U32: valStr = StringFromFormat("%u", Memory::ReadUnchecked_U32(addr)); break;
		case ScanValueType::FLOAT: valStr = StringFromFormat("%f", Memory::ReadUnchecked_Float(addr)); break;
		}

		editValues_.push_back(valStr);
		PopupTextInputChoice *valChoice = row->Add(new PopupTextInputChoice(GetRequesterToken(), &editValues_.back(), "", "", -1, screenManager(), new LinearLayoutParams(1.0f)));
		valChoice->OnChange.Add([this, addr, type, i](EventParams &) {
			OnValueChange(addr, type, editValues_[i]);
		});
		valueChoices_.push_back(valChoice);
		resultAddresses_.push_back(addr);
		resultTypes_.push_back(type);

		auto *lockCheck = row->Add(new CheckBox(&results[i].locked, "", "", new LinearLayoutParams(56.0f, WRAP_CONTENT)));
		lockCheck->OnClick.Add([scanner, i](EventParams &e) {
			if (!scanner->SetLocked(i, scanner->GetResults()[i].locked)) {
				scanner->GetResults()[i].locked = false;
				auto se = GetI18NCategory(I18NCat::SEARCH);
				g_OSD.Show(OSDType::MESSAGE_ERROR, se->T("Address already locked"), 3.0f);
			}
		});


	}
}

void MemoryScannerScreen::SearchAddress(const std::string &text, bool cwcheat) {
	MemoryScanner *scanner = g_MemoryScanner.GetActive();
	auto se = GetI18NCategory(I18NCat::SEARCH);
	try {
		std::string addressText = StripSpaces(text);
		if (addressText.empty()) throw std::invalid_argument("empty address");
		ScanValueType type = scanner->valueType;
		uint32_t address = 0;
		if (cwcheat) {
			// Require a single 0x-prefixed address word. Do not accept full _L lines.
			if (addressText.compare(0, 2, "0x") != 0 && addressText.compare(0, 2, "0X") != 0)
				throw std::invalid_argument("CWCheat address must start with 0x");
			if (addressText.find_first_of(" \\t\\r\\n") != std::string::npos || addressText.rfind("_L", 0) == 0)
				throw std::invalid_argument("invalid CWCheat address");
			uint32_t codeAddress = (uint32_t)std::stoul(addressText.substr(2), nullptr, 16);
			uint32_t opcode = codeAddress & 0xF0000000;
			if (opcode == 0x00000000) type = ScanValueType::U8;
			else if (opcode == 0x10000000) type = ScanValueType::U16;
			else if (opcode == 0x20000000) type = ScanValueType::U32;
			else throw std::invalid_argument("unsupported CWCheat opcode");
			address = 0x08800000 + (codeAddress & 0x0FFFFFFF);
		} else {
			// RAM addresses must be explicitly hexadecimal with a 0x prefix.
			if (addressText.compare(0, 2, "0x") != 0 && addressText.compare(0, 2, "0X") != 0)
				throw std::invalid_argument("RAM address must start with 0x");
			if (addressText.find_first_of(" \\t\\r\\n") != std::string::npos)
				throw std::invalid_argument("invalid RAM address");
			address = (uint32_t)std::stoul(addressText.substr(2), nullptr, 16);
		}
		const uint32_t start = 0x08000000;
		const uint32_t end = start + Memory::g_MemorySize;
		if (address < start || address >= end) throw std::out_of_range("address outside PSP RAM");
		for (const auto &result : scanner->GetResults()) {
			if (result.address == address && result.type == type) {
				g_OSD.Show(OSDType::MESSAGE_INFO, se->T("Address already in results"), 2.0f);
				return;
			}
		}
		ScanResult result(address, type);
		scanner->GetResults().insert(scanner->GetResults().begin(), result);
		scanner->MarkScanDone();
		g_OSD.Show(OSDType::MESSAGE_INFO, se->T("Address added to results"), 2.0f);
		RebuildResultsList();
	} catch (...) {
		g_OSD.Show(OSDType::MESSAGE_ERROR, se->T(cwcheat ? "Invalid CWCheat address" : "Invalid RAM address"), 3.0f);
	}
}

void MemoryScannerScreen::OnFirstScan(UI::EventParams &e) {
	MemoryScanner *scanner = g_MemoryScanner.GetActive();
	scanner->FirstScan(scanner->valueType, scanner->searchValue);
	RebuildResultsList();
}

void MemoryScannerScreen::OnNextScan(UI::EventParams &e) {
	MemoryScanner *scanner = g_MemoryScanner.GetActive();
	if (scanner->HasScanDone()) {
		scanner->NextScan(scanner->valueType, scanner->searchValue);
		RebuildResultsList();
	}
}

void MemoryScannerScreen::OnClear(UI::EventParams &e) {
	g_MemoryScanner.GetActive()->Clear();
	RebuildResultsList();
}

void MemoryScannerScreen::OnValueChange(uint32_t addr, ScanValueType type, const std::string &value) {
	auto se = GetI18NCategory(I18NCat::SEARCH);
	MemoryScanner *scanner = g_MemoryScanner.GetActive();

	if (g_MemoryScanner.IsAddressLocked(addr)) {
		bool lockedHere = false;
		for (const auto &res : scanner->GetResults()) {
			if (res.address == addr && res.locked) {
				lockedHere = true;
				break;
			}
		}
		if (!lockedHere) {
			g_OSD.Show(OSDType::MESSAGE_ERROR, se->T("Address locked in another tab"), 3.0f);
			return;
		}
	}

	try {
		if (type == ScanValueType::FLOAT) {
			float fval = std::stof(value);
			Memory::WriteUnchecked_Float(fval, addr);
			for (auto &res : scanner->GetResults()) {
				if (res.address == addr && res.locked) {
					memcpy(&res.lockValue, &fval, sizeof(fval));
				}
			}
		} else {
			uint32_t val = (uint32_t)std::stoul(value, nullptr, 0);
			switch (type) {
			case ScanValueType::U8: Memory::WriteUnchecked_U8((uint8_t)val, addr); break;
			case ScanValueType::U16: Memory::WriteUnchecked_U16((uint16_t)val, addr); break;
			case ScanValueType::U32: Memory::WriteUnchecked_U32(val, addr); break;
			case ScanValueType::FLOAT: break;
			}
			for (auto &res : scanner->GetResults()) {
				if (res.address == addr && res.locked) {
					res.lockValue = val;
				}
			}
		}
	} catch (...) {
		g_OSD.Show(OSDType::MESSAGE_ERROR, se->T("Invalid value"), 3.0f);
	}
}
