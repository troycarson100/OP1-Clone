/*
  ==============================================================================
  
    Custom Standalone Application Main
    Auto-enables all MIDI input devices on startup
  
  ==============================================================================
*/

#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_gui_basics/native/juce_WindowsHooks_windows.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "../JuceWrapper/PluginProcessor.h"

namespace juce
{

//==============================================================================
/**
    Helper class to enable MIDI devices with retry logic
    Uses exponential backoff if no devices are found initially
*/
class MidiDeviceEnabler : private Timer
{
public:
    MidiDeviceEnabler (StandaloneFilterWindow* w) : window (w), attemptCount (0), delayMs (500)
    {
        // Start with initial delay to ensure audio is fully initialized
        startTimer (500);
    }
    
    ~MidiDeviceEnabler() override
    {
        stopTimer();
    }
    
private:
    void timerCallback() override
    {
        attemptCount++;
        
        if (window == nullptr)
        {
            stopTimer();
            delete this;
            return;
        }
        
        auto& deviceManager = window->getDeviceManager();
        auto midiInputs = MidiInput::getAvailableDevices();
        
        // Enable each MIDI input device
        bool anyEnabled = false;
        for (const auto& device : midiInputs)
        {
            if (! deviceManager.isMidiInputDeviceEnabled (device.identifier))
            {
                deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
                anyEnabled = true;
            }
        }
        
        // If devices were found (whether we enabled them or they were already enabled), stop
        // Also stop if we've tried enough times (exponential backoff exhausted)
        if (midiInputs.size() > 0 || attemptCount >= 10)
        {
            stopTimer();
            delete this;
            return;
        }
        
        // No devices found yet - use exponential backoff: double the delay each time (max 4 seconds)
        delayMs = jmin (delayMs * 2, 4000);
        stopTimer();
        startTimer (delayMs);
    }
    
    StandaloneFilterWindow* window;
    int attemptCount;
    int delayMs;
};

//==============================================================================
/**
    Custom Standalone Filter App that automatically enables all MIDI input devices
*/
class StandaloneFilterApp : public JUCEApplication
{
public:
    StandaloneFilterApp()
    {
        PropertiesFile::Options options;
        
        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif
        
        appProperties.setStorageParameters (options);
    }
    
    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return true; }
    void anotherInstanceStarted (const String&) override    {}
    
    StandaloneFilterWindow* createWindow()
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }
        
        auto* window = new StandaloneFilterWindow (getApplicationName(),
                                                   LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                                                   createPluginHolder());
        
        return window;
    }
    
    std::unique_ptr<StandalonePluginHolder> createPluginHolder()
    {
        // Enable auto-open MIDI devices on desktop (normally only enabled on mobile)
        constexpr auto autoOpenMidiDevices = true;
        
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif
        
        return std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(),
                                                         false,
                                                         String{},
                                                         nullptr,
                                                         channelConfig,
                                                         autoOpenMidiDevices);
    }
    
    //==============================================================================
    void initialise (const String&) override
    {
        mainWindow = rawToUniquePtr (createWindow());
        
        if (mainWindow != nullptr)
        {
           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif
            
            mainWindow->setVisible (true);
            
            // Enable MIDI devices after window is visible and audio is initialized
            // Use MessageManager to ensure this happens after startPlaying() completes
            // The StandalonePluginHolder's timer will handle hot-plugged devices
            MessageManager::callAsync([this]() {
                Timer::callAfterDelay (500, [this]() {
                    if (mainWindow != nullptr)
                    {
                        enableAllMidiInputDevices (mainWindow.get());
                    }
                });
                // Also try again after longer delay in case devices aren't ready
                Timer::callAfterDelay (2000, [this]() {
                    if (mainWindow != nullptr)
                    {
                        enableAllMidiInputDevices (mainWindow.get());
                    }
                });
                // Use retry mechanism for devices that appear later
                new MidiDeviceEnabler (mainWindow.get());
            });
        }
        else
        {
            pluginHolder = createPluginHolder();
        }
    }
    
    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }
    
    void systemRequestedQuit() override
    {
        quit();
    }
    
private:
    //==============================================================================
    ApplicationProperties appProperties;
    std::unique_ptr<StandaloneFilterWindow> mainWindow;
    std::unique_ptr<StandalonePluginHolder> pluginHolder;
    
    //==============================================================================
    /**
        Enable all available MIDI input devices via the device manager
        Register our custom MidiInputHandler callback for each device
    */
    void enableAllMidiInputDevices (StandaloneFilterWindow* window)
    {
        if (window == nullptr)
            return;
            
        auto* processor = window->getAudioProcessor();
        if (processor == nullptr)
            return;
            
        // Cast to our processor type to access MidiInputHandler
        auto* op1Processor = dynamic_cast<Op1CloneAudioProcessor*> (processor);
        if (op1Processor == nullptr)
            return;
            
        auto* pluginHolder = window->getPluginHolder();
        if (pluginHolder == nullptr)
            return;
            
        auto& deviceManager = window->getDeviceManager();
        auto& midiHandler = op1Processor->getMidiInputHandler();
        
        // Get all available MIDI input devices
        auto midiInputs = MidiInput::getAvailableDevices();
        
        juce::StringArray enabledNames;
        
        // First, remove the default player callback and use only ours
        // The player callback goes to messageCollector, but we want our FIFO
        // We'll register our callback as the global one (empty string = all devices)
        deviceManager.removeMidiInputDeviceCallback ({}, &pluginHolder->player);
        deviceManager.addMidiInputDeviceCallback ({}, &midiHandler);
        
        // Enable each MIDI input device
        for (const auto& device : midiInputs)
        {
            // Enable the device
            if (! deviceManager.isMidiInputDeviceEnabled (device.identifier))
            {
                deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
            }
            
            // Also register per-device callback (redundant but ensures it works)
            deviceManager.addMidiInputDeviceCallback (device.identifier, &midiHandler);
            enabledNames.add (device.name);
        }
        
        // Update device names for UI
        midiHandler.setEnabledDeviceNames (enabledNames);
    }
    
};

} // namespace juce

//==============================================================================
START_JUCE_APPLICATION (juce::StandaloneFilterApp)

#endif // JucePlugin_Build_Standalone

