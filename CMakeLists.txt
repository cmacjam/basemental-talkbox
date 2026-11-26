cmake_minimum_required(VERSION 3.22)

# ---- Product identity (change the name here in one place) -------------------
set(PRODUCT_NAME "Basemental Talk Box")
set(PRODUCT_VERSION 1.0.0)

project(BasementalTalkBox VERSION ${PRODUCT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# ---- JUCE: local checkout via -DJUCE_DIR=..., otherwise downloaded ----------
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Formats per platform ------------------------------------------------------
#   macOS   : AU (Logic, GarageBand, MainStage), VST3, Standalone  [+ AAX for Pro Tools]
#   Windows : VST3, Standalone                                     [+ AAX for Pro Tools]
#   Linux   : VST3, LV2 (Ardour, Bitwig, Reaper, Carla...), Standalone
# Pro Tools: pass -DAAX_SDK_PATH=/path/to/aax-sdk (free from Avid's developer site).
if(APPLE)
    set(PLUGIN_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
else()
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
endif()
if(DEFINED AAX_SDK_PATH AND (APPLE OR WIN32))
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND PLUGIN_FORMATS AAX)
endif()

juce_add_plugin(BasementalTalkBox
    PRODUCT_NAME "${PRODUCT_NAME}"
    COMPANY_NAME "Basemental"
    BUNDLE_ID com.basemental.talkbox
    PLUGIN_MANUFACTURER_CODE Bsml
    PLUGIN_CODE BTlk
    FORMATS ${PLUGIN_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    AAX_CATEGORY AAX_ePlugInCategory_SWGenerators
    LV2URI "https://basemental.com/plugins/talkbox"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Basemental Talk Box uses your microphone as the talk box mouth.")

target_sources(BasementalTalkBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MicBridge.cpp
    Source/MicSetupPanel.cpp)

target_include_directories(BasementalTalkBox PRIVATE Source)

if(UNIX AND NOT APPLE)
    # Linux standalone: JACK and ALSA (both loaded at run time if installed)
    target_compile_definitions(BasementalTalkBox PUBLIC JUCE_JACK=1)
endif()

target_compile_definitions(BasementalTalkBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BasementalTalkBox
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Offline DSP test (no JUCE): renders demo WAVs -------------------------
option(BUILD_DSP_TESTS "Build the offline DSP render test" OFF)
if(BUILD_DSP_TESTS)
    add_executable(render_demo tests/render_demo.cpp)
endif()
