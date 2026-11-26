#include "MicBridge.h"

#if JUCE_MAC
 #include <CoreAudio/CoreAudio.h>

// Ask CoreAudio how a device is connected (Bluetooth / USB / built-in). Returns 0 if unknown.
static UInt32 macTransportTypeForName (const juce::String& name)
{
    AudioObjectPropertyAddress pa { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize (kAudioObjectSystemObject, &pa, 0, nullptr, &size) != noErr || size == 0)
        return 0;
    std::vector<AudioDeviceID> ids (size / sizeof (AudioDeviceID));
    if (AudioObjectGetPropertyData (kAudioObjectSystemObject, &pa, 0, nullptr, &size, ids.data()) != noErr)
        return 0;

    for (auto id : ids)
    {
        CFStringRef cfName = nullptr;
        UInt32 sz = sizeof (cfName);
        AudioObjectPropertyAddress np { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        if (AudioObjectGetPropertyData (id, &np, 0, nullptr, &sz, &cfName) != noErr || cfName == nullptr)
            continue;
        const auto devName = juce::String::fromCFString (cfName);
        CFRelease (cfName);
        if (devName.trim() != name.trim())
            continue;

        UInt32 transport = 0;
        sz = sizeof (transport);
        AudioObjectPropertyAddress tp { kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        if (AudioObjectGetPropertyData (id, &tp, 0, nullptr, &sz, &transport) == noErr)
            return transport;
    }
    return 0;
}
#endif

static bool nameLooksBluetooth (const juce::String& n)
{
    const auto s = n.toLowerCase();
    for (auto* key : { "bluetooth", "airpods", "beats", "hands-free", "handsfree", "headset", "buds", "bluez",
                       "jabra", "plantronics", "poly ", "bose", "wh-1000", "wf-1000", "sony wh", "jbl", "skullcandy",
                       "a2dp", "hfp", " bt", "bt-", "(bt", "shokz", "aftershokz", "soundcore" })
        if (s.contains (key)) return true;
    return false;
}

//==============================================================================
MicBridge::MicBridge()
{
    juce::AudioDeviceManager factory;
    juce::OwnedArray<juce::AudioIODeviceType> all;
    factory.createAudioDeviceTypes (all);

    // Keep the shared-mode system APIs only. They coexist with whatever driver the DAW is using
    // (ASIO allows one driver per process, so it is never opened here).
    const juce::StringArray keep { "CoreAudio", "Windows Audio", "ALSA", "JACK" };
    while (all.size() > 0)
    {
        std::unique_ptr<juce::AudioIODeviceType> t (all.removeAndReturn (0));
        if (keep.contains (t->getTypeName()))
        {
            t->addListener (this);
            types.add (t.release());
        }
    }
    // fallback: if none of the above exist (unusual setups), take DirectSound
    if (types.isEmpty())
    {
        factory.createAudioDeviceTypes (all);
        for (int i = all.size(); --i >= 0;)
            if (all[i]->getTypeName() == "DirectSound") { all[i]->addListener (this); types.add (all.removeAndReturn (i)); }
    }
    refreshDevices();
}

MicBridge::~MicBridge()
{
    stopTimer();
    closeDevice();
    for (auto* t : types) t->removeListener (this);
}

MicBridge::Device MicBridge::describe (juce::AudioIODeviceType& t, const juce::String& name, bool isDefault) const
{
    Device d;
    d.type = t.getTypeName();
    d.name = name;
    d.isDefault = isDefault;
    const auto low = name.toLowerCase();

   #if JUCE_MAC
    const auto tr = macTransportTypeForName (name);
    d.bluetooth = (tr == kAudioDeviceTransportTypeBluetooth || tr == kAudioDeviceTransportTypeBluetoothLE);
    d.usb       = (tr == kAudioDeviceTransportTypeUSB);
    d.builtIn   = (tr == kAudioDeviceTransportTypeBuiltIn);
    if (tr == 0) d.bluetooth = nameLooksBluetooth (name);
   #else
    d.bluetooth = nameLooksBluetooth (name);
    d.usb       = low.contains ("usb");
    d.builtIn   = low.contains ("built-in") || low.contains ("internal") || low.contains ("realtek")
                  || low.contains ("microphone array") || low.contains ("hda intel");
   #endif
    d.wireless = d.bluetooth || low.contains ("iphone") || low.contains ("continuity") || low.contains ("wireless");
    return d;
}

void MicBridge::refreshDevices()
{
    devices.clearQuick();
    for (auto* t : types)
    {
        t->scanForDevices();
        const auto names = t->getDeviceNames (true);
        const int def = t->getDefaultDeviceIndex (true);
        for (int i = 0; i < names.size(); ++i)
        {
            // ALSA lists raw hardware nodes twice; skip obvious duplicates and loopbacks
            if (names[i].containsIgnoreCase ("loopback") || names[i].containsIgnoreCase ("monitor of")) continue;
            devices.add (describe (*t, names[i], i == def));
        }
    }
}

void MicBridge::audioDeviceListChanged()
{
    refreshDevices();
    // the open device vanished? (unplugged, Bluetooth turned off)
    if (device != nullptr)
    {
        bool stillThere = false;
        for (auto& d : devices) if (d.name == openName) stillThere = true;
        if (! stillThere)
        {
            closeDevice();
            state = State::Missing;
            status = "Lost " + openName + ". Waiting for it to come back...";
        }
    }
    if (wanted && device == nullptr) retryCountdown = 0;   // try again right away
    notify();
}

void MicBridge::setWanted (const juce::String& type, const juce::String& name, int channel)
{
    wantedType = type; wantedName = name;
    channelMode.store (channel);
    wanted = true;
    tryOpen();
    startTimer (1000);
    notify();
}

void MicBridge::close()
{
    wanted = false;
    stopTimer();
    closeDevice();
    state = State::Off;
    status = {}; warning = {};
    notify();
}

void MicBridge::timerCallback()
{
    if (errorFlag.exchange (false) && device != nullptr)
    {
        closeDevice();
        state = State::Missing;
        status = "The microphone stopped. Reconnecting...";
        retryCountdown = 0;
        notify();
    }
    if (wanted && device == nullptr)
    {
        if (--retryCountdown <= 0)
        {
            retryCountdown = 2;   // every ~2 s
            refreshDevices();
            tryOpen();
            notify();
        }
    }
}

void MicBridge::closeDevice()
{
    if (device != nullptr)
    {
        device->stop();
        device->close();
        device.reset();
    }
    running.store (false);
    openName = {}; openWireless = false; channelNames.clear();
}

bool MicBridge::tryOpen()
{
    closeDevice();
    warning = {};

    // macOS: the app hosting us needs microphone permission, otherwise CoreAudio just delivers silence
    if (! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio, [] (bool) {});
       #if JUCE_MAC
        state = State::Denied;
        status = "Microphone access is off for this app. Open System Settings > Privacy & Security > Microphone and switch it on for "
                 + juce::File::getSpecialLocation (juce::File::hostApplicationPath).getFileNameWithoutExtension() + ".";
        return false;
       #endif
    }

    // find the device
    const Device* pick = nullptr;
    for (auto& d : devices)
    {
        const bool typeOk = wantedType.isEmpty() || d.type == wantedType;
        if (wantedName.isEmpty() ? (d.isDefault && typeOk) : (d.name == wantedName && typeOk)) { pick = &d; break; }
    }
    if (pick == nullptr && wantedName.isEmpty())
        for (auto& d : devices) { pick = &d; break; }   // no default reported: take the first input
    if (pick == nullptr)
    {
        state = State::Missing;
        status = wantedName.isEmpty() ? "No microphone found. Plug one in or pair a Bluetooth mic."
                                      : "Waiting for " + wantedName + " (plug it in or turn it on)...";
        return false;
    }

    juce::AudioIODeviceType* type = nullptr;
    for (auto* t : types) if (t->getTypeName() == pick->type) type = t;
    if (type == nullptr) { state = State::Failed; status = "Audio system not available."; return false; }

    std::unique_ptr<juce::AudioIODevice> d (type->createDevice ({}, pick->name));
    if (d == nullptr) { state = State::Failed; status = "Couldn't open " + pick->name + "."; return false; }

    // sample rate: 48k or 44.1k if offered, otherwise the best the device has (Bluetooth: 16/24 kHz)
    const auto rates = d->getAvailableSampleRates();
    double rate = -1.0;
    for (double want : { 48000.0, 44100.0 })
        for (auto r : rates) if (rate < 0 && std::abs (r - want) < 1.0) rate = r;
    if (rate < 0) for (auto r : rates) if (r <= 96000.0) rate = juce::jmax (rate, r);
    if (rate < 0 && ! rates.isEmpty()) rate = rates[0];
    if (rate < 0) rate = 48000.0;

    const int numIn = juce::jmax (1, d->getInputChannelNames().size());
    juce::BigInteger ins; ins.setRange (0, juce::jmin (numIn, 32), true);
    int buf = d->getDefaultBufferSize();
    for (auto b : d->getAvailableBufferSizes()) if (b >= 128 && b <= 512) { buf = b; break; }

    const auto err = d->open (ins, {}, rate, buf);
    if (err.isNotEmpty())
    {
        state = State::Failed;
        status = "Couldn't open " + pick->name + ": " + err;
       #if JUCE_WINDOWS
        status << "  If Windows is blocking the mic: Settings > Privacy & security > Microphone > allow desktop apps.";
       #endif
        return false;
    }

    micRate.store (d->getCurrentSampleRate());
    micBlock.store (d->getCurrentBufferSizeSamples());
    channelNames = d->getInputChannelNames();
    for (auto& e : chanEnv) e = 0;
    generation.fetch_add (1);
    openName = pick->name;
    openWireless = pick->wireless;
    wirelessOpen.store (pick->wireless);
    device = std::move (d);
    device->start (this);

    state = State::Running;
    const double ms = 1000.0 * (device->getInputLatencyInSamples() + 2.5 * device->getCurrentBufferSizeSamples())
                      / juce::jmax (1.0, device->getCurrentSampleRate());
    status = openName + "  |  " + juce::String (device->getCurrentSampleRate() / 1000.0, 1) + " kHz  |  ~"
             + juce::String (juce::roundToInt (ms)) + " ms";
    if (pick->wireless)
        warning = "Wireless mic: expect about 0.15-0.25 s of extra delay, and Bluetooth headphones switch to "
                  "call quality while their mic is in use. Great for practice; use a wired or USB mic on stage.";
    return true;
}

//==============================================================================
void MicBridge::audioDeviceAboutToStart (juce::AudioIODevice* d)
{
    micRate.store (d->getCurrentSampleRate());
    micBlock.store (d->getCurrentBufferSizeSamples());
    running.store (true);
}

void MicBridge::audioDeviceStopped()                 { running.store (false); }
void MicBridge::audioDeviceError (const juce::String&) { errorFlag.store (true); }

void MicBridge::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                  int n, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c) if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], n);
    if (n <= 0) return;

    const int chans = juce::jmin (numIn, 32);
    int mode = channelMode.load();
    int sel = activeChannel.load();

    if (mode == kAutoChannel && chans > 1)
    {
        // follow the loudest input, with 6 dB of hysteresis so it doesn't flicker
        for (int c = 0; c < chans; ++c)
        {
            float pk = in[c] != nullptr ? juce::FloatVectorOperations::findMaximum (in[c], n) : 0.0f;
            if (in[c] != nullptr) pk = juce::jmax (pk, -juce::FloatVectorOperations::findMinimum (in[c], n));
            chanEnv[c] = juce::jmax (pk, chanEnv[c] * 0.97f);
        }
        int best = 0;
        for (int c = 1; c < chans; ++c) if (chanEnv[c] > chanEnv[best]) best = c;
        if (sel >= chans || (best != sel && chanEnv[best] > 2.0f * chanEnv[sel] && chanEnv[best] > 0.003f)) sel = best;
        activeChannel.store (sel);
    }
    else if (mode >= 0) { sel = juce::jlimit (0, juce::jmax (0, chans - 1), mode); activeChannel.store (sel); }
    else if (chans <= 1) sel = 0;

    auto w = writeIdx.load (std::memory_order_relaxed);
    const auto r = readIdx.load (std::memory_order_acquire);
    const bool room = (w - r) + (juce::uint64) n <= (juce::uint64) kRingSize;
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        if (mode == kMixChannels && chans > 1)
        {
            for (int c = 0; c < chans; ++c) if (in[c] != nullptr) s += in[c][i];
            s /= (float) chans;
        }
        else if (chans > 0 && in[sel] != nullptr) s = in[sel][i];
        pk = juce::jmax (pk, std::abs (s));
        if (room) ring[(size_t) ((w + (juce::uint64) i) & (kRingSize - 1))] = s;
    }
    if (room) writeIdx.store (w + (juce::uint64) n, std::memory_order_release);
    float old = peak.load();
    while (pk > old && ! peak.compare_exchange_weak (old, pk)) {}
}

//==============================================================================
void MicBridge::read (float* dest, int n)
{
    // never jump to silence (that's a click): glide the last sample down to zero
    auto silence = [&]
    {
        for (int i = 0; i < n; ++i) { lastOut *= 0.995f; dest[i] = lastOut; }
        fadeIn = 0.0f;
    };
    const auto gen = generation.load (std::memory_order_acquire);
    if (gen != seenGen)
    {
        seenGen = gen; primed = false; frac = 0; corr = 0;
        for (auto& h : hist) h = 0;
        readIdx.store (writeIdx.load (std::memory_order_acquire), std::memory_order_release);
    }
    const double mr = micRate.load(), hr = hostRate.load();
    if (! running.load() || mr <= 0 || hr <= 0) { silence(); primed = false; return; }

    const double base = mr / hr;
    fadeStep = (float) (1.0 / (0.005 * hr));
    const auto w = writeIdx.load (std::memory_order_acquire);
    auto r = readIdx.load (std::memory_order_relaxed);
    const double fill = (double) (w - r);
    // wireless/Bluetooth devices deliver audio in bursts: give them a deeper cushion
    const double cushion = wirelessOpen.load() ? 0.040 * mr : 48.0;
    const double target = 1.5 * micBlock.load() + n * base + cushion;

    if (! primed)
    {
        if (fill < target) { silence(); return; }
        primed = true;
    }
    if (fill > target * 3.0 + mr * 0.03)          // backlog (host paused, clocks drifted): catch up
        r = w - (juce::uint64) target;

    corr = juce::jlimit (-0.004, 0.004, corr + 0.00003 * ((fill - target) / target));
    const double step = base * (1.0 + corr);

    for (int i = 0; i < n; ++i)
    {
        frac += step;
        while (frac >= 1.0)
        {
            if (r >= w)
            {
                // underrun: fade out the rest smoothly and rebuffer
                for (int j = i; j < n; ++j) { lastOut *= 0.995f; dest[j] = lastOut; }
                fadeIn = 0.0f;
                primed = false;
                readIdx.store (r, std::memory_order_release);
                return;
            }
            hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = hist[3];
            hist[3] = ring[(size_t) (r & (kRingSize - 1))];
            ++r; frac -= 1.0;
        }
        // 4-point Hermite between hist[1] and hist[2]
        const float t = (float) frac;
        const float c0 = hist[1], c1 = 0.5f * (hist[2] - hist[0]);
        const float c2 = hist[0] - 2.5f * hist[1] + 2.0f * hist[2] - 0.5f * hist[3];
        const float c3 = 0.5f * (hist[3] - hist[0]) + 1.5f * (hist[1] - hist[2]);
        fadeIn = juce::jmin (1.0f, fadeIn + fadeStep);   // ~5 ms fade-in after any gap
        lastOut = (((c3 * t + c2) * t + c1) * t + c0) * fadeIn;
        dest[i] = lastOut;
    }
    readIdx.store (r, std::memory_order_release);
}
