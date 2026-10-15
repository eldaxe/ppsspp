# Integration plan for `new/` branches

Target base: `master`.

Branches discovered:
- `new/button-pressed-style`
- `new/buttons-svg-quality-option`
- `new/circular-touch-analog-range`
- `new/log-view-overlay`
- `new/swap-touch-layout`
- `new/touch-button-summary`

## Conflict-sensitive areas

- `Core/Config.h` and `Core/Config.cpp`: feature settings must be combined, not selected from one branch. Pressed styles, SVG quality/shadows, circular analog range, swap layout, and custom button summary settings overlap these config files.
- `UI/GameSettingsScreen.cpp`: multiple branches add controls into the same Controls settings section.
- `UI/GamepadEmu.cpp` / `.h`: pressed feedback, circular analog input, one-shot swap-layout touch handling, and summary labels overlap gameplay button/analog code.
- `UI/TouchControlLayoutScreen.cpp`, `UI/TouchControlVisibilityScreen.cpp`, and mapping screens: swap layout and custom button summary features overlap editor/configuration UI.
- `.github/workflows/manual_generate_apk.yml`: all old commits include unrelated workflow edits. Keep current `master` workflow unless a specific necessary change is separately justified.

## Source history warning

The branches have different historical merge bases and are hundreds to over a thousand commits behind current `master`. Their original commit trees contain many changes unrelated to their intended feature. Do not merge those branch tips as whole trees or use an entire old file to replace a current file. Port feature-level changes onto current `master`, reviewing overlapping code and preserving all settings and behavior.

This document is an integration checklist, not a claim that the features have been merged or tested.