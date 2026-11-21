using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using Loupedeck;

namespace SmileHaptics;

public sealed class SmileHapticsPlugin : Plugin
{
    private const int PulsePort = 48643;
    private const string WarningEvent = "meetingWarning";
    private UdpClient? listener;
    private CancellationTokenSource? shutdown;

    public override bool UsesApplicationApiOnly => true;
    public override bool HasNoApplication => true;

    public override void Load()
    {
        try
        {
            listener = new UdpClient(AddressFamily.InterNetwork) { ExclusiveAddressUse = true };
            listener.Client.Bind(new IPEndPoint(IPAddress.Loopback, PulsePort));
        }
        catch (SocketException error)
        {
            listener?.Dispose();
            listener = null;
            Log.Error($"SMILE haptics listener could not start: {error.Message}");
            return;
        }

        PluginEvents.AddEvent(WarningEvent, "SMILE meeting warning", "A meeting warning appeared in SMILE");
        shutdown = new CancellationTokenSource();
        _ = ListenAsync(listener, shutdown.Token);
        Log.Info("SMILE haptics listener ready");
    }

    public override void Unload()
    {
        shutdown?.Cancel();
        listener?.Dispose();
        shutdown?.Dispose();
        shutdown = null;
        listener = null;
    }

    private async Task ListenAsync(UdpClient socket, CancellationToken cancellation)
    {
        long? lastPulse = null;
        try
        {
            while (!cancellation.IsCancellationRequested)
            {
                var received = await socket.ReceiveAsync(cancellation).ConfigureAwait(false);
                if (cancellation.IsCancellationRequested
                    || !received.Buffer.AsSpan().SequenceEqual("SMILE:pulse:v1"u8)
                    || (lastPulse is long previous && Stopwatch.GetElapsedTime(previous).TotalMilliseconds < 250))
                    continue;

                lastPulse = Stopwatch.GetTimestamp();
                PluginEvents.RaiseEvent(WarningEvent);
                Log.Info("SMILE haptic event raised");
            }
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { }
        catch (ObjectDisposedException) when (cancellation.IsCancellationRequested) { }
        catch (SocketException) when (cancellation.IsCancellationRequested) { }
        catch (Exception error)
        {
            Log.Error($"SMILE haptics listener stopped: {error.Message}");
        }
    }
}
