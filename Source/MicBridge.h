// Basemental Talk Box - MicBridge
// Opens any microphone directly (built-in, USB interface, Bluetooth headset/AirPods, iPhone mic...),
// independent of the DAW's own audio device, and feeds it to the plug-in at the host's sample rate.
//
//   device thread  --(lock-free ring, mic rate)-->  host audio thread  --(drift-corrected resampler)-->  voice
//
// The two clocks are never exactly the same (and Bluetooth mics often run at 16 or 24 kHz), so the
// reader resamples with a cubic interpolator whose ratio is nudged to keep the ring at a steady fill.
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <vector>

class MicBridge : private juce::AudioIODeviceCallback,
                  private juce::AudioIODeviceType::Listener,
                  private juce::Timer
{
public:
    struct Device
    {
        juce::String type, name;
        bool bluetooth = false, usb = false, builtIn = false, wireless = false, isDefault = false;
    };
    enum class State { Off, Running, Missing, Failed, Denied };

    // channel modes
    static constexpr int kAutoChannel = -1;   // follow whichever input you're singing into
    static constexpr int kMixChannels = -2;   // average all inputs

    MicBridge();
    ~MicBridge() override;

    //=== message thread ===========================================================
    void refreshDevices();
    const juce::Array<Device>& getDevices() const { return devices; }
    // name empty = the system's default input. Opens (or re-opens) right away and keeps retrying.
    void setWanted (const juce::String& type, const juce::String& name, int channel);
    void setChannelMode (int mode) { channelMode.store (mode); }
    int  getChannelMode() const    { return channelMode.load(); }
    void close();                  // stop and forget (side-chain mode)

    State getState() const               { return state; }
    juce::String getStatus() const       { return status; }
    juce::String getWarning() const      { return warning; }
    juce::String getOpenName() const     { return openName; }
    juce::String getWantedName() const   { return wantedName; }
    juce::String getWantedType() const   { return wantedType; }
    bool isOpenDeviceWireless() const    { return openWireless; }
    juce::StringArray getChannelNames() const { return channelNames; }
    int getActiveChannel() const         { return activeChannel.load(); }
    float takePeak()                     { return peak.exchange (0.0f); }

    std::function<void()> onChange;      // called on the message thread when the list or state changes

   #if BTB_UI_TEST
    void addFakeDevicesForTesting()
    {
        devices.add ({ "CoreAudio", "MacBook Pro Microphone", false, false, true, false, true });
        devices.add ({ "CoreAudio", "AirPods Pro", true, false, false, true, false });
        devices.add ({ "CoreAudio", "Scarlett 2i2 USB", false, true, false, false, false });
        devices.add ({ "CoreAudio", "Shure MV7", false, true, false, false, false });
        devices.add ({ "CoreAudio", "Chris's iPhone Microphone", false, false, false, true, false });
        wantedName = "Scarlett 2i2 USB"; openName = wantedName; state = State::Running;
        status = "Scarlett 2i2 USB  |  48.0 kHz  |  ~14 ms";
        channelNames = { "Input 1", "Input 2" };
        notify();
    }
   #endif

    //=== host audio thread ========================================================
    void prepare (double hostSampleRate) { hostRate.store (hostSampleRate); }
    void read (float* dest, int numSamples);        // always writes numSamples
    bool isRunning() const { return running.load(); }

private:
    // device callback
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int n, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& message) override;
    void audioDeviceListChanged() override;
    void timerCallback() override;

    bool tryOpen();
    void closeDevice();
    void notify() { if (onChange) onChange(); }
    Device describe (juce::AudioIODeviceType& t, const juce::String& name, bool isDefault) const;

    juce::OwnedArray<juce::AudioIODeviceType> types;
    juce::Array<Device> devices;
    std::unique_ptr<juce::AudioIODevice> device;

    juce::String wantedType, wantedName, openName, status, warning;
    juce::StringArray channelNames;
    State state = State::Off;
    bool wanted = false, openWireless = false;
    std::atomic<bool> errorFlag { false };
    int retryCountdown = 0;

    // ring buffer (single producer: device thread, single consumer: host thread)
    static constexpr int kRingSize = 1 << 16;
    std::vector<float> ring = std::vector<float> ((size_t) kRingSize, 0.0f);
    std::atomic<juce::uint64> writeIdx { 0 }, readIdx { 0 };
    std::atomic<juce::uint32> generation { 0 };
    std::atomic<double> micRate { 0.0 }, hostRate { 48000.0 };
    std::atomic<int> micBlock { 512 };
    std::atomic<bool> running { false };
    std::atomic<float> peak { 0.0f };
    std::atomic<int> channelMode { kAutoChannel }, activeChannel { 0 };
    float chanEnv[32] {};

    // reader state (host thread only)
    juce::uint32 seenGen = 0xffffffff;
    bool primed = false;
    double frac = 0.0, corr = 0.0;
    float hist[4] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicBridge)
};
