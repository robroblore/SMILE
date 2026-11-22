# S.M.I.L.E.

**Smart Meeting Interactive Live Education**

SMILE is a desktop coach that helps you **correct your body behaviour in online meetings** such as job interviews, client pitches and presentations. It watches and listens through your webcam and microphone, coaches you while you train, checks your setup before a call, gives a few quiet nudges during it, and scores you afterwards. Only you see it.

> SMILE only reads your camera and microphone. It never alters, filters or overlays your outgoing video, never joins the meeting as a bot, and never records to disk.

**Download:** Windows (`.zip`), macOS (universal `.dmg`) and Linux (`.AppImage`) builds are attached to every [GitHub release](../../releases). See [Installation and setup](#installation-and-setup).

<p align="center"><img src="docs/screenshots/companion.png" alt="The SMILE companion with three live tips: face covered, smile more, pause for questions" width="760"></p>

| Home | Mock meeting |
|---|---|
| ![Home page with four big buttons, level, streak, score chart and badges](docs/screenshots/home.png) | ![Mock meeting: the SMILE avatar, your camera, live feedback and the chat](docs/screenshots/mock-meeting.png) |
| **Results** | **Meet** |
| ![Results: overall score, improvements, breakdown and change since last time](docs/screenshots/results.png) | ![Meet page: start a meeting, mute, section timer and how picky SMILE is](docs/screenshots/meet.png) |

---

## Built with our partners

### Logitech: coaching you can feel, and a camera that thinks

SMILE is designed around Logitech hardware, not just compatible with it:

* **[MX Master 4](https://www.logitech.com/en-us/shop/p/mx-master-4) haptic feedback (implemented).** When a live tip pops up, your mouse pulses, so you notice it even while looking at your slides or at the other person, without reading anything. The pulse repeats while the problem lasts (for example while your face stays covered) and stops the moment you fix it. Intensity, speed and duration are set in **Settings → Mouse haptics**. It runs through our own plugin for [Logi Options+](https://www.logitech.com/en-us/software/logi-options-plus) ([setup below](#logitech-mouse-haptics-mx-master-4)).
* **Logitech webcams (in progress).** SMILE already supports the Logitech LogiInsight camera, including its infrared feed in the debug view ([details](#logitech-logiinsight-webcam)). The next step is to run SMILE's face tracking **on the camera itself** instead of on your computer: lower CPU use, faster reactions, and your video never needs to be processed by the PC at all. See [Logitech webcams](https://www.logitech.com/en-us/shop/c/webcams).

### Schole AI: a companion, not a chatbot

SMILE is deliberately **not a chatbot**. There's no text box to ask questions and no wall of generated text. Instead, a small, friendly animated companion **teaches you how to come across professionally by reacting to what you actually do**: it pops up for a moment when you slouch, cover your face or forget to smile, then gets out of the way. You learn by practising (rehearsals, a mock meeting with SMILE as the other person) and by seeing your own progress, the way a coach sitting next to you would teach, not by reading answers.

---

## What SMILE focuses on

Live, SMILE only talks about how you come across on camera, in this order of importance:

1. **Face covered or out of frame**: a hand over your face, leaning out of the picture.
2. **Posture and movement**: slouching, a tilted head, fidgeting, distracting hands.
3. **Eye contact**: looking away from the camera, or reading for too long.
4. **Filler words**: "um", "uh", "you know".
5. **Smile and expression**: no smile for a long time, or a tense look.
6. **Holding the floor**: talking for about 3 minutes without a break for others: *"Pause and ask for questions."*

Everything else (timing, key points, pace, wording, lighting, clothing) never pops up during a session. It is measured and waits for your results.

---

## The app

| Page | What it's for |
|---|---|
| **Home** | Four big buttons (Practise, Check my setup, Start a meeting, My results) plus your progress: level and XP, day streak, last and best score, a score chart, badges and recent sessions. |
| **Training** | **Rehearse** your whole talk against your plan; practise **one section** at a time; or run a **mock meeting** with SMILE as the other person (presentation, interview, negotiation, business proposal, sales pitch). |
| **Setup check** | A live camera preview and a one-click AI check of framing, camera angle, lighting, collar, posture and clutter, with red boxes on what to fix. A live sound check tells you if there's background noise. |
| **Meet** | Start a real meeting. SMILE minimizes; the small animated companion pops up only when something about your body language needs fixing. |
| **Results** | Your score, the three most important things to work on, a breakdown you can expand into exact measurements and timestamped moments, and the change since last time. Every earlier session is listed below and can be reopened to compare. |
| **Settings** | AI key, how picky SMILE is (separately for meetings and training), AI intelligence, camera and microphone, mouse haptics, appearance and debug mode. |

### Logitech LogiInsight webcam

Connect the webcam over USB and choose **Logitech LogiInsight (color)** in **Settings → Camera**. **Check my setup** shows the entire camera frame. In **Settings → Debugging → Open debug view**, select **Show taller view** for the full monochrome image. The button is available while SMILE is using the Logitech color camera and its infrared feed is connected. The preview closes when you switch cameras or close the debug view.

### Training

* **Rehearse**: paste your speech or notes. SMILE splits them into sections with key points, you adjust the sections and target times, then rehearse out loud. Training is graded more strictly than real meetings and gives more feedback.
* **One section**: practise a single section with your notes in view, get a score and try again, compared with your last attempt.
* **Mock meeting**: a video-call layout with the SMILE avatar next to your own camera. The avatar plays a role, listens, answers within a few seconds of you finishing, and asks follow-up questions. Behaviour feedback appears under your video, and **"SMILE steps in"** (Never / Light / Balanced / Strict) sets how often the avatar itself comments on a covered face, fidgeting or slouching. A banner warns you when it's too noisy around you. Headphones work best.

SMILE follows which section you're on by **meaning**, not exact words, and judges key points leniently on the idea. If you drift off your plan it notes it for the results.

### During a meeting

* There's no long quiet time between pop-ups: a tip shows as soon as its problem lasts long enough. Each kind of tip then waits before it repeats, so nothing nags.
* **How picky SMILE is** sets how long a problem must last: at the middle setting, 2 s of a covered face, 6 s looking away, 40 s without a smile, 3 minutes talking without a break. At the strictest, the smile reminder comes every few seconds.
* The most urgent tip always wins: a hidden face replaces anything else on screen.
* "Talking too long" counts only real turns: short pauses between sentences don't reset it, a few seconds of silence (someone else speaking) does, and it reminds you at most once per 3 minutes.
* Right-click the companion to **snooze tips for 10 minutes** or **mute all tips** until you unmute them. Mute is also in the tray menu and on the Meet page. Muted sessions are still scored.
* The companion is always on top but only shows its face area to the mouse, so it doesn't block clicks on your meeting app. Drag it anywhere.
* An optional section timer can sit next to it (Meet → Options).
* With an MX Master 4 and our Logi Options+ plugin, every pop-up also pulses the mouse ([setup](#logitech-mouse-haptics-mx-master-4)).

### Scoring

The overall score weighs how you come across most:

| Category | Weight |
|---|---|
| Face & framing (visible, not covered, steady) | 2.0 |
| Body language (posture, movement, hands) | 2.0 |
| Facial engagement (warmth; a calm, attentive face counts as good) | 1.5 |
| Eye contact | 1.5 |
| Speech delivery (pace, filler words, pauses, volume) | 1.25 |
| Professional language | 1.0 |
| Key points | 0.6 |
| Timing | 0.5 |

A covered face or disturbing movement also caps the overall score, so good timing can't make up for it. Training sessions need a higher score for the same rating.

---

## How SMILE watches and listens

| | On your computer | In the cloud (Together AI) |
|---|---|---|
| Camera | YuNet face tracker (~15×/s): position, landmarks, head direction. FER+ expression model (~5×/s): smile and expression. | Vision model every 2 s (adjustable to 1 s) and immediately when the tracker sees a change: posture, hands, covered face, tension. |
| Voice | Speech detection tuned to ignore background noise, talk time, pauses, speaking rate, noise level. | Whisper: exact words, filler words, words per minute. |

The cloud results calibrate the local ones as you go. **Debug mode** (Settings) shows the tracking points, live values, raw AI answers and the reason behind every tip.

---

## Installation and setup

### 1. Install SMILE

Download the latest build from the [releases page](../../releases):

| System | File | How to start it |
|---|---|---|
| Windows | `SMILE-windows-x64.zip` | Unzip it anywhere and run `SMILE.exe`. |
| macOS | `SMILE-macos-universal.dmg` | Open it and drag SMILE to Applications. The first time, right-click SMILE and choose **Open** (the app is not notarised yet). Allow camera and microphone access when asked. |
| Linux | `SMILE-linux-x86_64.AppImage` | `chmod +x SMILE-linux-x86_64.AppImage`, then run it. |

### 2. Add your AI key

SMILE's smart features (posture, covered face, filler words, the setup check, the mock meeting and written feedback) use [Together AI](https://together.ai). Face tracking, expression, timing and speech rhythm work without a key.

1. Create an account at [together.ai](https://together.ai) and create an API key in your Together dashboard.
2. In SMILE, open **Settings → AI coach**, paste the key and click **Save**.
3. Click **Test connection**. A green message means you're ready.

The key is stored only on your computer and is **never** built into the app or committed to git. Developers can also provide it, in this order of priority after the app setting, through the `TOGETHER_API_KEY` environment variable or a `.env` file next to the app (copy `.env.example` to `.env`; `.env` is git-ignored).

### 3. Check your setup

Open **Setup check**, click **Check my setup**, and fix anything marked in red. The sound check tells you if your room is too noisy. Then start practising in **Training**.

### Logitech mouse haptics (MX Master 4)

Optional: let your [MX Master 4](https://www.logitech.com/en-us/shop/p/mx-master-4) vibrate with every live tip.

1. Install [Logi Options+](https://www.logitech.com/en-us/software/logi-options-plus) (with Logi Plugin Service 6.4 or newer) and the .NET 10 SDK.
2. In [`integrations/logi-haptics`](integrations/logi-haptics), run `dotnet build -c Release`.
3. Create a text file `SmileHaptics.link` containing the full path of that folder's `build/Release` directory, and put it in `%LOCALAPPDATA%\Logi\LogiPluginService\Plugins` (Windows) or `~/Library/Application Support/Logi/LogiPluginService/Plugins` (macOS).
4. Restart Logi Plugin Service from the Logi Options+ settings, and enable haptics for **SMILE Haptics** in the MX Master 4 settings if needed.
5. In SMILE, turn on **Settings → Mouse haptics → Mouse vibration** and choose intensity, speed and duration.

SMILE talks to the plugin only on your own computer (`127.0.0.1`); the plugin accepts no outside traffic. Full details are in the [plugin README](integrations/logi-haptics/README.md).

### AI intelligence and models

Choose in **Settings → Coaching → AI intelligence**:

| Level | Models | Cost |
|---|---|---|
| Standard (default) | `nvidia/NVIDIA-Nemotron-3.5-Super-VL` (camera), `openai/gpt-oss-120b` (text) | about $0.15–0.25 per hour |
| Enhanced | `moonshotai/Kimi-K3` | ~5× |
| Maximum | `moonshotai/Kimi-K3.1-Pro` | ~15× |

Speech always uses `openai/whisper-large-v3`, the mock meeting's conversation always uses the fast `gpt-oss-120b`, the avatar speaks with `hexgrad/Kokoro-82M`, and the setup check always uses the quick vision model so it answers in a second or two.

---

## Future extensions: a coach that adapts to you

Our main focus is **adaptability**: SMILE should learn who you are, where you present and what you need, instead of applying one rulebook to everyone.

* **Remember your habits.** Keep a private, on-device memory of *your* recurring tics (touching your face, looking down when nervous, saying "basically") and coach those first, with exercises aimed at them.
* **Learn from you.** Train the on-device models on your own sessions, so "your calm face", "your normal posture" and "your voice" are recognised more accurately over time.
* **Cultures and traditions.** Coaching norms differ: some cultures consider frequent smiling or direct eye contact rude, others expect them. Let users pick (or SMILE infer) the cultural context of a meeting and adjust what is coached and how strictly.
* **More languages.** Filler words, hedging and professional wording in many languages, not just English.
* **More body language.** Hand gestures, shoulder tension, nodding, blinking, leaning, posture over time, and how you react while others speak.
* **More speech tics.** Up-talk, rushed sentence endings, monotone delivery, repeated phrases and interruptions.
* **Feedback on the content.** Beyond delivery: clarity of the message, structure, strength of arguments and answers to likely questions, based on your plan and what you actually said.
* **Better and faster models.** Run more of the analysis on-device (and on the [Logitech webcam](https://www.logitech.com/en-us/shop/c/webcams) itself) for instant, private feedback, and use stronger models for the reviews as they become affordable.
* **Rewards that keep you going.** Grow the levels, streaks and badges into personal goals, challenges and milestones tied to the habits you are working on.
* **Hardware feedback.** Richer haptic patterns on the MX Master 4 so different problems feel different, and quiet LED or camera-light cues.

---

## Privacy

* Video and audio are processed on your computer. While a session runs, a small snapshot every couple of seconds and short speech clips are sent to Together AI over HTTPS for analysis. Nothing is stored in the cloud. Switch **Analyse with AI** off in Settings to keep everything local.
* Nothing is recorded to disk. Session results (scores and the written report, no video or audio) are saved on your computer only, and **Results → Delete history** removes them.
* There are no accounts.

---

## Building from source

Requirements: CMake 3.21+, a C++17 compiler, Qt 6.8+ (Widgets, Multimedia, Network) and Python 3.

```bash
python3 scripts/fetch_onnxruntime.py
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/SMILE
```

ONNX Runtime is loaded at run time, so SMILE still starts without it (without on-device tracking). The FER+ expression model is downloaded and checked by CMake at configure time. GitHub Actions builds and publishes all three platforms when a `vX.Y.Z` tag is pushed.

Handy environment variables: `SMILE_THEME=light|dark`, `SMILE_COMPANION_INPROCESS=1` (run the companion inside the main process), `SMILE_MUTE_AUDIO=1` (silent avatar, for demos and tests).

### Layout

```
src/app         main window, pages (Home, Training, Setup check, Meet, Results, Settings), session history
src/companion   the floating companion (separate process) and its avatar
src/core        session controller, settings, tips, reports
src/sensors     camera and microphone
src/vision      on-device face tracking and expression model
src/ai          Together AI client, vision and speech coaching, mock meeting, key-point judge
src/ui          theme and widgets
integrations    Logi Options+ plugin for mouse haptics
```

---

<sub>*S.M.I.L.E.: because being Supercalifragilisticexpialidocious in meetings takes practice.*</sub>
