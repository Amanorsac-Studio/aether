# AETHER — Dynamic Air Exciter

**Amanorsac Studio** · VST3 (AU on macOS; AAX and Standalone optional) · JUCE 8 · C++17

Fresh Air gives you two fixed shelves and a trim. AETHER gives you a tunable tone section, a real harmonic
exciter, a sibilance-aware limiter on the boost, mid/side placement, loudness-matched auditioning and a live
analyser that shows exactly what you're adding — in a UI that actually looks like 2026.

## Why it beats Fresh Air

| | Fresh Air | AETHER |
|---|---|---|
| Bands | Mid Air + High Air, fixed centres | **Presence** (1.5–8 kHz, tunable) + **Air** (6–18 kHz, tunable) |
| Harmonics | none — it's EQ | **Glow**: asymmetric soft-saturation exciter (2nd + 3rd harmonics), band-limited to the air region |
| Harshness control | none — you just back off | **Guard**: program-dependent gain reduction on the *boost only* when 5–9 kHz gets hot. Air stays; sibilance doesn't |
| Stereo | L/R only | **Focus**: sweep the air from centre (vocals) to sides (pads, choir, width) via M/S |
| Level honesty | trim knob | **Auto Gain**: RMS-matched wet/dry so "brighter" never masquerades as "better" |
| Aliasing | — | 2× / 4× oversampling (half-band polyphase IIR) around the exciter |
| Metering | none | Live in/out spectrum, boost curve overlay, Guard lamp, output peak + auto-gain readout |
| Presets | none | 10 factory presets (vocal, choir, piano, bus, mastering…) |
| UI | flat grey | Frosted-silver deck, brushed-aluminium knobs with cyan light arcs, dark glass analyser, LED meters, resizable |
| Presets | none | 10 factory + user presets saved to `Documents\Amanorsac Studio\AETHER\Presets` |
| About / legal | none | About screen with version, credits, third-party licences, legal link |

## Controls

- **PRESENCE / FREQ** — wide peak (Q 0.7), up to +9 dB. Bite and articulation.
- **AIR / FREQ** — high shelf with a soft resonant corner, up to +12 dB. Silk and sparkle.
- **GLOW** — harmonic excitation. HP → asymmetric cubic saturation → HP, blended in. Generates top end that
  isn't in the source (great on dull vocals, rolled-off samples, distant choir mics).
- **GUARD** — sidechain band-pass at 7 kHz drives an envelope (1 ms / 60 ms); above threshold the boost delta
  is scaled down. Zero = classic static EQ; 100 % = it will not let esses through.
- **FOCUS** — −100 % = boost only the Mid; +100 % = boost only the Sides; 0 = plain stereo.
- **MIX / TRIM / AUTO GAIN** — parallel blend, ±12 dB output, loudness match (±6 dB range).
- **OS** — Off / 2× / 4×. Latency is reported to the host.
- **Presets** — click the name: Factory / User / Save preset… / Show presets folder.
- **Knobs** — drag, scroll-wheel, arrow keys; double-click resets; **Shift-drag = fine**; visible keyboard focus ring.

## Company standards compliance

Built against the Amanorsac Studio Master Standard: single brand (no Aquarii Audio), Inter + JetBrains Mono bundled locally
(no runtime font fetch), colour *token roles* filled with AETHER's own palette, the shared knob contract, focus-visible states,
an About screen, and runtime data only under `Documents\Amanorsac Studio\AETHER\`.

## Licensing

AETHER follows the Amanorsac Studio License Integration Standard: the plugin is silent until
a key from *My Apps* is activated, activation is proved by an ECDSA P-256 signature verified
against the studio key compiled into the binary, and a verified proof keeps working offline
for 30 days. Deactivate is in the About screen so a buyer can move machines.

`Source/License/` holds it: `LicenseCrypto` (proof verification via CNG on Windows,
Security.framework on macOS), `LicenseClient` (device id, encrypted storage, activate,
heartbeat, deactivate) and `ActivationPanel` (the screen). `tests/license_test.cpp` runs the
shipping verification code against a known ECDSA vector — a good signature verifies, a
tampered signature or body does not, and a proof signed by any other key is refused.

## Build

Requirements: CMake ≥ 3.22, a C++17 compiler (VS 2022 / Xcode 14+ / GCC 11+). JUCE is fetched automatically
if you don't drop a checkout at `./JUCE`.

```
Windows installer (recommended):  make_installer.bat   ->  dist\AETHER_Setup_1.0.0.exe
Windows plugin only:              build.bat
macOS / Linux:                    ./build.sh
```

`make_installer.bat` configures with Visual Studio 2026 (falling back to 2022), builds the Release VST3,
runs the DSP self-test, and packages everything into one installer via `installer\build-installer.ps1`.

AETHER ships as a plugin, not an app, so the installer only places the VST3 — in
`%LOCALAPPDATA%\Programs\Common\VST3\Amanorsac Studio\`, next to the other Amanorsac Studio plugins. It is a
native Win32 executable: no .NET, no runtime dependencies, no administrator rights. It adds an Apps & Features
entry and copies itself into `%LOCALAPPDATA%\Programs\Amanorsac Studio\AETHER\` as the uninstaller.
Uninstalling removes only AETHER and keeps your presets. Command line: `/S` silent, `/VST3=<dir>` plugin folder,
`/D=<dir>` support folder, `/U` uninstall.

A standalone app is not built by default. If you ever want one, configure with `-DAETHER_BUILD_STANDALONE=ON`.

The EXE is unsigned until the company Authenticode certificate exists, so SmartScreen warns once
("More info" -> "Run anyway"). Signing it later needs no change to this project — only a
`WINDOWS_CERTIFICATE` secret and a signtool step in the workflow.

macOS is built, signed and notarised by `.github/workflows/build.yml` on an Apple runner
(`tools/sign_macos.sh`); it cannot be produced on Windows or Linux. A tagged release fails
the build if the signing secrets are missing, rather than shipping something Gatekeeper will
refuse. Signing across the portfolio is a company-level P0 in the launch audit.

## Project layout

```
CMakeLists.txt              JUCE plugin + DSP test target
Source/AetherDSP.h          Pure DSP: biquads, envelope follower, Channel, Processor
Source/PluginProcessor.*    APVTS params, oversampling, FIFOs for the analyser, presets, state
Source/PluginEditor.*       Layout, hero knobs, output meter, header/footer
Source/AetherLookAndFeel.h  Colour tokens, bundled fonts, rotary/toggle/combo/button drawing
Source/AboutPanel.h         About screen (version, credits, licences, legal)
Resources/fonts/            Inter + JetBrains Mono (SIL OFL) — embedded via juce_add_binary_data
Source/SpectrumDisplay.h    Live analyser with boost-curve overlay and Guard lamp
tests/dsp_test.cpp          DSP verification (JUCE-free)
tests/license_test.cpp      Licence verification against a known ECDSA vector
tools/sign_macos.sh         macOS signing, notarisation and stapling
tools/make_license_test_vector.py  Regenerates the licence test vector
.github/workflows/build.yml Windows + macOS CI; macOS is signed and notarised there
installer/native/           Win32 installer: AetherSetup.cpp, resources, vendored miniz, its own CMakeLists
installer/build-installer.ps1  Packs the payload and builds the installer into dist\
installer/pack_payload.py   Cross-platform payload packer (used by CI / container builds)
cmake/                      MinGW-w64 cross-compilation toolchain (container builds only)
make_installer.bat          One-click: build Release + package installer into dist\
CMakePresets.json           Visual Studio 2026 / 2022 presets
```

## Roadmap ideas
- A/B compare + undo
- "Listen" solo for the boost delta
- Per-band Guard thresholds; sidechain frequency knob
- Resizable UI already works; add a HiDPI-aware knob image cache if CPU on paint matters
