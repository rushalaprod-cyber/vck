# vck — vocoder

Vocoder VST3 / AU / application autonome, Windows et macOS (Apple Silicon + Intel).
Même univers visuel que **ravenn**.

![vck](dist/apercu-interface.png)

## Installer

Télécharge l'installeur dans la page **Releases** du dépôt :

- **Windows** : `vck-1.0.0-windows-setup.exe` → installe le VST3 (`C:\Program Files\Common Files\VST3`) et l'application.
- **macOS** : `vck-1.0.0-macos.pkg` → installe VST3, AU et l'application.
  Le plugin n'est pas notarisé : si macOS bloque l'installeur, clic droit → **Ouvrir**.

## En 30 secondes

1. Mets **vck** sur ta piste de voix.
2. Choisis la **SOURCE** (ce qui joue les notes du robot) :
   - **AUTO** : rien à faire, chante — la porteuse suit ta voix, calée dans la gamme (KEY / SCALE), avec harmonies possibles.
   - **MIDI** : joue des notes sur la piste de vck, ou **glisse un fichier `.mid`** dans la fenêtre.
     Le clip se cale tout seul sur le **BPM du DAW** (et suit la lecture), ou tourne en boucle au BPM réglé en mode **FREE**.
   - **SIDECHAIN** : un synthé externe envoyé dans l'entrée sidechain de vck.
3. Choisis un preset.

## Presets

Calés sur des sons célèbres (inspirations, pas des samples) :

| Catégorie | Presets |
|---|---|
| **DAFT PUNK** | Around The World, Harder Better Faster, Digital Love, Something About Us, One More Time, Robot Rock, Lose Yourself To Dance, Instant Crush, Television Rules The Nation |
| **CLASSIQUES** | The Robots (Kraftwerk), Mr. Blue Sky (ELO), I Thought It Was You (Herbie Hancock), Computer Love (Zapp), O Superman (Laurie Anderson), Sexy Boy (Air), Intergalactic (Beastie Boys) |
| **MODERNES** | Hide And Seek (Imogen Heap), Woods (Bon Iver), Friends / Prismizer (Francis and the Lights), Talkbox Funk, Hyperpop Robot |
| **DOUBLEUR** | Doubleur 2 voix, Doubleur 4 voix, Doubleur ADT serré, Robot + Voix doublée |
| **CRÉATIFS** | Chœur Céleste, Murmure, Basse Vocoder, Chipmunk Bot, Démon, Radio Robot |

Tes presets : bouton **SAVE** (dossier `Documents/vck/Presets`).

## Ce qu'il y a dedans

- **Vocoder** 8 à 64 bandes, filtres de **Butterworth** exacts (8e ordre en analyse, 6e en synthèse), porteuse blanchie : la voix reste intelligible quel que soit le son de la porteuse.
  FORMANT (±12 demi-tons sans changer la note), SHARP, BRIGHT, GATE, ATTACK / RELEASE, SIBILANCE (les consonnes passent), FREEZE.
- **Porteuse** : synthé polyphonique anti-aliasé (SAW, SQUARE, PULSE, SUPER, BUZZ, NOISE), unisson 7 voix, glide, vibrato, POLY / MONO / LEGATO.
- **AUTO** : détection de hauteur YIN (précision ~1 cent), 9 gammes dont DRONE (robot monotone), harmonies (octave, quinte, accord, chœur), RETUNE de 0 (autotune dur) à naturel.
- **Clip MIDI** : import `.mid`, synchronisé au tempo du DAW à l'échantillon près, ½ / 1x / 2x, transposition, boucle. Le clip est sauvé avec le projet.
- **Doubleur** façon Waves Doubler : 2 ou 4 voix, désaccord, retard, largeur, dérive naturelle, mix. Fonctionne aussi sur la voix seule (ROBOT à 0, VOIX à 100 %).
- **Effets** : ensemble (chorus de string machine), délai ping-pong calé sur le BPM, réverbe FDN.
- **Sortie** : niveau automatique (le robot sort au niveau de ta voix), limiteur doux, zéro latence.

## Compiler

```
cmake -B build
cmake --build build --config Release --target vck_VST3 vck_Standalone vckTest
build/vckTest_artefacts/Release/vckTest
```

`vckTest` rend tous les presets, vérifie la précision MIDI / BPM, le suivi de hauteur, l'intelligibilité, la performance et produit la capture `dist/apercu-interface.png`.

Chaque push compile Windows + macOS sur GitHub Actions ; un tag `v*` publie les installeurs dans une release.
