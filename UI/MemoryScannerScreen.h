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

#pragma once

#include <vector>
#include <string>

#include "Common/File/Path.h"
#include "Common/UI/UIScreen.h"
#include "Common/UI/View.h"
#include "Common/UI/ViewGroup.h"
#include "Core/MemoryScanner.h"

class PopupTextInputChoice;

class MemoryScannerScreen : public UIDialogScreen {
public:
	MemoryScannerScreen(Path gamePath);
	~MemoryScannerScreen() override;

	void CreateViews() override;
	void update() override;
	void RebuildResultsList();

	[[nodiscard]] const char *tag() const override { return "MemoryScanner"; }

private:
	void OnFirstScan(UI::EventParams &e);
	void OnNextScan(UI::EventParams &e);
	void OnClear(UI::EventParams &e);
	void OnValueChange(uint32_t addr, ScanValueType type, const std::string &value);
	void SearchAddress(const std::string &text, bool cwcheat);

	Path gamePath_;

	UI::TextView *countText_ = nullptr;
	PopupTextInputChoice *searchValueEdit_ = nullptr;
	PopupTextInputChoice *addressSearchEdit_ = nullptr;
	PopupTextInputChoice *cwcheatSearchEdit_ = nullptr;
	std::string addressSearch_;
	std::string cwcheatSearch_;
	UI::LinearLayout *resultsList_ = nullptr;
	std::vector<PopupTextInputChoice *> valueChoices_;
	std::vector<std::string> editValues_;
	std::vector<uint32_t> resultAddresses_;
	std::vector<ScanValueType> resultTypes_;
};
