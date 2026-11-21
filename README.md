# S.M.I.L.E.

**Smart Meeting Interactive Live Education**

SMILE is a desktop companion that coaches you before, during and after high-stakes online meetings such as job interviews, client pitches and board presentations. It watches and listens through your webcam and microphone, then gives private feedback on posture, expression, pacing and speech. Only you see it.

> SMILE is a **training and learning** tool. It never alters, filters or overlays your outgoing video feed. Other participants see the real you; SMILE helps that version improve.

---

## Who it's for

Industry professionals heading into meetings where how they come across matters as much as what they say: job candidates, consultants, salespeople, executives and presenters.

**What they value:** looking and sounding professional.

---

## How it works: four phases

| Phase | When | What SMILE does |
|---|---|---|
| **1. Training** | Days or hours before | You enter the key points you want to make and a target length for each. You rehearse, and SMILE gives live cues plus a full debrief afterwards. |
| **2. Pre-Meet** | Minutes before joining | A readiness check: framing and lighting, posture, a straight collar and tidy appearance, and whether notes or other clutter are visible on camera. |
| **3. Meet** | During the call | Quiet, glanceable tips such as *"over time on point 2"*, *"straighten up"*, *"smile"*, *"you've been talking for 3 min, let others speak"*. |
| **4. Post-Meet** | After the call | A report on what went well, what didn't, and specific steps to improve, with trends across sessions. |

---

## Three pillars

1. **Training / Preparation**: rehearse against your own agenda and timing goals.
2. **Pre-Meeting Readiness**: catch visual problems before anyone else sees them.
3. **Real-Time Monitoring**: subtle in-meeting nudges that don't distract you.

---

## Core components

| Component | Phase(s) | Description |
|---|---|---|
| **Posture tracking** | Pre-Meet, Meet, Training | Pose estimation from the webcam: slouching, leaning, head tilt, fidgeting, awkward body language. |
| **Facial expression / smile** | Meet, Training | Detects smiling, engagement and eye contact (gaze toward the camera). |
| **Appearance check** | Pre-Meet | Collar and clothing alignment, framing, lighting, visible notes or distractions in the frame. |
| **Speech analysis** | Training, Post-Meet (some live) | Filler words ("uhm", "like"), speaking pace, long monologues, talk-time share and interruptions. |
| **Agenda and timing tracker** | Training, Meet | Matches what you're saying to your planned points and warns when a point runs long. |
| **LLM feedback engine** | Training, Post-Meet | Turns raw metrics and transcripts into readable, actionable coaching. |
| **Companion UI** | All | A small, Clippy-style desktop companion that sits beside your meeting window and shows tips without getting in the way. |

---

## Proposed architecture

```
 ┌────────────┐   ┌────────────┐
 │  Webcam    │   │ Microphone │        (read-only: SMILE never writes to the video feed)
 └─────┬──────┘   └─────┬──────┘
       │                │
 ┌─────▼──────────────────▼──────────────────────────────┐
 │                 C++ real-time core                     │
 │  • Capture (OpenCV)                                    │
 │  • Pose + face landmarks (e.g. MediaPipe / ONNX)       │
 │  • Smile / gaze / posture scoring                      │
 │  • Voice activity + filler-word detection              │
 │  • Session event log (timestamps, metrics)             │
 └─────┬─────────────────────────────────────┬────────────┘
       │ live cues                            │ session data
 ┌─────▼──────────────┐              ┌────────▼─────────────────────┐
 │  Companion UI      │              │  Python AI layer             │
 │  (Qt Widgets)      │              │  • Speech-to-text (Whisper)  │
 │  overlay tips      │◄─────────────┤  • Agenda/point matching     │
 └────────────────────┘   summaries  │  • LLM API for feedback      │
                                     └──────────────────────────────┘
```

**Language split**
- **C++**: anything latency-sensitive: capture, vision models, live scoring, the UI.
- **Python**: transcription, NLP, and LLM API calls for training and post-meet reports.
- The two communicate over a local IPC channel such as ZeroMQ, gRPC or a local socket.

**Privacy by design**
- All video and audio processing runs locally by default.
- Only text (transcripts and metric summaries) goes to an LLM API, and only when the user opts in.
- Recordings are never stored unless the user asks.

---

## Example live tips

- ⏱️ "Point 2 is 90 s over its target. Consider wrapping up."
- 🧍 "You're slouching. Sit up straight."
- 🙂 "Try a smile. You've looked tense for a while."
- 🗣️ "You've spoken for 4 minutes straight. Pause for questions."
- 👀 "Look at the camera, not the screen."
- 🔁 "5 'uhms' in the last minute."

---

## Roadmap (draft)

- [x] App skeleton: main window, four phases, companion, agenda timing, CI builds
- [ ] **MVP**: webcam posture and smile detection with a basic companion overlay
- [ ] Pre-Meet readiness checklist (framing, lighting, collar, visible notes)
- [ ] Speech pipeline: filler words, pace, talk-time
- [ ] Training mode: agenda input, per-point timing, post-rehearsal LLM report
- [ ] Live Meet mode with low-distraction tips
- [ ] Post-Meet report and progress history
- [ ] Settings: tip sensitivity, quiet mode, per-meeting profiles

---

## Status

🚧 **App skeleton (v0.1.0).** The main window, the four phase pages and the floating companion all work. Agenda timing is real. There's no camera, microphone or AI yet: posture, smile and speech tips come from an optional **demo** source.

What works today:
- **Training**: edit your agenda (points + target minutes), rehearse with a live per-point timer, and get timing tips ("30 s left", "time's up", "2:00 over").
- **Pre-Meet**: a readiness checklist you tick by hand. The automatic camera check is a disabled placeholder.
- **Meet**: run the agenda during a real call, and choose which tip categories the companion may show.
- **Post-Meet**: a report with timing per point, what went well, what to improve and next steps.
- **Companion**: a frameless, always-on-top smiley with a speech bubble. Its expression changes with each tip. Drag it to move it, click it to dismiss a tip, double-click it to open SMILE, or right-click it for a menu.

---

## Download

Every push to `main` builds Linux, Windows and macOS packages in GitHub Actions (see the **Actions** tab → latest run → *Artifacts*). Tagged versions (`v*`) are also published under **Releases**:

| Platform | File | Notes |
|---|---|---|
| Linux | `SMILE-linux-x86_64.AppImage` | `chmod +x` it, then run it. |
| Windows | `SMILE-windows-x64.zip` | Unzip and run `SMILE.exe`. |
| macOS | `SMILE-macos-universal.dmg` | Not notarised yet: the first time, right-click the app → **Open**. |

---

## Building from source

Requirements: **CMake ≥ 3.21**, a C++17 compiler, and **Qt 6.5 or newer** (the CI uses Qt 6.8 LTS).

```bash
# Debian / Ubuntu / Kali
sudo apt install cmake ninja-build qt6-base-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/SMILE
```

If Qt comes from the online installer or [aqtinstall](https://github.com/miurahr/aqtinstall), point CMake at it with `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64` (or `msvc2022_64`, `macos`).

---

## Code layout

```
src/
├── main.cpp
├── core/                  # Session logic, no UI. AI modules plug in here.
│   ├── Agenda.h           # AgendaPoint: title, target time, actual time
│   ├── Tip.h/.cpp         # Tip: category, severity, text, source
│   ├── TipSource.h        # ⭐ Interface every AI module implements
│   ├── DemoTipSource.*    # Placeholder that emits sample tips
│   ├── SessionController.*# Runs a session: timing, timing tips, report
│   └── SessionReport.h    # Everything a session produced
├── companion/
│   ├── CompanionWindow.*  # Floating always-on-top coach
│   └── SmileyPainter.*    # Draws the character and the app icon
└── app/
    ├── MainWindow.*       # Sidebar + pages, menus, wiring
    ├── SessionRunWidget.* # Live timer view shared by Training and Meet
    └── pages/             # TrainingPage, PreMeetPage, MeetPage, PostMeetPage
packaging/linux/           # .desktop file and icon for the AppImage
.github/workflows/build.yml# CI: builds and packages all three platforms
```

### Adding an AI module

1. Subclass `TipSource` (for example `PostureTipSource`), implement `start()`/`stop()`, and `emit tipRaised(tip)` whenever the model spots something.
2. Register it in `MainWindow`'s constructor with `m_controller->addTipSource(new PostureTipSource);`.

That's all. Its tips then appear on the companion (if that category is switched on) and in the Post-Meet report. Heavy models should run on a worker thread or in a separate Python process, and only emit tips back to the UI thread.

---

<sub>*S.M.I.L.E.: because being Supercalifragilisticexpialidocious in meetings takes practice.*</sub>
