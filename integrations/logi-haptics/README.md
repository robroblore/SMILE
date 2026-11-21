# SMILE Haptics

Requires an MX Master 4, Logi Options+ with Logi Plugin Service 6.4 or newer, and the .NET 10 SDK for building the plugin.

1. Run `dotnet build -c Release` in this folder.
2. Create a plain-text file named `SmileHaptics.link` containing the absolute path to this folder's `build/Release` directory. Put it in `%LOCALAPPDATA%\Logi\LogiPluginService\Plugins` on Windows or `~/Library/Application Support/Logi/LogiPluginService/Plugins` on macOS.
3. In Logi Options+ settings, restart Logi Plugin Service. Enable haptics for SMILE Haptics in the MX Master 4 settings if needed.
4. In SMILE, turn on **Settings → Debugging → Play mouse haptics**. Start a meeting with a short timed agenda item and let it run over. Appearance tips take priority over the Time's up hint if they compete. Expect pulses while a warning is visible, and check that they stop when it closes, you advance, or the meeting ends. No Actions Ring binding is required.
5. Set **Vibration duration** to **Until resolved** (the default) or a limit of 1–120 seconds. **Vibration intensity** selects a gentle tap, the original sharp pulse, or a multi-tap effect. **Vibration speed** sets the time between repeat requests to 3 seconds, 1 second (default), or half a second. Warnings such as a word already spoken receive one pulse because they have no ongoing condition to track.

SMILE sends a fixed pulse message (optionally suffixed with `:low` or `:medium`) to the plugin over UDP on `127.0.0.1:48643`. The plugin accepts no remote traffic and limits pulses to one every 250 ms. SMILE's debug entry confirms that a request was sent. The plugin's log confirms that it raised the event. Neither can confirm that the mouse physically vibrated.
