#include "PluginEditor.h"
#include <BinaryData.h>
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif
#include "UiDraw.h"
using namespace uidraw;

namespace
{
    juce::String T (const char* s) { return juce::String::fromUTF8 (s); }
    enum Page { pgVocoder = 0, pgCarrier, pgAuto, pgDoubler, pgEffects, numPages };
}

VckEditor::VckEditor (VckProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      // ---- VOCODER
      bands (p.apvts, "bands", ui::violet, T ("Nombre de bandes : peu = robot rétro, beaucoup = voix précise et intelligible. Le changement se fait en fondu, sans clic.")),
      loFreq (p.apvts, "loFreq", "LOW", ui::violet, T ("Fréquence de la bande la plus grave.")),
      hiFreq (p.apvts, "hiFreq", "HIGH", ui::violet, T ("Fréquence de la bande la plus aiguë.")),
      sharp (p.apvts, "sharp", "SHARP", ui::violet, T ("Finesse des bandes : bas = doux et fondu, haut = précis et métallique.")),
      spread (p.apvts, "spread", "SPREAD", ui::violet, T ("Bandes alternées gauche / droite : un robot en stéréo.")),
      inGain (p.apvts, "inGain", "INPUT", ui::cyan, T ("Niveau de la voix qui entre dans vck (règle le gate et la détection).")),
      gate (p.apvts, "gate", "GATE", ui::cyan, T ("Seuil du gate (avec hystérésis et maintien : il ne hache jamais la voix).")),
      attack (p.apvts, "attack", "ATTACK", ui::cyan, T ("Vitesse de réaction du robot aux syllabes.")),
      release (p.apvts, "release", "RELEASE", ui::cyan, T ("Durée de retombée : court = haché et robotique, long = lié et fluide.")),
      formant (p.apvts, "formant", "FORMANT", ui::pink, T ("Déplace les formants : + = voix plus petite, - = voix plus grosse, sans changer la note.")),
      bright (p.apvts, "bright", "BRIGHT", ui::pink, T ("Brillance : remonte les aigus de la voix pour plus d'intelligibilité.")),
      character (p.apvts, "character", "CHARACTER", ui::pink, T ("0 = intelligibilité maximale. Plus haut = on entend davantage le timbre de la porteuse (onde, synthé).")),
      sibil (p.apvts, "sibil", "CONSONNES", ui::pink, T ("Laisse passer les consonnes (s, ch, t, f) de ta voix.")),
      breath (p.apvts, "breath", "SOUFFLE", ui::pink, T ("Souffle ajouté à la porteuse sur les consonnes : des « s » naturels plutôt que buzzés.")),
      freeze (p.apvts, "freeze", "FREEZE", ui::cyan, T ("Fige la voyelle en cours.")),
      bankView (p),
      // ---- CARRIER
      wave (p.apvts, "wave", ui::cyan, T ("SAW classique, SQUARE creux, PULSE nasal, SUPER large, BUZZ robot dur, NOISE chuchoté.")),
      mode (p.apvts, "mode", ui::cyan, T ("POLY = accords, MONO = une note (glide toujours), LEGATO = glide sur les notes liées (talkbox).")),
      octave (p.apvts, "octave", ui::cyan, T ("Octave de la porteuse.")),
      unison (p.apvts, "unison", "UNISON", ui::cyan, T ("Oscillateurs par note : épaissit le son.")),
      detune (p.apvts, "detune", "DETUNE", ui::cyan, T ("Écart entre les oscillateurs de l'unisson.")),
      sub (p.apvts, "sub", "SUB", ui::cyan, T ("Oscillateur une octave en dessous : plus de corps.")),
      noise (p.apvts, "noise", "NOISE", ui::cyan, T ("Souffle permanent dans la porteuse.")),
      glide (p.apvts, "glide", "GLIDE", ui::cyan, T ("Glissement entre les notes.")),
      vibrato (p.apvts, "vibrato", "VIBRATO", ui::cyan, T ("Vibrato naturel, qui arrive après l'attaque.")),
      vibRate (p.apvts, "vibRate", "VIB RATE", ui::cyan, T ("Vitesse du vibrato.")),
      velSens (p.apvts, "velSens", "VELOCITY", ui::cyan, T ("Sensibilité à la vélocité MIDI.")),
      envA (p.apvts, "envA", "ENV ATT", ui::pink, T ("Attaque des notes.")),
      envR (p.apvts, "envR", "ENV REL", ui::pink, T ("Relâchement des notes.")),
      bendRange (p.apvts, "bendRange", "BEND", ui::pink, T ("Amplitude du pitch bend (demi-tons).")),
      // ---- AUTO
      key (p.apvts, "key", ui::green, T ("Tonalité du morceau. Jamais changée par un preset.")),
      voiceRange (p.apvts, "voiceRange", ui::green, T ("Tessiture de ta voix : évite les erreurs d'octave. LARGE = tout.")),
      harmony (p.apvts, "harmony", ui::green, T ("Harmonies ajoutées automatiquement, dans la gamme.")),
      scale (p.apvts, "scale", T ("Gamme du morceau (jamais changée par un preset). LIBRE = suit ta voix exactement.")),
      drone (p.apvts, "drone", "DRONE", ui::green, T ("Robot monotone : la porteuse reste sur la tonique, quelle que soit la note chantée.")),
      retune (p.apvts, "retune", "RETUNE", ui::green, T ("Vitesse de correction : 0 = autotune dur, plus haut = plus naturel.")),
      humanize (p.apvts, "humanize", "HUMANIZE", ui::green, T ("Garde une partie des nuances de ta voix (vibrato, attaques) autour de la note corrigée.")),
      tracking (p.apvts, "tracking", "TRACKING", ui::green, T ("Sensibilité de la détection : bas = sûr (voix nette), haut = suit aussi les voix soufflées.")),
      hold (p.apvts, "hold", "HOLD", ui::green, T ("Temps pendant lequel la note tient quand ta voix s'arrête ou devient une consonne.")),
      harmLevel (p.apvts, "harmLevel", "HARM LEVEL", ui::green, T ("Niveau des harmonies par rapport à la voix principale.")),
      autoTrans (p.apvts, "autoTrans", "TRANSPOSE", ui::green, T ("Transpose le robot (demi-tons) par rapport à ta voix.")),
      pitchGraph (p),
      // ---- DOUBLER
      dblVoices (p.apvts, "dblVoices", ui::violet, T ("Doubleur façon Waves Doubler : 2 ou 4 voix.")),
      dblMix (p.apvts, "dblMix", "MIX", ui::violet, T ("Niveau des voix doublées.")),
      dblDirect (p.apvts, "dblDirect", "DIRECT", ui::violet, T ("Niveau du son direct (non doublé).")),
      dblWidth (p.apvts, "dblWidth", "WIDTH", ui::violet, T ("Largeur des voix doublées. Au-delà de 100 % : encore plus large (M/S).")),
      dblDetune (p.apvts, "dblDetune", "DETUNE x", ui::violet, T ("Multiplie le désaccord de toutes les voix.")),
      dblDelay (p.apvts, "dblDelay", "DELAY x", ui::violet, T ("Multiplie le retard de toutes les voix.")),
      dblDrift (p.apvts, "dblDrift", "DRIFT", ui::violet, T ("Dérive naturelle de hauteur et de placement (comme un vrai double).")),
      dblRate (p.apvts, "dblRate", "RATE", ui::violet, T ("Vitesse de la dérive.")),
      dblHpf (p.apvts, "dblHpf", "HPF", ui::violet, T ("Coupe-bas des voix doublées.")),
      dblLpf (p.apvts, "dblLpf", "LPF", ui::violet, T ("Coupe-haut des voix doublées.")),
      // ---- EFFECTS
      drive (p.apvts, "drive", "DRIVE", ui::pink, T ("Saturation à lampe, anti-repliement.")),
      lowCut (p.apvts, "lowCut", "LOW CUT", ui::pink, T ("Coupe-bas de sortie.")),
      highCut (p.apvts, "highCut", "HIGH CUT", ui::pink, T ("Coupe-haut de sortie.")),
      comp (p.apvts, "comp", "COMP", ui::pink, T ("Compresseur un bouton : resserre et densifie le robot.")),
      ensemble (p.apvts, "ensemble", "ENSEMBLE", ui::cyan, T ("Chorus des string machines : le son vocoder des années 80.")),
      ensRate (p.apvts, "ensRate", "ENS RATE", ui::cyan, T ("Vitesse de l'ensemble.")),
      phaser (p.apvts, "phaser", "PHASER", ui::cyan, T ("Phaser 6 étages stéréo.")),
      phRate (p.apvts, "phRate", "PH RATE", ui::cyan, T ("Vitesse du phaser.")),
      phDepth (p.apvts, "phDepth", "PH DEPTH", ui::cyan, T ("Profondeur du phaser.")),
      delayMix (p.apvts, "delayMix", "MIX", ui::violet, T ("Délai calé sur le BPM.")),
      delayFb (p.apvts, "delayFb", "FEEDBACK", ui::violet, T ("Répétitions du délai.")),
      delayTone (p.apvts, "delayTone", "TONE", ui::violet, T ("Couleur des répétitions (plus sombre à gauche).")),
      reverb (p.apvts, "reverb", "MIX", ui::green, T ("Réverbération.")),
      revSize (p.apvts, "revSize", "SIZE", ui::green, T ("Taille de la pièce.")),
      revPre (p.apvts, "revPre", "PRE-DELAY", ui::green, T ("Pré-délai : sépare la voix de la réverbe.")),
      revDamp (p.apvts, "revDamp", "DAMP", ui::green, T ("Amortissement des aigus de la réverbe.")),
      revWidth (p.apvts, "revWidth", "WIDTH", ui::green, T ("Largeur stéréo de la réverbe.")),
      delayTime (p.apvts, "delayTime", T ("Division du délai, calée sur le BPM.")),
      pingPong (p.apvts, "pingPong", "PING-PONG", ui::violet, T ("Répétitions alternées gauche / droite.")),
      // ---- OUTPUT
      source (p.apvts, "source", ui::cyan, T ("Ce qui joue les notes du robot. MIDI : clavier ou fichier .mid (sans note, vck suit ta voix tout seul). AUTO : suit ta voix. SIDECHAIN : un synthé externe."),
              juce::StringArray { "MIDI", "AUTO", "SIDECHAIN" }),
      robot (p.apvts, "robot", "ROBOT", ui::red, T ("Niveau du robot (vocoder).")),
      voice (p.apvts, "voice", "VOIX", ui::red, T ("Ta voix d'origine sous le robot (doublée si le doubleur est actif).")),
      width (p.apvts, "width", "WIDTH", ui::red, T ("Largeur stéréo globale.")),
      output (p.apvts, "output", "OUTPUT", ui::red, T ("Volume de sortie (limiteur transparent intégré).")),
      // ---- MIDI
      roll (p),
      clipOn (p.apvts, "clipOn", "PLAY", ui::pink, T ("Joue le clip MIDI importé.")),
      loop (p.apvts, "loop", "LOOP", ui::pink, T ("Boucle le clip MIDI.")),
      sync (p.apvts, "sync", ui::pink, T ("HOST : suit la lecture et le BPM du DAW. FREE : tourne au BPM réglé.")),
      rate (p.apvts, "rate", ui::pink, T ("Vitesse du clip : moitié, normale, double.")),
      bpm (p.apvts, "bpm", T ("BPM en mode FREE (ou sans DAW). Glisse pour changer, double-clic = 120.")),
      transpose (p.apvts, "transpose", T ("Transposition du clip MIDI en demi-tons.")),
      voiceView (p),
      noLatency (p.apvts, "noLatency", "NO LATENCY", ui::cyan, T ("Coupé : mode HQ (17 ms compensés par le DAW) — la hauteur est détectée en avance et le limiteur anticipe. Allumé : zéro latence pour chanter en direct.")),
      bypass (p.apvts, "bypass", "BYPASS", ui::red, T ("Écoute le son d'origine (aligné sur la latence).")),
      browser (p)
{
    setLookAndFeel (&lnf);
    ravenImg = juce::ImageCache::getFromMemory (BinaryData::raven_png, BinaryData::raven_pngSize);
    coverImg = juce::ImageCache::getFromMemory (BinaryData::cover_jpg, BinaryData::cover_jpgSize);

    key.setColumns (12);
    dblVoices.setColumns (1);
    source.setColumns (1);

    // 4 voix du doubleur : retard, desaccord, panoramique, gain
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n (i + 1);
        dblVoiceKnobs.add (new NeonKnob (p.apvts, "dblDelay" + n, "DELAY", ui::violet, T ("Retard de cette voix.")));
        dblVoiceKnobs.add (new NeonKnob (p.apvts, "dblDetune" + n, "DETUNE", ui::cyan, T ("Désaccord de cette voix (cents, + ou -).")));
        dblVoiceKnobs.add (new NeonKnob (p.apvts, "dblPan" + n, "PAN", ui::pink, T ("Placement de cette voix.")));
        dblVoiceKnobs.add (new NeonKnob (p.apvts, "dblGain" + n, "GAIN", ui::red, T ("Niveau de cette voix.")));
    }

    // ---- placement par page (-1 = toujours visible)
    for (juce::Component* c : std::initializer_list<juce::Component*> { &bands, &loFreq, &hiFreq, &sharp, &spread, &inGain, &gate, &attack, &release,
                                                                         &formant, &bright, &character, &sibil, &breath, &freeze, &bankView })
        place (*c, pgVocoder);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &wave, &mode, &octave, &unison, &detune, &sub, &noise, &glide, &vibrato,
                                                                         &vibRate, &velSens, &envA, &envR, &bendRange })
        place (*c, pgCarrier);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &key, &voiceRange, &harmony, &scale, &drone, &retune, &humanize, &tracking,
                                                                         &hold, &harmLevel, &autoTrans, &pitchGraph })
        place (*c, pgAuto);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &dblVoices, &dblMix, &dblDirect, &dblWidth, &dblDetune, &dblDelay, &dblDrift,
                                                                         &dblRate, &dblHpf, &dblLpf })
        place (*c, pgDoubler);
    for (auto* k : dblVoiceKnobs) place (*k, pgDoubler);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &drive, &lowCut, &highCut, &comp, &ensemble, &ensRate, &phaser, &phRate, &phDepth,
                                                                         &delayMix, &delayFb, &delayTone, &delayTime, &pingPong, &reverb, &revSize,
                                                                         &revPre, &revDamp, &revWidth })
        place (*c, pgEffects);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &source, &robot, &voice, &width, &output, &roll, &importMidiBtn, &clearMidiBtn,
                                                                         &clipOn, &loop, &sync, &rate, &bpm, &transpose, &voiceView,
                                                                         &noLatency, &bypass, &undoBtn, &redoBtn, &resetBtn, &prevBtn, &nextBtn,
                                                                         &saveBtn, &folderBtn, &importBtn, &copyBtn, &presetNameBtn })
        addAndMakeVisible (c);

    // ---- MIDI
    importMidiBtn.setButtonText ("IMPORT MIDI");
    clearMidiBtn.setButtonText ("CLEAR");
    for (auto* b : { &importMidiBtn, &clearMidiBtn }) b->setColour (juce::TextButton::buttonOnColourId, ui::pink);
    importMidiBtn.setTooltip (T ("Charge un fichier .mid : la mélodie / les accords joueront le robot, calés sur le tempo."));
    clearMidiBtn.setTooltip (T ("Retire le clip MIDI."));
    importMidiBtn.onClick = [this] { importMidi(); };
    clearMidiBtn.onClick = [this] { proc.clearMidi(); };

    // ---- en-tete (comme ravenn)
    undoBtn.setButtonText ("UNDO");
    undoBtn.setTooltip (T ("Annuler (undo)"));
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.setButtonText ("REDO");
    redoBtn.setTooltip (T ("Rétablir (redo)"));
    redoBtn.onClick = [this] { proc.undo.redo(); };
    resetBtn.setButtonText ("RESET");
    resetBtn.setTooltip (T ("Remet tous les paramètres par défaut (la gamme reste). Annulable avec UNDO, refaisable avec REDO."));
    resetBtn.onClick = [this] { proc.resetAllParameters(); };
    prevBtn.onClick = [this] { proc.presets.previous(); };
    nextBtn.onClick = [this] { proc.presets.next(); };
    saveBtn.onClick = [this] { savePreset(); };
    folderBtn.setButtonText ("DOSSIER");
    folderBtn.setTooltip (T ("Ouvre le dossier des presets (Documents/vck/Presets)"));
    folderBtn.onClick = [this] { proc.presets.openFolder(); };
    importBtn.setButtonText ("IMPORT");
    importBtn.setTooltip (T ("Importe des fichiers .preset (copiés dans ton dossier de presets)"));
    importBtn.onClick = [this] { importPresets(); };
    presetNameBtn.setTooltip (T ("Ouvre la page des presets"));
    presetNameBtn.onClick = [this] { showBrowser (! browser.isVisible()); };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = slotBtns.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
        b->setColour (juce::TextButton::buttonOnColourId, ui::cyan);
        b->setTooltip (T ("Slots de comparaison A / B / C / D : chaque slot garde ses propres réglages."));
        b->onClick = [this, i] { proc.selectSlot (i); };
        addAndMakeVisible (b);
    }
    copyBtn.setButtonText (T ("→"));
    copyBtn.setTooltip (T ("Copie les réglages actuels dans un autre slot"));
    copyBtn.onClick = [this]
    {
        const int cur = proc.getABSlot();
        juce::PopupMenu m;
        m.addSectionHeader (T ("Copier ") + juce::String::charToString ((juce::juce_wchar) ('A' + cur)) + T (" vers…"));
        for (int i = 0; i < 4; ++i)
            if (i != cur) m.addItem (i + 1, juce::String::charToString ((juce::juce_wchar) ('A' + i)) + (proc.slotHasState (i) ? T ("  (remplace)") : juce::String()));
        m.addSeparator();
        m.addItem (10, T ("Tous les autres slots"));
        juce::Component::SafePointer<VckEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&copyBtn), [safe, cur] (int r)
        {
            if (safe == nullptr || r == 0) return;
            if (r == 10) { for (int i = 0; i < 4; ++i) if (i != cur) safe->proc.copySlotTo (i); }
            else safe->proc.copySlotTo (r - 1);
        });
    };

    // ---- onglets
    const char* tabNames[numPages] = { "VOCODER", "CARRIER", "AUTO", "DOUBLER", "EFFECTS" };
    for (int i = 0; i < numPages; ++i)
    {
        auto* t = tabs.add (new juce::TextButton (tabNames[i]));
        t->getProperties().set ("tab", true);
        t->setColour (juce::TextButton::buttonOnColourId, ui::violet);
        t->onClick = [this, i] { setPage (i); };
        addAndMakeVisible (t);
    }

    // ---- page des presets
    addChildComponent (browser);
    browser.onClose = [this] { browser.setVisible (false); };
    browser.onSave = [this] { savePreset(); };
    browser.onImport = [this] { importPresets(); };
    browser.onExport = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (T ("Exporter le preset"),
                      juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (proc.presets.getCurrent() + ".preset"), "*.preset");
        juce::Component::SafePointer<VckEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (auto xml = safe->proc.apvts.copyState().createXml()) xml->writeTo (f.withFileExtension (".preset"));
        });
    };

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (baseW / baseH);
        c->setSizeLimits ((int) (baseW * 0.6f), (int) (baseH * 0.6f), (int) (baseW * 1.6f), (int) (baseH * 1.6f));
    }
    setSize ((int) (baseW * 0.88f), (int) (baseH * 0.88f));
    setPage (pgVocoder);
    refreshPresets();
    startTimerHz (15);
}

VckEditor::~VckEditor() { setLookAndFeel (nullptr); }

void VckEditor::place (juce::Component& c, int pg)
{
    pageOf.push_back ({ &c, pg });
    addChildComponent (c);
}

void VckEditor::setPage (int pg)
{
    page = pg;
    for (auto& [c, p] : pageOf) c->setVisible (p == page);
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setToggleState (i == page, juce::dontSendNotification);
    shownVoices = -1;
    browser.toFront (false);
    repaint();
}

void VckEditor::refreshPresets()
{
    shownPreset = proc.presets.getCurrent();
    presetNameBtn.setButtonText (shownPreset + T ("  ▾"));
    browser.refresh();
}

void VckEditor::savePreset()
{
    auto* w = new juce::AlertWindow (T ("Sauvegarder le preset"), T ("Nom du preset :"), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.isFactory (proc.presets.getCurrent()) ? juce::String() : proc.presets.getCurrent());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<VckEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        if (safe->proc.presets.save (w->getTextEditorContents ("name"))) { safe->refreshPresets(); safe->showMessage (T ("Preset sauvé")); }
        else safe->showMessage (T ("Nom invalide (ou nom d'un preset d'usine)"));
    }), true);
}

void VckEditor::importPresets()
{
    chooser = std::make_unique<juce::FileChooser> (T ("Importer des presets vck"), juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.preset");
    juce::Component::SafePointer<VckEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const int n = safe->proc.presets.importFiles (fc.getResults());
        if (n > 0) { safe->refreshPresets(); safe->showMessage (juce::String (n) + T (" preset(s) importé(s)")); }
    });
}

void VckEditor::importMidi()
{
    chooser = std::make_unique<juce::FileChooser> (T ("Choisis un fichier MIDI"), juce::File(), "*.mid;*.midi");
    juce::Component::SafePointer<VckEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto f = fc.getResult();
        if (f.existsAsFile()) safe->filesDropped ({ f.getFullPathName() }, 0, 0);
    });
}

bool VckEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi") || f.endsWithIgnoreCase (".preset")) return true;
    return false;
}

void VckEditor::filesDropped (const juce::StringArray& files, int, int)
{
    roll.dropHover = dragHover = false;
    juce::Array<juce::File> presetFiles;
    for (auto& path : files)
    {
        if (path.endsWithIgnoreCase (".preset")) { presetFiles.add (juce::File (path)); continue; }
        if (! (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi"))) continue;
        juce::String err;
        if (proc.loadMidi (juce::File (path), err))
        {
            // importer un MIDI = vouloir l'entendre : source MIDI + lecture (une etape d'undo)
            proc.undo.beginNewTransaction ("MIDI");
            proc.presets.setTreeValue ("source", 0.f, &proc.undo);
            proc.presets.setTreeValue ("clipOn", 1.f, &proc.undo);
            proc.undo.beginNewTransaction();
            showMessage (T ("MIDI chargé : ") + juce::File (path).getFileNameWithoutExtension() + T ("  -  calé sur le tempo"));
        }
        else showMessage (err);
    }
    if (! presetFiles.isEmpty() && proc.presets.importFiles (presetFiles) > 0) { refreshPresets(); showMessage (T ("Preset(s) importé(s)")); }
    repaint();
}

void VckEditor::showMessage (const juce::String& m) { message = m; messageTicks = 15 * 4; repaint (msgArea); }

void VckEditor::timerCallback()
{
    if (proc.presets.getCurrent() != shownPreset)
    {
        shownPreset = proc.presets.getCurrent();
        presetNameBtn.setButtonText (shownPreset + T ("  ▾"));
        browser.syncSelection();
        repaint (infoArea);
    }
    const int slot = proc.getABSlot();
    for (int i = 0; i < slotBtns.size(); ++i)
    {
        slotBtns[i]->setToggleState (i == slot, juce::dontSendNotification);
        slotBtns[i]->setAlpha (proc.slotHasState (i) ? 1.f : 0.5f);
    }
    // une etape d'undo par geste (rien tant que la souris est enfoncee)
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) proc.undo.beginNewTransaction();
    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());

    // doubleur : voix 3 et 4 estompees en mode 2 voix
    const int dv = (int) proc.apvts.getRawParameterValue ("dblVoices")->load();
    if (dv != shownVoices)
    {
        shownVoices = dv;
        for (int i = 0; i < dblVoiceKnobs.size(); ++i)
        {
            const int voiceIdx = i / 4;
            const bool on = dv == 2 || (dv == 1 && voiceIdx < 2);
            dblVoiceKnobs[i]->setAlpha (on ? 1.f : 0.35f);
        }
        if (page == pgDoubler) repaint (dblColsArea);
    }
    if (messageTicks > 0 && --messageTicks == 0) repaint (msgArea);
    repaint (statusArea);
    repaint (midiInfoArea);
    if (page == pgAuto) repaint (autoInfoArea);
}

void VckEditor::buildBackground (float pxScale)
{
    using namespace juce;
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    background = Image (Image::ARGB, jmax (1, roundToInt (w * pxScale)), jmax (1, roundToInt (h * pxScale)), true);
    bgScale = pxScale;
    bgPage = page;
    Graphics g (background);
    g.addTransform (AffineTransform::scale (pxScale));
    g.fillAll (ui::bg);
    const float s = (float) getWidth() / baseW;
    if (coverImg.isValid())
    {
        auto bl = ui::blurred (coverImg, w, h, 32);
        g.setOpacity (0.95f);
        g.drawImageAt (bl, 0, 0);
        g.setOpacity (1.f);
    }
    g.setColour (Colour (0xff04122a).withAlpha (0.30f));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.4f, Colours::black.withAlpha (0.65f), 0.f, (float) h, true));
    g.fillAll();
    for (int y = 0; y < h; y += 3) { g.setColour (Colours::black.withAlpha (0.08f)); g.fillRect (0, y, w, 1); }
    Random rng (1337);
    for (int i = 0, count = w * h / 30; i < count; ++i)
    {
        g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.035f));
        g.fillRect (rng.nextInt (w), rng.nextInt (h), 1, 1);
    }
    if (ravenImg.isValid())
    {
        const float rh = (float) h * 0.78f, rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        g.setOpacity (0.07f);
        g.drawImage (ravenImg, Rectangle<float> ((float) w * 0.52f - rw * 0.5f, (float) h * 0.30f, rw, rh), RectanglePlacement::stretchToFit);
        g.setOpacity (1.f);
    }
    for (auto& pnl : panels)
    {
        if (pnl.page != -1 && pnl.page != page) continue;
        drawPanelFrame (g, pnl.area.toFloat(), pnl.colour, s);
        drawPanelTitle (g, pnl.area.toFloat(), pnl.title, pnl.colour, s);
    }
}

void VckEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / baseW;
    const float pxs = jlimit (1.f, 4.f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (background.isNull() || bgPage != page || bgScale != pxs
        || background.getWidth() != roundToInt (getWidth() * pxs) || background.getHeight() != roundToInt (getHeight() * pxs))
        buildBackground (pxs);
    g.drawImage (background, getLocalBounds().toFloat());

    auto hb = header.toFloat();
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.88f), 0, hb.getY(), Colours::black.withAlpha (0.55f), 0, hb.getBottom(), false));
    g.fillRect (hb);
    g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.0f), 0, 0, ui::cyan.withAlpha (0.6f), (float) getWidth() * 0.5f, 0, true));
    g.fillRect (Rectangle<float> (0, hb.getBottom() - 1.f, (float) getWidth(), 1.f));

    // logo : le corbeau de ravenn, net au pixel pres
    auto la = logoArea.toFloat();
    if (ravenImg.isValid())
    {
        const float rh = la.getHeight() * 1.12f;
        const float rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        const float px = jmax (1.f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int tw = jmax (1, roundToInt (rw * px)), th = jmax (1, roundToInt (rh * px));
        auto box = Rectangle<float> (std::round (la.getX() * px) / px, std::round ((la.getCentreY() - rh * 0.5f) * px) / px, (float) tw / px, (float) th / px);
        if (ravenCrisp.getWidth() != tw || ravenCrisp.getHeight() != th)
        {
            Image src = ravenImg.convertedToFormat (Image::ARGB);
            while (src.getWidth() > tw * 2 && src.getHeight() > th * 2)
                src = src.rescaled (src.getWidth() / 2, src.getHeight() / 2, Graphics::highResamplingQuality);
            ravenCrisp = src.rescaled (tw, th, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            Image::BitmapData bd (ravenCrisp, Image::BitmapData::readWrite);
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x)
                    bd.setPixelColour (x, y, Colours::white.withAlpha (jlimit (0.f, 1.f, (bd.getPixelColour (x, y).getFloatAlpha() - 0.12f) * 2.4f)));
        }
        g.drawImageTransformed (ravenCrisp, AffineTransform::scale (1.f / px).translated (box.getX(), box.getY()));
        la.removeFromLeft (rw + 6.f * s);
    }
    auto word = la.removeFromTop (la.getHeight() * 0.66f);
    g.setGradientFill (ColourGradient (Colours::white, word.getX(), word.getY(), ui::cyan, word.getRight(), word.getBottom(), false));
    g.setFont (ui::font (34.f * s, true, 0.06f));
    g.drawText ("vck", word, Justification::bottomLeft);
    g.setColour (ui::dim);
    g.setFont (ui::font (8.f * s, true, 0.5f));
    g.drawText (String ("VOCODER  ") + JucePlugin_VersionString, la.withTrimmedLeft (2.f * s), Justification::topLeft);

    // description du preset (a cote des onglets)
    g.setColour (ui::dim);
    g.setFont (ui::font (10.5f * s));
    g.drawFittedText (proc.presets.getInfo (proc.presets.getCurrent()), infoArea, Justification::centredLeft, 2, 0.9f);

    auto tag = [&] (Component& c, const String& t)
    {
        if (! c.isVisible()) return;
        auto r = c.getBounds().toFloat().translated (0, -14.f * s).withHeight (13.f * s);
        g.setColour (ui::dim);
        g.setFont (ui::font (9.f * s, true, 0.3f));
        g.drawText (t, r, Justification::centred);
    };
    tag (bands, "BANDS"); tag (wave, "WAVE"); tag (mode, "MODE"); tag (octave, "OCTAVE");
    tag (key, "KEY"); tag (scale, "SCALE"); tag (voiceRange, "VOIX"); tag (harmony, "HARMONY");
    tag (sync, "SYNC"); tag (rate, "RATE"); tag (bpm, "BPM"); tag (transpose, "TRANSPOSE");
    tag (source, "SOURCE"); tag (delayTime, "TIME");

    // doubleur : titres des colonnes de voix
    if (page == pgDoubler)
    {
        for (int i = 0; i < 4; ++i)
        {
            auto r = dblColsArea.toFloat();
            const float cw = r.getWidth() / 4.f;
            auto c = Rectangle<float> (r.getX() + cw * (float) i, r.getY(), cw, 16.f * s);
            const bool on = shownVoices == 2 || (shownVoices == 1 && i < 2);
            g.setColour (on ? ui::text : ui::dim.withAlpha (0.5f));
            g.setFont (ui::font (10.f * s, true, 0.3f));
            g.drawText ("VOIX " + String (i + 1), c, Justification::centred);
            if (i > 0)
            {
                g.setColour (ui::edge.withAlpha (0.6f));
                g.fillRect (Rectangle<float> (c.getX(), r.getY() + 4.f * s, 1.f, r.getHeight() - 8.f * s));
            }
        }
    }
    if (page == pgCarrier)
    {
        g.setColour (ui::dim);
        g.setFont (ui::font (11.f * s));
        g.drawFittedText (T ("SOURCE (colonne de droite) :\n"
                             "- MIDI : joue des notes ou glisse un .mid. Sans note pendant un instant, vck suit ta voix tout seul : jamais de silence.\n"
                             "- AUTO : la porteuse suit ta voix, calée dans la gamme (onglet AUTO).\n"
                             "- SIDECHAIN : un synthé externe envoyé dans l'entrée sidechain de vck.\n\n"
                             "Le synthé de cet onglet est la porteuse en MIDI et en AUTO."),
                          carrierInfoArea, Justification::topLeft, 12, 0.9f);
    }
    if (page == pgAuto)
    {
        auto a = autoInfoArea.toFloat();
        const float hz = proc.uiPitchHz.load(), tn = proc.uiTargetNote.load();
        const bool on = proc.uiAutoActive.load();
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (a, 3.f);
        g.setColour ((hz > 0.f && on) ? ui::green : ui::dim.withAlpha (0.6f));
        g.setFont (ui::font (28.f * s, true, 0.05f));
        g.drawText (on && tn >= 0.f ? noteName (tn) : String ("--"), a.withTrimmedBottom (a.getHeight() * 0.38f), Justification::centred);
        g.setFont (ui::font (9.5f * s, true, 0.15f));
        g.setColour (ui::dim);
        g.drawText (on ? (hz > 0.f ? "VOIX " + String (hz, 0) + " Hz" : T ("EN ÉCOUTE")) : T ("AUTO EN PAUSE (MIDI JOUE)"),
                    a.withTrimmedTop (a.getHeight() * 0.62f), Justification::centredTop);
    }

    // colonne de sortie : etat
    {
        auto a = statusArea.toFloat();
        g.setFont (ui::font (9.5f * s, true, 0.12f));
        const bool nl = proc.apvts.getRawParameterValue ("noLatency")->load() > 0.5f;
        const bool autoOn = proc.uiAutoActive.load();
        const int src = (int) proc.apvts.getRawParameterValue ("source")->load();
        String l1 = src == 2 ? String ("SIDECHAIN") : (autoOn ? (src == 0 ? T ("MIDI > AUTO (pas de note)") : String ("AUTO : SUIT LA VOIX")) : String ("MIDI : NOTES"));
        String l2 = nl ? String ("NO LATENCY : 0 ms") : "HQ : " + String (proc.uiLatencyMs.load(), 1) + " ms";
        String l3 = "LIMITEUR " + String (proc.uiLimGr.load(), 1) + " dB";
        g.setColour (autoOn ? ui::green : ui::text);
        g.drawText (l1, a.removeFromTop (a.getHeight() / 3.f), Justification::centred);
        g.setColour (ui::dim);
        g.drawText (l2, a.removeFromTop (a.getHeight() / 2.f), Justification::centred);
        g.setColour (proc.uiLimGr.load() < -0.5f ? ui::cyan : ui::dim);
        g.drawText (l3, a, Justification::centred);
    }

    {
        g.setFont (ui::font (10.5f * s, true, 0.1f));
        const bool host = proc.uiHostSync.load();
        g.setColour (host ? (proc.uiHostPlaying.load() ? ui::cyan : ui::dim) : ui::text);
        g.drawText (String (proc.uiBpm.load(), 1) + " BPM  " + (host ? (proc.uiHostPlaying.load() ? "HOST  >" : "HOST  ||") : "FREE"),
                    midiInfoArea, Justification::centredRight);
    }

    if (messageTicks > 0)
    {
        g.setColour (Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (msgArea.toFloat(), 4.f);
        g.setColour (ui::cyan);
        g.drawRoundedRectangle (msgArea.toFloat().reduced (0.5f), 4.f, 1.f);
        g.setFont (ui::font (12.f * s, true, 0.05f));
        g.setColour (ui::text);
        g.drawFittedText (message, msgArea.reduced (10, 2), Justification::centred, 2);
    }
    if (dragHover)
    {
        g.setColour (ui::cyan);
        g.drawRect (getLocalBounds().reduced (roundToInt (6 * s)), roundToInt (2 * s));
    }
    if (proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f)
    {
        g.setColour (Colours::black.withAlpha (0.3f));
        g.fillRect (getLocalBounds().withTrimmedTop (header.getBottom()));
    }
}

void VckEditor::resized()
{
    using namespace juce;
    const float s = (float) getWidth() / baseW;
    auto R = [s] (float x, float y, float w, float h)
    {
        return Rectangle<int> (roundToInt (x * s), roundToInt (y * s), roundToInt (w * s), roundToInt (h * s));
    };
    panels.clear();
    auto panel = [&] (const char* t, Colour c, float x, float y, float w, float h, int pg) { panels.push_back ({ T (t), c, R (x, y, w, h), pg }); };
    auto row = [&] (std::initializer_list<Component*> cs, float x, float y, float w, float h, float step)
    {
        int i = 0;
        for (auto* c : cs) { if (c != nullptr) c->setBounds (R (x + step * (float) i, y, w, h)); ++i; }
    };

    // ---- en-tete (positions de ravenn)
    header = R (0, 0, baseW, 70);
    logoArea = R (10, 6, 240, 58);
    undoBtn.setBounds (R (254, 18, 46, 34));
    redoBtn.setBounds (R (302, 18, 46, 34));
    resetBtn.setBounds (R (350, 18, 54, 34));
    prevBtn.setBounds (R (410, 18, 28, 34));
    presetNameBtn.setBounds (R (440, 18, 180, 34));
    nextBtn.setBounds (R (622, 18, 28, 34));
    saveBtn.setBounds (R (654, 18, 56, 34));
    folderBtn.setBounds (R (712, 18, 76, 34));
    importBtn.setBounds (R (790, 18, 66, 34));
    noLatency.setBounds (R (862, 18, 108, 34));
    bypass.setBounds (R (974, 18, 86, 34));
    for (int i = 0; i < slotBtns.size(); ++i) slotBtns[i]->setBounds (R (1066 + 30.f * (float) i, 18, 30, 34));
    copyBtn.setBounds (R (1188, 18, 40, 34));
    browser.setBounds (getLocalBounds().withTrimmedTop (roundToInt (70 * s)));
    msgArea = R (370, 74, 500, 30);

    // ---- ligne 1 : vue de la voix + MIDI (toujours visibles)
    panel ("VOICE VIEW", ui::cyan, 12, 82, 796, 262, -1);
    voiceView.setBounds (R (26, 108, 770, 228));
    panel ("MIDI", ui::pink, 820, 82, 408, 262, -1);
    midiInfoArea = R (1010, 90, 206, 18);
    roll.setBounds (R (834, 112, 380, 130));
    importMidiBtn.setBounds (R (830, 248, 122, 32));
    clearMidiBtn.setBounds (R (954, 248, 70, 32));
    clipOn.setBounds (R (1026, 248, 96, 32));
    loop.setBounds (R (1124, 248, 96, 32));
    sync.setBounds (R (830, 300, 96, 30));
    rate.setBounds (R (932, 300, 104, 30));
    bpm.setBounds (R (1042, 300, 84, 30));
    transpose.setBounds (R (1132, 300, 86, 30));

    // ---- onglets
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (R (12.f + (float) i * 152.f, 354, 148, 32));
    infoArea = R (778, 354, 222, 32);

    // ---- colonne de sortie (toujours visible)
    panel ("OUTPUT", ui::red, 1008, 354, 220, 514, -1);
    source.setBounds (R (1022, 404, 192, 108));
    statusArea = R (1018, 518, 200, 54);
    row ({ &robot, &voice }, 1016, 580, 100, 132, 104);
    row ({ &width, &output }, 1016, 724, 100, 132, 104);

    // ---- VOCODER
    panel ("BANDES", ui::violet, 12, 394, 484, 474, pgVocoder);
    bands.setBounds (R (26, 438, 456, 30));
    bankView.setBounds (R (26, 478, 456, 226));
    row ({ &loFreq, &hiFreq, &sharp, &spread }, 20, 716, 114, 144, 118);
    panel ("ANALYSE", ui::cyan, 508, 394, 488, 232, pgVocoder);
    row ({ &inGain, &gate, &attack, &release }, 516, 428, 90, 140, 94);
    freeze.setBounds (R (516 + 4 * 94 + 4, 478, 84, 34));
    panel ("TIMBRE", ui::pink, 508, 636, 488, 232, pgVocoder);
    row ({ &formant, &bright, &character, &sibil, &breath }, 516, 670, 90, 140, 94);

    // ---- CARRIER
    panel ("OSCILLATEUR", ui::cyan, 12, 394, 484, 474, pgCarrier);
    wave.setBounds (R (26, 438, 456, 30));
    mode.setBounds (R (26, 494, 280, 30));
    octave.setBounds (R (316, 494, 166, 30));
    row ({ &unison, &detune, &sub, &noise }, 20, 540, 114, 150, 118);
    row ({ &glide, &vibrato, &vibRate, &velSens }, 20, 704, 114, 150, 118);
    panel ("ENVELOPPE & JEU", ui::pink, 508, 394, 488, 232, pgCarrier);
    row ({ &envA, &envR, &bendRange }, 530, 428, 140, 140, 150);
    panel ("SOURCES", ui::cyan, 508, 636, 488, 232, pgCarrier);
    carrierInfoArea = R (526, 668, 456, 190);

    // ---- AUTO
    panel ("GAMME", ui::green, 12, 394, 484, 232, pgAuto);
    key.setBounds (R (26, 438, 456, 30));
    scale.setBounds (R (26, 494, 220, 30));
    voiceRange.setBounds (R (256, 494, 226, 30));
    drone.setBounds (R (26, 552, 140, 34));
    autoInfoArea = R (186, 538, 296, 78);
    panel ("CORRECTION", ui::green, 12, 636, 484, 232, pgAuto);
    row ({ &retune, &humanize, &tracking, &hold }, 20, 670, 114, 144, 118);
    panel ("PITCH", ui::green, 508, 394, 488, 300, pgAuto);
    pitchGraph.setBounds (R (522, 424, 460, 258));
    panel ("HARMONIES", ui::green, 508, 706, 488, 162, pgAuto);
    harmony.setBounds (R (522, 762, 250, 30));
    row ({ &harmLevel, &autoTrans }, 780, 730, 104, 132, 104);

    // ---- DOUBLER
    panel ("DOUBLER", ui::violet, 12, 394, 984, 170, pgDoubler);
    dblVoices.setBounds (R (26, 430, 96, 120));
    row ({ &dblMix, &dblDirect, &dblWidth, &dblDetune, &dblDelay, &dblDrift, &dblRate, &dblHpf, &dblLpf }, 132, 424, 92, 134, 95);
    panel ("VOIX", ui::violet, 12, 576, 984, 292, pgDoubler);
    dblColsArea = R (20, 604, 968, 258);
    for (int v = 0; v < 4; ++v)
        for (int k = 0; k < 4; ++k)
        {
            const float x = 24.f + (float) v * 242.f + (float) (k % 2) * 116.f;
            const float y = 624.f + (float) (k / 2) * 120.f;
            dblVoiceKnobs[v * 4 + k]->setBounds (R (x, y, 110, 116));
        }

    // ---- EFFECTS
    panel ("COULEUR", ui::pink, 12, 394, 484, 232, pgEffects);
    row ({ &drive, &lowCut, &highCut, &comp }, 20, 428, 114, 140, 118);
    panel ("MODULATION", ui::cyan, 508, 394, 488, 232, pgEffects);
    row ({ &ensemble, &ensRate, &phaser, &phRate, &phDepth }, 516, 428, 90, 140, 94);
    panel ("DELAY", ui::violet, 12, 636, 484, 232, pgEffects);
    row ({ &delayMix, &delayFb, &delayTone }, 20, 670, 114, 140, 118);
    delayTime.setBounds (R (374, 690, 110, 30));
    pingPong.setBounds (R (374, 742, 110, 34));
    panel ("REVERB", ui::green, 508, 636, 488, 232, pgEffects);
    row ({ &reverb, &revSize, &revPre, &revDamp, &revWidth }, 516, 670, 90, 140, 94);

    background = {};
    browser.toFront (false);
    repaint();
}
