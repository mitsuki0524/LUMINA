cmake_minimum_required(VERSION 3.22)
project(LUMINA VERSION 1.0.0 LANGUAGES C CXX)

# VST3の互換エラー回避
set(JUCE_VST3_AUTO_MANIFEST OFF)
add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)
add_compile_definitions(JUCE_USE_WINDOWS_MEDIA_FORMAT=1)

# 1. C++20の有効化
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. JUCEディレクトリの指定 (環境に合わせて変更してください)
set(JUCE_DIR "C:/JUCE")
add_subdirectory(${JUCE_DIR} build_juce)

# 3. プラグインの定義 (LUMINA向けにエフェクト仕様へ変更)
juce_add_plugin(LUMINA
    COMPANY_NAME "OTODESK"
    IS_SYNTH FALSE                  
    NEEDS_MIDI_INPUT FALSE          
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE 
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE "Otdk"
    PLUGIN_CODE "Lumn"              
    FORMATS VST3 Standalone
    PRODUCT_NAME "Lumina"
    CHANNEL_CONFIGS 2,2             
)

# 4. コンパイルオプション（SIMD最適化と文字コード設定）
target_compile_options(LUMINA PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:
        $<$<CONFIG:Release>:/O2 /fp:fast> /arch:AVX2 /wd4819 /utf-8
    >
)

# 5. ソースコードの追加 (計画書の構成に完全準拠)
target_sources(LUMINA PRIVATE
    # Main
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    # DSP Group
    Source/DSP/HPSSEngine.cpp
    Source/DSP/HPSSEngine.h
    Source/DSP/MaskingModel.cpp
    Source/DSP/MaskingModel.h
    Source/DSP/OnsetDetector.cpp
    Source/DSP/OnsetDetector.h
    Source/DSP/MSEncoder.cpp
    Source/DSP/MSEncoder.h
    Source/DSP/SpectralEngine.h
    Source/DSP/SpectralEngine.cpp

    # GUI Group
    Source/GUI/SpectrumAnalyzer.cpp
    Source/GUI/SpectrumAnalyzer.h
    Source/GUI/GRMeter.cpp
    Source/GUI/GRMeter.h
    Source/GUI/AnalysisFifo.h
    Source/GUI/AbletonLookAndFeel.h
 "Source/DSP/AnalyzerCore.h" "Source/DSP/AnalyzerCore.cpp")

# 6. インクルードパス (ディレクトリ階層の指定)
target_include_directories(LUMINA PRIVATE
    Source
    Source/DSP
    Source/GUI
    # ⚡ IntelliSenseのエラーを消すための明示的パス
    ${CMAKE_CURRENT_BINARY_DIR}/LUMINA_artefacts/JuceLibraryCode
)

# 7. JUCEモジュールのリンク
target_link_libraries(LUMINA PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core        
    juce::juce_events      
)

# 8. JUCE 8 固有の設定
juce_generate_juce_header(LUMINA)

if(WIN32)    
    target_compile_definitions(LUMINA PRIVATE JUCE_DIRECT2D=1)
endif()