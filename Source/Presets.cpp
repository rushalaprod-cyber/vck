#include "Presets.h"

// Index des listes :
//  bands     0:8 1:12 2:16 3:24 4:32 5:48 6:64
//  source    0:MIDI 1:AUTO 2:SIDECHAIN
//  wave      0:SAW 1:SQUARE 2:PULSE 3:SUPER 4:BUZZ 5:NOISE
//  mode      0:POLY 1:MONO 2:LEGATO
//  octave    0:-2 1:-1 2:0 3:+1 4:+2
//  scale     0:LIBRE 1:CHROMA 2:MAJEUR 3:MINEUR 4:MIN HARM 5:PENTA MAJ 6:PENTA MIN 7:DORIEN 8:DRONE
//  harmony   0:AUCUNE 1:OCTAVE 2:QUINTE 3:ACCORD 4:CHOEUR
//  dblVoices 0:OFF 1:2 VOIX 2:4 VOIX
//  delayTime 0:1/16 1:1/8 2:1/8 D 3:1/4 4:1/4 D 5:1/2 6:1/8 T
//  key       0:C 1:C# 2:D 3:D# 4:E 5:F 6:F# 7:G 8:G# 9:A 10:A# 11:B

const std::vector<FactoryPreset>& PresetManager::factory()
{
    static const std::vector<FactoryPreset> list =
    {
        { "Init", "BASE", "Point de depart neutre : la porteuse suit ta voix (AUTO), 24 bandes.", {} },

        // ---------------------------------------------------------------- DAFT PUNK
        { "Around The World", "DAFT PUNK",
          "Robot talkbox mono, notes liees et glissees, tres lisible. Joue la melodie en MIDI.",
          { { "source", 0 }, { "wave", 0 }, { "mode", 2 }, { "glide", 45 }, { "bands", 4 }, { "sharp", 0.72f },
            { "attack", 2 }, { "release", 30 }, { "sibil", 0.35f }, { "bright", 4 }, { "vibrato", 0.1f },
            { "envA", 3 }, { "envR", 90 }, { "reverb", 0.06f } } },
        { "Harder Better Faster", "DAFT PUNK",
          "Robot syllabique, impulsion etroite et serree : chaque syllabe claque.",
          { { "source", 0 }, { "wave", 2 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.8f }, { "attack", 1 },
            { "release", 18 }, { "bright", 5 }, { "sibil", 0.3f }, { "envA", 1 }, { "envR", 40 },
            { "delayMix", 0.08f }, { "delayTime", 0 }, { "delayFb", 0.2f }, { "reverb", 0.04f } } },
        { "Digital Love", "DAFT PUNK",
          "Poly chaud et large : supersaw, ensemble, doubleur 2 voix et delai pointe.",
          { { "source", 0 }, { "wave", 3 }, { "unison", 5 }, { "detune", 0.3f }, { "bands", 3 }, { "release", 60 },
            { "sharp", 0.5f }, { "envA", 15 }, { "envR", 400 }, { "ensemble", 0.35f }, { "dblVoices", 1 },
            { "dblMix", 0.35f }, { "delayMix", 0.12f }, { "delayTime", 2 }, { "reverb", 0.2f } } },
        { "Something About Us", "DAFT PUNK",
          "Doux et feutre, formants un peu plus bas, ensemble et grande reverbe.",
          { { "source", 0 }, { "wave", 0 }, { "unison", 3 }, { "detune", 0.15f }, { "bands", 4 }, { "release", 90 },
            { "sharp", 0.6f }, { "bright", 1 }, { "formant", -1 }, { "envA", 40 }, { "envR", 600 },
            { "ensemble", 0.5f }, { "reverb", 0.3f }, { "revSize", 0.6f }, { "sibil", 0.4f } } },
        { "One More Time", "DAFT PUNK",
          "Autotune robot dur, sans MIDI : chante, vck cale chaque note dans la gamme (majeur).",
          { { "source", 1 }, { "scale", 2 }, { "retune", 0 }, { "wave", 3 }, { "unison", 5 }, { "detune", 0.2f },
            { "bands", 5 }, { "sharp", 0.6f }, { "sibil", 0.6f }, { "voice", 0.12f }, { "bright", 4 },
            { "dblVoices", 1 }, { "dblMix", 0.3f }, { "reverb", 0.1f }, { "envR", 60 } } },
        { "Robot Rock", "DAFT PUNK",
          "Mono carre brut, formants releves, double serre.",
          { { "source", 0 }, { "wave", 1 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.75f }, { "formant", 1 },
            { "bright", 6 }, { "sibil", 0.25f }, { "dblVoices", 1 }, { "dblDetune", 8 }, { "dblMix", 0.4f }, { "envR", 60 } } },
        { "Lose Yourself To Dance", "DAFT PUNK",
          "Choeur de robots : poly, doubleur 4 voix, ensemble.",
          { { "source", 0 }, { "wave", 0 }, { "unison", 3 }, { "detune", 0.2f }, { "bands", 4 }, { "release", 50 },
            { "ensemble", 0.3f }, { "dblVoices", 2 }, { "dblMix", 0.45f }, { "reverb", 0.25f }, { "envR", 250 } } },
        { "Instant Crush", "DAFT PUNK",
          "Vocoder pop brillant et precis, delai a la noire.",
          { { "source", 0 }, { "wave", 3 }, { "unison", 5 }, { "bands", 5 }, { "sharp", 0.65f }, { "bright", 6 },
            { "sibil", 0.6f }, { "dblVoices", 1 }, { "dblMix", 0.3f }, { "delayMix", 0.1f }, { "delayTime", 3 },
            { "reverb", 0.15f }, { "envR", 200 } } },
        { "Television Rules The Nation", "DAFT PUNK",
          "Robot monotone : la porteuse reste sur la tonique (DRONE en Mi), sans MIDI.",
          { { "source", 1 }, { "scale", 8 }, { "key", 4 }, { "octave", 1 }, { "wave", 4 }, { "bands", 2 },
            { "sharp", 0.85f }, { "sibil", 0.3f }, { "retune", 0 }, { "bright", 3 } } },

        // ---------------------------------------------------------------- CLASSIQUES
        { "The Robots", "CLASSIQUES",
          "Kraftwerk : 16 bandes tres selectives, impulsion buzz, sec et froid.",
          { { "source", 0 }, { "wave", 4 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.9f }, { "release", 25 },
            { "sibil", 0.2f }, { "bright", 2 }, { "reverb", 0.05f } } },
        { "Mr. Blue Sky", "CLASSIQUES",
          "ELO : vocoder mono en dents de scie, petit delai a la croche.",
          { { "source", 0 }, { "wave", 0 }, { "mode", 1 }, { "bands", 2 }, { "sharp", 0.6f }, { "release", 35 },
            { "sibil", 0.4f }, { "delayMix", 0.12f }, { "delayTime", 1 }, { "delayFb", 0.3f }, { "reverb", 0.15f } } },
        { "I Thought It Was You", "CLASSIQUES",
          "Herbie Hancock : vocoder poly chaleureux, ensemble a la Sennheiser.",
          { { "source", 0 }, { "wave", 0 }, { "unison", 2 }, { "detune", 0.1f }, { "bands", 3 }, { "release", 55 },
            { "sharp", 0.55f }, { "envR", 300 }, { "ensemble", 0.45f }, { "reverb", 0.18f } } },
        { "Computer Love", "CLASSIQUES",
          "Zapp & Roger : talkbox funk, legato tres glisse, vibrato.",
          { { "source", 0 }, { "wave", 0 }, { "mode", 2 }, { "glide", 80 }, { "vibrato", 0.25f }, { "bands", 4 },
            { "sharp", 0.75f }, { "formant", -1 }, { "bright", 3 }, { "sibil", 0.3f }, { "envR", 120 } } },
        { "O Superman", "CLASSIQUES",
          "Laurie Anderson : poly carre, delai a la noire qui se repete, espace.",
          { { "source", 0 }, { "wave", 1 }, { "bands", 3 }, { "release", 70 }, { "sharp", 0.5f }, { "delayMix", 0.25f },
            { "delayTime", 3 }, { "delayFb", 0.45f }, { "reverb", 0.3f }, { "envR", 500 } } },
        { "Sexy Boy", "CLASSIQUES",
          "Air : mono carre, 12 bandes, formants graves, un peu d'ensemble.",
          { { "source", 0 }, { "wave", 1 }, { "mode", 1 }, { "bands", 1 }, { "formant", -2 }, { "sharp", 0.6f },
            { "glide", 20 }, { "sibil", 0.25f }, { "ensemble", 0.2f }, { "reverb", 0.12f } } },
        { "Intergalactic", "CLASSIQUES",
          "Beastie Boys : voix de robot monotone en Re, buzz, sans MIDI.",
          { { "source", 1 }, { "scale", 8 }, { "key", 2 }, { "wave", 4 }, { "bands", 2 }, { "sharp", 0.8f },
            { "sibil", 0.4f }, { "octave", 1 }, { "retune", 0 } } },

        // ---------------------------------------------------------------- MODERNES
        { "Hide And Seek", "MODERNES",
          "Imogen Heap : harmonies poly limpides (joue des accords), consonnes tres presentes.",
          { { "source", 0 }, { "wave", 0 }, { "bands", 5 }, { "sharp", 0.45f }, { "release", 40 }, { "sibil", 0.75f },
            { "bright", 3 }, { "envA", 10 }, { "envR", 250 }, { "reverb", 0.35f }, { "revSize", 0.7f } } },
        { "Woods", "MODERNES",
          "Bon Iver : choeur automatique a partir de ta voix (harmonies dans la gamme), reverbe ample.",
          { { "source", 1 }, { "scale", 2 }, { "harmony", 4 }, { "retune", 15 }, { "wave", 0 }, { "unison", 2 },
            { "bands", 5 }, { "sibil", 0.65f }, { "reverb", 0.4f }, { "revSize", 0.75f }, { "dblVoices", 1 },
            { "dblMix", 0.3f }, { "voice", 0.1f } } },
        { "Friends (Prismizer)", "MODERNES",
          "Francis and the Lights : accord automatique precis, 64 bandes, voix d'origine dessous.",
          { { "source", 1 }, { "scale", 2 }, { "harmony", 3 }, { "retune", 10 }, { "wave", 3 }, { "unison", 3 },
            { "bands", 6 }, { "sibil", 0.7f }, { "voice", 0.25f }, { "reverb", 0.2f } } },
        { "Talkbox Funk", "MODERNES",
          "Chromeo : talkbox legato, aigus brillants, double leger.",
          { { "source", 0 }, { "wave", 0 }, { "mode", 2 }, { "glide", 60 }, { "bands", 4 }, { "sharp", 0.75f },
            { "bright", 5 }, { "vibrato", 0.15f }, { "dblVoices", 1 }, { "dblMix", 0.25f } } },
        { "Hyperpop Robot", "MODERNES",
          "Autotune sec, formants hauts, double et delai a la croche.",
          { { "source", 1 }, { "scale", 2 }, { "retune", 0 }, { "formant", 3 }, { "wave", 3 }, { "unison", 5 },
            { "bands", 5 }, { "sibil", 0.6f }, { "dblVoices", 1 }, { "dblMix", 0.4f }, { "delayMix", 0.1f }, { "delayTime", 1 } } },

        // ---------------------------------------------------------------- DOUBLEUR
        { "Doubleur 2 voix", "DOUBLEUR",
          "Doubleur seul (sans robot), facon Waves Doubler 2 : une voix a gauche, une a droite.",
          { { "robot", 0 }, { "voice", 1 }, { "dblVoices", 1 }, { "dblDetune", 9 }, { "dblDelay", 14 },
            { "dblWidth", 0.9f }, { "dblDrift", 0.4f }, { "dblMix", 0.55f }, { "reverb", 0 } } },
        { "Doubleur 4 voix", "DOUBLEUR",
          "Doubleur seul, facon Waves Doubler 4 : petit choeur naturel tres large.",
          { { "robot", 0 }, { "voice", 1 }, { "dblVoices", 2 }, { "dblDetune", 14 }, { "dblDelay", 22 },
            { "dblWidth", 1 }, { "dblDrift", 0.6f }, { "dblMix", 0.6f }, { "reverb", 0 } } },
        { "Doubleur ADT serre", "DOUBLEUR",
          "Double automatique serre facon studio 60s : 2 voix tres proches.",
          { { "robot", 0 }, { "voice", 1 }, { "dblVoices", 1 }, { "dblDetune", 4 }, { "dblDelay", 8 },
            { "dblWidth", 0.7f }, { "dblDrift", 0.25f }, { "dblMix", 0.5f }, { "reverb", 0 } } },
        { "Robot + Voix doublee", "DOUBLEUR",
          "Ta voix doublee avec un robot discret dessous.",
          { { "source", 1 }, { "scale", 1 }, { "robot", 0.45f }, { "voice", 1 }, { "dblVoices", 2 }, { "dblDetune", 11 },
            { "dblDelay", 18 }, { "dblMix", 0.5f }, { "reverb", 0.12f } } },

        // ---------------------------------------------------------------- CREATIFS
        { "Choeur Celeste", "CREATIFS",
          "Nappe de choeur : supersaw 7 voix, attaque lente, ensemble et grande reverbe.",
          { { "source", 0 }, { "wave", 3 }, { "unison", 7 }, { "detune", 0.35f }, { "envA", 300 }, { "envR", 1500 },
            { "ensemble", 0.6f }, { "reverb", 0.55f }, { "revSize", 0.85f }, { "bands", 4 }, { "sharp", 0.4f }, { "sibil", 0.4f } } },
        { "Murmure", "CREATIFS",
          "Voix chuchotee : la porteuse est un souffle, elle suit ta voix.",
          { { "source", 1 }, { "wave", 5 }, { "bands", 5 }, { "sibil", 0.9f }, { "sharp", 0.5f }, { "reverb", 0.2f } } },
        { "Basse Vocoder", "CREATIFS",
          "Basse qui parle : mono carre une octave plus bas.",
          { { "source", 0 }, { "wave", 1 }, { "mode", 1 }, { "octave", 1 }, { "bands", 2 }, { "bright", -3 },
            { "sharp", 0.6f }, { "sibil", 0.2f } } },
        { "Chipmunk Bot", "CREATIFS",
          "Robot aigu et rapide : formants +7, une octave au-dessus.",
          { { "source", 1 }, { "scale", 1 }, { "retune", 5 }, { "formant", 7 }, { "octave", 3 }, { "bands", 4 } } },
        { "Demon", "CREATIFS",
          "Voix d'outre-tombe : formants -6, une octave en dessous, buzz.",
          { { "source", 1 }, { "scale", 0 }, { "formant", -6 }, { "octave", 1 }, { "wave", 4 }, { "bands", 3 }, { "reverb", 0.2f } } },
        { "Radio Robot", "CREATIFS",
          "8 bandes etroites, tres brillant : talkie-walkie de science-fiction.",
          { { "source", 1 }, { "scale", 8 }, { "bands", 0 }, { "sharp", 0.9f }, { "bright", 8 }, { "wave", 4 } } },
    };
    return list;
}

juce::StringArray PresetManager::categories()
{
    juce::StringArray c;
    for (auto& p : factory()) c.addIfNotAlreadyThere (p.category);
    return c;
}

juce::File PresetManager::userDir() const
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("vck").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

juce::StringArray PresetManager::userNames() const
{
    juce::StringArray out;
    for (auto& f : userDir().findChildFiles (juce::File::findFiles, false, "*.vckpreset"))
        out.add (f.getFileNameWithoutExtension());
    out.sort (true);
    return out;
}

void PresetManager::setParam (const juce::String& id, float v)
{
    if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (st.getParameter (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    }
}

// Les reglages de lecture MIDI, la sortie et le bypass ne sont pas touches par un preset
static bool keptByPresets (const juce::String& id)
{
    return id == "clipOn" || id == "sync" || id == "bpm" || id == "transpose" || id == "rate" || id == "loop"
        || id == "bypass" || id == "output" || id == "gate";
}

void PresetManager::resetToDefaults()
{
    for (auto* p : st.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keptByPresets (rp->paramID))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
}

void PresetManager::loadFactory (int index)
{
    auto& list = factory();
    if (index < 0 || index >= (int) list.size()) return;
    resetToDefaults();
    for (auto& kv : list[(size_t) index].values) setParam (kv.first, kv.second);
    currentName = list[(size_t) index].name;
    currentInfo = u8 (list[(size_t) index].info);
    currentFactory = index;
}

bool PresetManager::saveUser (const juce::String& name)
{
    auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty()) return false;
    juce::XmlElement xml ("VCKPRESET");
    xml.setAttribute ("name", name);
    for (auto* p : st.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keptByPresets (rp->paramID))
                xml.setAttribute (rp->paramID, rp->convertFrom0to1 (rp->getValue()));
    const bool ok = xml.writeTo (userDir().getChildFile (safe + ".vckpreset"));
    if (ok) { currentName = name; currentInfo = u8 ("Preset utilisateur"); currentFactory = -1; }
    return ok;
}

bool PresetManager::loadUser (const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (userDir().getChildFile (name + ".vckpreset"));
    if (xml == nullptr || ! xml->hasTagName ("VCKPRESET")) return false;
    resetToDefaults();
    for (int i = 0; i < xml->getNumAttributes(); ++i)
    {
        const auto id = xml->getAttributeName (i);
        if (id != "name" && ! keptByPresets (id)) setParam (id, (float) xml->getAttributeValue (i).getDoubleValue());
    }
    currentName = name; currentInfo = u8 ("Preset utilisateur"); currentFactory = -1;
    return true;
}
