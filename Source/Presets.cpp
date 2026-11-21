#include "Presets.h"

// Index des listes :
//  bands     0:8 1:12 2:16 3:24 4:32 5:48 6:64
//  source    0:MIDI 1:AUTO 2:SIDECHAIN      (en MIDI sans notes, vck suit la voix tout seul)
//  wave      0:SAW 1:SQUARE 2:PULSE 3:SUPER 4:BUZZ 5:NOISE
//  mode      0:POLY 1:MONO 2:LEGATO
//  octave    0:-2 1:-1 2:0 3:+1 4:+2
//  harmony   0:AUCUNE 1:OCTAVE 2:QUINTE 3:ACCORD 4:CHOEUR
//  dblVoices 0:OFF 1:2 VOIX 2:4 VOIX
//  delayTime 0:1/16 1:1/8 2:1/8 D 3:1/4 4:1/4 D 5:1/2 6:1/8 T

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, juce::UndoManager& u) : state (s), undo (u)
{
    auto add = [this] (const char* name, const char* cat, const char* info, std::initializer_list<std::pair<const char*, float>> v)
    {
        Factory f { u8 (name), u8 (cat), u8 (info), {} };
        for (auto& kv : v) f.values.push_back ({ kv.first, kv.second });
        factory.push_back (std::move (f));
    };

    add ("Init", "BASE", "Point de départ neutre : la porteuse suit ta voix (AUTO), 24 bandes.", {});
    add ("Haute Définition", "BASE", "64 bandes fines, consonnes très présentes : le robot le plus intelligible.",
         { { "bands", 6 }, { "sharp", 0.75f }, { "sibil", 0.7f }, { "breath", 0.6f }, { "character", 0.f }, { "bright", 4 } });

    // ---------------------------------------------------------------- DAFT PUNK
    add ("Around The World", "DAFT PUNK", "Robot talkbox mono, notes liées et glissées, très lisible.",
         { { "source", 0 }, { "wave", 0 }, { "mode", 2 }, { "glide", 45 }, { "bands", 4 }, { "sharp", 0.72f },
           { "attack", 2 }, { "release", 30 }, { "sibil", 0.35f }, { "bright", 4 }, { "vibrato", 0.1f },
           { "envA", 3 }, { "envR", 90 }, { "reverb", 0.06f }, { "phaser", 0.15f }, { "phRate", 0.3f } });
    add ("Harder Better Faster", "DAFT PUNK", "Robot syllabique, impulsion étroite et serrée : chaque syllabe claque.",
         { { "source", 0 }, { "wave", 2 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.8f }, { "attack", 1 },
           { "release", 18 }, { "bright", 5 }, { "sibil", 0.3f }, { "envA", 1 }, { "envR", 40 }, { "comp", 0.4f },
           { "delayMix", 0.08f }, { "delayTime", 0 }, { "delayFb", 0.2f }, { "reverb", 0.04f } });
    add ("Digital Love", "DAFT PUNK", "Poly chaud et large : supersaw, ensemble, doubleur 2 voix et délai pointé.",
         { { "source", 0 }, { "wave", 3 }, { "unison", 5 }, { "detune", 0.3f }, { "bands", 3 }, { "release", 60 },
           { "sharp", 0.5f }, { "envA", 15 }, { "envR", 400 }, { "ensemble", 0.35f }, { "dblVoices", 1 },
           { "dblMix", 0.35f }, { "delayMix", 0.12f }, { "delayTime", 2 }, { "reverb", 0.2f }, { "spread", 0.3f } });
    add ("Something About Us", "DAFT PUNK", "Doux et feutré, formants un peu plus bas, ensemble et grande réverbe.",
         { { "source", 0 }, { "wave", 0 }, { "unison", 3 }, { "detune", 0.15f }, { "bands", 4 }, { "release", 90 },
           { "sharp", 0.6f }, { "bright", 1 }, { "formant", -1 }, { "envA", 40 }, { "envR", 600 }, { "highCut", 9000 },
           { "ensemble", 0.5f }, { "reverb", 0.3f }, { "revSize", 0.6f }, { "sibil", 0.4f }, { "drive", 0.15f } });
    add ("One More Time", "DAFT PUNK", "Autotune robot dur, sans MIDI : chante, vck cale chaque note dans la gamme.",
         { { "source", 1 }, { "retune", 0 }, { "humanize", 0 }, { "wave", 3 }, { "unison", 5 }, { "detune", 0.2f },
           { "bands", 5 }, { "sharp", 0.6f }, { "sibil", 0.6f }, { "voice", 0.12f }, { "bright", 4 },
           { "dblVoices", 1 }, { "dblMix", 0.3f }, { "reverb", 0.1f }, { "envR", 60 }, { "comp", 0.3f } });
    add ("Robot Rock", "DAFT PUNK", "Mono carré brut, formants relevés, saturé, double serré.",
         { { "source", 0 }, { "wave", 1 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.75f }, { "formant", 1 },
           { "bright", 6 }, { "sibil", 0.25f }, { "dblVoices", 1 }, { "dblDetune", 0.8f }, { "dblMix", 0.4f },
           { "envR", 60 }, { "drive", 0.45f }, { "comp", 0.5f } });
    add ("Lose Yourself To Dance", "DAFT PUNK", "Chœur de robots : poly, doubleur 4 voix, ensemble.",
         { { "source", 0 }, { "wave", 0 }, { "unison", 3 }, { "detune", 0.2f }, { "bands", 4 }, { "release", 50 },
           { "ensemble", 0.3f }, { "dblVoices", 2 }, { "dblMix", 0.45f }, { "reverb", 0.25f }, { "envR", 250 }, { "spread", 0.4f } });
    add ("Instant Crush", "DAFT PUNK", "Vocoder pop brillant et précis, délai à la noire.",
         { { "source", 0 }, { "wave", 3 }, { "unison", 5 }, { "bands", 5 }, { "sharp", 0.65f }, { "bright", 6 },
           { "sibil", 0.6f }, { "dblVoices", 1 }, { "dblMix", 0.3f }, { "delayMix", 0.1f }, { "delayTime", 3 },
           { "reverb", 0.15f }, { "envR", 200 } });
    add ("Television Rules The Nation", "DAFT PUNK", "Robot monotone : la porteuse reste sur la tonique de ta gamme (DRONE), sans MIDI.",
         { { "source", 1 }, { "drone", 1 }, { "octave", 1 }, { "wave", 4 }, { "bands", 2 }, { "sharp", 0.85f },
           { "sibil", 0.3f }, { "retune", 0 }, { "bright", 3 }, { "phaser", 0.25f } });

    // ---------------------------------------------------------------- CLASSIQUES
    add ("The Robots", "CLASSIQUES", "Kraftwerk : 16 bandes très sélectives, impulsion buzz, sec et froid.",
         { { "source", 0 }, { "wave", 4 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.9f }, { "release", 25 },
           { "sibil", 0.2f }, { "bright", 2 }, { "reverb", 0.05f } });
    add ("Mr. Blue Sky", "CLASSIQUES", "ELO : vocoder mono en dents de scie, petit délai à la croche.",
         { { "source", 0 }, { "wave", 0 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.6f }, { "release", 35 },
           { "sibil", 0.4f }, { "delayMix", 0.12f }, { "delayTime", 1 }, { "delayFb", 0.3f }, { "reverb", 0.15f } });
    add ("I Thought It Was You", "CLASSIQUES", "Herbie Hancock : vocoder poly chaleureux, ensemble à la Sennheiser.",
         { { "source", 0 }, { "wave", 0 }, { "unison", 2 }, { "detune", 0.1f }, { "bands", 3 }, { "release", 55 },
           { "sharp", 0.55f }, { "envR", 300 }, { "ensemble", 0.45f }, { "reverb", 0.18f }, { "drive", 0.1f } });
    add ("Computer Love", "CLASSIQUES", "Zapp & Roger : talkbox funk, legato très glissé, vibrato.",
         { { "source", 0 }, { "wave", 0 }, { "mode", 2 }, { "glide", 80 }, { "vibrato", 0.25f }, { "bands", 4 },
           { "sharp", 0.75f }, { "formant", -1 }, { "bright", 3 }, { "sibil", 0.3f }, { "envR", 120 }, { "drive", 0.2f } });
    add ("O Superman", "CLASSIQUES", "Laurie Anderson : poly carré, délai à la noire qui se répète, espace.",
         { { "source", 0 }, { "wave", 1 }, { "bands", 3 }, { "release", 70 }, { "sharp", 0.5f }, { "delayMix", 0.25f },
           { "delayTime", 3 }, { "delayFb", 0.45f }, { "reverb", 0.3f }, { "envR", 500 } });
    add ("Sexy Boy", "CLASSIQUES", "Air : mono carré, 12 bandes, formants graves, un peu d'ensemble.",
         { { "source", 0 }, { "wave", 1 }, { "mode", 1 }, { "bands", 1 }, { "formant", -2 }, { "sharp", 0.6f },
           { "glide", 20 }, { "sibil", 0.25f }, { "ensemble", 0.2f }, { "reverb", 0.12f } });
    add ("Intergalactic", "CLASSIQUES", "Beastie Boys : voix de robot monotone (DRONE), buzz, sans MIDI.",
         { { "source", 1 }, { "drone", 1 }, { "wave", 4 }, { "bands", 2 }, { "sharp", 0.8f },
           { "sibil", 0.4f }, { "octave", 1 }, { "retune", 0 }, { "drive", 0.3f } });

    // ---------------------------------------------------------------- MODERNES
    add ("Hide And Seek", "MODERNES", "Imogen Heap : harmonies poly limpides (joue des accords), consonnes très présentes.",
         { { "source", 0 }, { "wave", 0 }, { "bands", 5 }, { "sharp", 0.45f }, { "release", 40 }, { "sibil", 0.75f },
           { "bright", 3 }, { "envA", 10 }, { "envR", 250 }, { "reverb", 0.35f }, { "revSize", 0.7f }, { "character", 0.f } });
    add ("Woods", "MODERNES", "Bon Iver : chœur automatique à partir de ta voix (harmonies dans la gamme), réverbe ample.",
         { { "source", 1 }, { "harmony", 4 }, { "retune", 15 }, { "wave", 0 }, { "unison", 2 },
           { "bands", 5 }, { "sibil", 0.65f }, { "reverb", 0.4f }, { "revSize", 0.75f }, { "dblVoices", 1 },
           { "dblMix", 0.3f }, { "voice", 0.1f }, { "spread", 0.35f } });
    add ("Friends (Prismizer)", "MODERNES", "Francis and the Lights : accord automatique précis, 64 bandes, voix d'origine dessous.",
         { { "source", 1 }, { "harmony", 3 }, { "retune", 10 }, { "wave", 3 }, { "unison", 3 },
           { "bands", 6 }, { "sibil", 0.7f }, { "voice", 0.25f }, { "reverb", 0.2f } });
    add ("Talkbox Funk", "MODERNES", "Chromeo : talkbox legato, aigus brillants, double léger.",
         { { "source", 0 }, { "wave", 0 }, { "mode", 2 }, { "glide", 60 }, { "bands", 4 }, { "sharp", 0.75f },
           { "bright", 5 }, { "vibrato", 0.15f }, { "dblVoices", 1 }, { "dblMix", 0.25f }, { "drive", 0.25f } });
    add ("Hyperpop Robot", "MODERNES", "Autotune sec, formants hauts, double et délai à la croche.",
         { { "source", 1 }, { "retune", 0 }, { "formant", 3 }, { "wave", 3 }, { "unison", 5 },
           { "bands", 5 }, { "sibil", 0.6f }, { "dblVoices", 1 }, { "dblMix", 0.4f }, { "delayMix", 0.1f }, { "delayTime", 1 },
           { "comp", 0.5f } });

    // ---------------------------------------------------------------- DOUBLEUR
    add ("Doubleur 2 voix", "DOUBLEUR", "Doubleur seul (sans robot), façon Waves Doubler 2 : une voix à gauche, une à droite.",
         { { "robot", 0 }, { "voice", 1 }, { "dblVoices", 1 }, { "dblDetune", 0.9f }, { "dblDelay", 1.f },
           { "dblWidth", 1.f }, { "dblDrift", 0.4f }, { "dblMix", 0.55f }, { "reverb", 0 } });
    add ("Doubleur 4 voix", "DOUBLEUR", "Doubleur seul, façon Waves Doubler 4 : petit chœur naturel très large.",
         { { "robot", 0 }, { "voice", 1 }, { "dblVoices", 2 }, { "dblDetune", 1.3f }, { "dblDelay", 1.3f },
           { "dblWidth", 1.4f }, { "dblDrift", 0.6f }, { "dblMix", 0.6f }, { "reverb", 0 } });
    add ("Doubleur ADT serré", "DOUBLEUR", "Double automatique serré façon studio 60s : 2 voix très proches.",
         { { "robot", 0 }, { "voice", 1 }, { "dblVoices", 1 }, { "dblDetune", 0.4f }, { "dblDelay", 0.55f },
           { "dblWidth", 0.8f }, { "dblDrift", 0.25f }, { "dblMix", 0.5f }, { "reverb", 0 } });
    add ("Doubleur Ultra Large", "DOUBLEUR", "4 voix écartées au maximum (largeur 200 %) : mur de voix.",
         { { "robot", 0 }, { "voice", 1 }, { "dblVoices", 2 }, { "dblDetune", 1.6f }, { "dblDelay", 1.5f },
           { "dblWidth", 2.f }, { "dblDrift", 0.7f }, { "dblMix", 0.65f }, { "reverb", 0.08f } });
    add ("Robot + Voix doublée", "DOUBLEUR", "Ta voix doublée avec un robot discret dessous.",
         { { "source", 1 }, { "robot", 0.45f }, { "voice", 1 }, { "dblVoices", 2 }, { "dblDetune", 1.1f },
           { "dblMix", 0.5f }, { "reverb", 0.12f } });

    // ---------------------------------------------------------------- CREATIFS
    add ("Chœur Céleste", "CRÉATIFS", "Nappe de chœur : supersaw 7 voix, attaque lente, ensemble et grande réverbe.",
         { { "source", 0 }, { "wave", 3 }, { "unison", 7 }, { "detune", 0.35f }, { "envA", 300 }, { "envR", 1500 },
           { "ensemble", 0.6f }, { "reverb", 0.55f }, { "revSize", 0.85f }, { "bands", 4 }, { "sharp", 0.4f },
           { "sibil", 0.4f }, { "spread", 0.6f } });
    add ("Murmure", "CRÉATIFS", "Voix chuchotée : la porteuse est un souffle, elle suit ta voix.",
         { { "source", 1 }, { "wave", 5 }, { "bands", 5 }, { "sibil", 0.9f }, { "sharp", 0.5f }, { "reverb", 0.2f } });
    add ("Basse Vocoder", "CRÉATIFS", "Basse qui parle : mono carré une octave plus bas, sub.",
         { { "source", 0 }, { "wave", 1 }, { "mode", 1 }, { "octave", 1 }, { "bands", 2 }, { "bright", -3 },
           { "sharp", 0.6f }, { "sibil", 0.2f }, { "sub", 0.6f }, { "drive", 0.3f } });
    add ("Chipmunk Bot", "CRÉATIFS", "Robot aigu et rapide : formants +7, une octave au-dessus.",
         { { "source", 1 }, { "retune", 5 }, { "formant", 7 }, { "octave", 3 }, { "bands", 4 } });
    add ("Démon", "CRÉATIFS", "Voix d'outre-tombe : formants -6, une octave en dessous, buzz.",
         { { "source", 1 }, { "humanize", 1 }, { "formant", -6 }, { "octave", 1 }, { "wave", 4 }, { "bands", 3 },
           { "reverb", 0.2f }, { "sub", 0.4f }, { "drive", 0.3f } });
    add ("Radio Robot", "CRÉATIFS", "8 bandes étroites, très brillant : talkie-walkie de science-fiction.",
         { { "source", 1 }, { "drone", 1 }, { "bands", 0 }, { "sharp", 0.9f }, { "bright", 8 }, { "wave", 4 },
           { "lowCut", 220 }, { "highCut", 6000 }, { "drive", 0.6f }, { "comp", 0.5f } });
    add ("Phaser Spatial", "CRÉATIFS", "Robot qui tourne : phaser lent, bandes en stéréo, délai ping-pong.",
         { { "source", 1 }, { "wave", 3 }, { "bands", 4 }, { "spread", 0.8f }, { "phaser", 0.6f }, { "phRate", 0.15f },
           { "phDepth", 0.8f }, { "delayMix", 0.2f }, { "delayTime", 2 }, { "reverb", 0.2f } });

    current = factory.front().name;
}

bool PresetManager::keptByPresets (const juce::String& id)
{
    return id == "clipOn" || id == "sync" || id == "bpm" || id == "transpose" || id == "rate" || id == "loop"
        || id == "bypass" || id == "output" || id == "gate" || id == "inGain" || id == "noLatency"
        || id == "key" || id == "scale";
}

juce::File PresetManager::getDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("vck").getChildFile ("Presets");
}

void PresetManager::setTreeValue (const juce::String& id, float value, juce::UndoManager* um)
{
    if (auto* rp = state.getParameter (id))
    {
        auto child = state.state.getChildWithProperty ("id", id);
        const float vv = rp->convertFrom0to1 (rp->convertTo0to1 (value));
        if (child.isValid()) child.setProperty ("value", vv, um);
        else rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }
}

void PresetManager::resetForPreset (juce::UndoManager* um)
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keptByPresets (rp->paramID))
                setTreeValue (rp->paramID, rp->convertFrom0to1 (rp->getDefaultValue()), um);
}

void PresetManager::load (const juce::String& name)
{
    for (auto& f : factory)
        if (f.name == name)
        {
            undo.beginNewTransaction ("Preset");
            resetForPreset (&undo);
            for (auto& [id, v] : f.values)
            {
                if (! keptByPresets (id)) setTreeValue (id, v, &undo);
            }
            undo.beginNewTransaction();
            current = name;
            return;
        }

    auto file = getDirectory().getChildFile (name + ".preset");
    if (! file.existsAsFile()) return;
    if (auto xml = juce::parseXML (file))
    {
        if (! xml->hasTagName (state.state.getType())) return;
        auto tree = juce::ValueTree::fromXml (*xml);
        undo.beginNewTransaction ("Preset");
        resetForPreset (&undo);
        for (const auto& child : tree)
        {
            const auto id = child.getProperty ("id").toString();
            if (id.isNotEmpty() && ! keptByPresets (id) && child.hasProperty ("value"))
                setTreeValue (id, (float) child.getProperty ("value"), &undo);
        }
        undo.beginNewTransaction();
        current = name;
    }
}

bool PresetManager::save (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty() || isFactory (name)) return false;
    auto dir = getDirectory();
    if (! dir.createDirectory()) return false;
    auto st = state.copyState();
    st.setProperty ("preset", name, nullptr);
    if (auto xml = st.createXml())
        if (xml->writeTo (dir.getChildFile (name + ".preset")))
        {
            current = name;
            return true;
        }
    return false;
}

bool PresetManager::remove (const juce::String& name)
{
    if (isFactory (name)) return false;
    auto file = getDirectory().getChildFile (name + ".preset");
    if (! file.existsAsFile()) return false;
    const bool ok = file.moveToTrash();
    if (ok) current = factory.front().name;
    return ok;
}

int PresetManager::importFiles (const juce::Array<juce::File>& files)
{
    auto dir = getDirectory();
    dir.createDirectory();
    int n = 0;
    for (auto& f : files)
    {
        if (! f.existsAsFile()) continue;
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName (state.state.getType())) continue;
        auto dest = dir.getNonexistentChildFile (f.getFileNameWithoutExtension(), ".preset", false);
        if (f.copyFileTo (dest)) ++n;
    }
    return n;
}

void PresetManager::openFolder() const
{
    auto dir = getDirectory();
    dir.createDirectory();
    dir.startAsProcess();
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;
    for (auto& f : factory) names.add (f.name);
    return names;
}

std::vector<std::pair<juce::String, juce::StringArray>> PresetManager::getFactoryByCategory() const
{
    std::vector<std::pair<juce::String, juce::StringArray>> out;
    for (auto& f : factory)
    {
        auto it = std::find_if (out.begin(), out.end(), [&] (auto& p) { return p.first == f.category; });
        if (it == out.end()) out.push_back ({ f.category, juce::StringArray { f.name } });
        else it->second.add (f.name);
    }
    return out;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    for (auto& f : getDirectory().findChildFiles (juce::File::findFiles, false, "*.preset"))
        if (! isFactory (f.getFileNameWithoutExtension())) names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getAllNames() const
{
    auto all = getFactoryNames();
    all.addArray (getUserNames());
    return all;
}

bool PresetManager::isFactory (const juce::String& name) const
{
    for (auto& f : factory) if (f.name == name) return true;
    return false;
}

juce::String PresetManager::getInfo (const juce::String& name) const
{
    for (auto& f : factory) if (f.name == name) return f.info;
    return u8 ("Preset utilisateur");
}

void PresetManager::next()
{
    auto all = getAllNames();
    if (all.isEmpty()) return;
    const int idx = all.indexOf (current);
    load (all[(idx + 1) % all.size()]);
}

void PresetManager::previous()
{
    auto all = getAllNames();
    if (all.isEmpty()) return;
    const int idx = all.indexOf (current);
    load (all[(idx <= 0 ? all.size() : idx) - 1]);
}
