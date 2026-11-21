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

## Four pillars

1. **Training / Preparation**: rehearse against your own agenda and timing goals.
2. **Pre-Meeting Readiness**: catch visual problems before anyone else sees them.
3. **Real-Time Monitoring**: subtle in-meeting nudges that don't distract you.
4. **Post-Meeting Analysis**: generate a clear and to-the-point report of things to improve for next time.

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
 │  (Qt / ImGui)      │              │  • Speech-to-text (Whisper)  │
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

## How SMILE watches and listens

SMILE uses **two AIs in tandem**:

| | Local (on your computer) | Cloud (Together AI) |
|---|---|---|
| Camera | YuNet face tracker, ~15×/s, ~7 ms per frame: face position, 5 landmarks, head direction (eye contact), mouth shape | Vision model every 3 s **and immediately when the tracker sees a change**: smile, expression, posture, tension, collar, clutter |
| Voice | Speech detection, talk time, monologues, speaking rate from syllables, twice a second | Whisper on ~10 s clips: exact words, filler words, true words per minute |

The cloud answers also calibrate the local estimates to your face and voice as you go. Tips fire only when a pattern lasts (for example, no smile for 20 s), each kind has a cooldown, and **Settings → Coaching** offers Relaxed, Balanced and Responsive presets.

**Debug mode** (Settings → Debugging) shows the face landmarks on the camera, live values for everything detected, the cloud AI's raw answers, and a log of every alert and tip with the exact reason it fired.

**Training** turns your speech or notes into sections with headings that fit the kind of talk, plus the key points SMILE then listens for. If you move on without saying one, SMILE tells you, and the report rates how many you got across.

---

## AI coach setup

SMILE's smart features (smile, posture and eye-contact tips, filler-word counting, the Pre-Meet camera check and written feedback) use [Together AI](https://together.ai). Timing, lighting and monologue detection work without it.

The key is **never** built into the app or committed to git. SMILE looks for it in this order:

1. **Settings → AI coach**: paste it in the app. It's stored only on that computer.
2. The `TOGETHER_API_KEY` environment variable.
3. A `.env` file in the working directory or next to the app (copy `.env.example` to `.env`). `.env` is git-ignored.

Models used: `nvidia/NVIDIA-Nemotron-3.5-Super-VL` (camera), `openai/whisper-large-v3` (speech) and `openai/gpt-oss-120b` (structuring and feedback). Typical cost is about 25 cents per hour of coaching.

**Privacy:** while a session runs, a small snapshot is sent every 3 s or so (adjustable) and speech clips every ~10 s, over HTTPS. Nothing is recorded to disk, and SMILE never changes your video feed. Switch **Analyse with AI** off in Settings to keep everything local.

---

## Roadmap (draft)

- [ ] **MVP**: webcam posture and smile detection with a basic companion overlay
- [ ] Pre-Meet readiness checklist (framing, lighting, collar, visible notes)
- [ ] Speech pipeline: filler words, pace, talk-time
- [ ] Training mode: agenda input, per-point timing, post-rehearsal LLM report
- [ ] Live Meet mode with low-distraction tips
- [ ] Post-Meet report and progress history
- [ ] Settings: tip sensitivity, quiet mode, per-meeting profiles

---

## Status

🚧 Concept / early design. No code yet.

---

<sub>*S.M.I.L.E.: because being Supercalifragilisticexpialidocious in meetings takes practice.*</sub>
