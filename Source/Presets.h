#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <utility>

// Texte UTF-8 (accents) -> juce::String
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

struct FactoryPreset
{
    const char* name;
    const char* category;
    const char* info;                                    // ce que fait le preset / la reference
    std::vector<std::pair<const char*, float>> values;   // valeurs reelles (index pour les listes)
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : st (s) {}

    static const std::vector<FactoryPreset>& factory();
    static juce::StringArray categories();

    juce::File userDir() const;
    juce::StringArray userNames() const;

    void loadFactory (int index);
    bool loadUser (const juce::String& name);
    bool saveUser (const juce::String& name);
    void resetToDefaults();

    juce::String currentName { "Init" }, currentInfo;
    int currentFactory = 0;      // -1 = preset utilisateur

private:
    void setParam (const juce::String& id, float realValue);
    juce::AudioProcessorValueTreeState& st;
};
