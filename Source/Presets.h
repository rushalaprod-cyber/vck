#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <utility>

inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

// Presets facon ravenn : usine par categories + presets utilisateur (.preset dans
// Documents/vck/Presets), import / export, precedent / suivant. Un chargement = une
// seule etape d'undo. La gamme (KEY / SCALE), le clip MIDI, le niveau d'entree, le
// gate, la sortie, le bypass et le mode de latence ne sont jamais changes par un preset.
class PresetManager
{
public:
    struct Factory
    {
        juce::String name, category, info;
        std::vector<std::pair<juce::String, float>> values;
    };

    PresetManager (juce::AudioProcessorValueTreeState& s, juce::UndoManager& u);

    static bool keptByPresets (const juce::String& id);

    void load (const juce::String& name);
    bool save (const juce::String& name);
    bool remove (const juce::String& name);
    void next();
    void previous();
    int importFiles (const juce::Array<juce::File>& files);
    void openFolder() const;
    static juce::File getDirectory();

    juce::StringArray getFactoryNames() const;
    std::vector<std::pair<juce::String, juce::StringArray>> getFactoryByCategory() const;
    juce::StringArray getUserNames() const;
    juce::StringArray getAllNames() const;
    bool isFactory (const juce::String& name) const;
    juce::String getInfo (const juce::String& name) const;
    const std::vector<Factory>& getFactory() const { return factory; }

    juce::String getCurrent() const { return current; }
    void setCurrentName (const juce::String& n) { current = n; }

    // ecrit une valeur dans l'arbre de l'APVTS (avec ou sans undo)
    void setTreeValue (const juce::String& id, float value, juce::UndoManager* um);

private:
    void resetForPreset (juce::UndoManager* um);
    juce::AudioProcessorValueTreeState& state;
    juce::UndoManager& undo;
    std::vector<Factory> factory;
    juce::String current;
};
