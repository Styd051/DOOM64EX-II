# DOOM 64 EX II — document de passation

Ce document résume tout le travail effectué du 31 août au 9 septembre 2026.
À transmettre en début de session Claude Code pour reprendre le projet.

---

## 1. Le projet

**Nom :** DOOM 64 EX II
**Dépôt :** https://github.com/Styd051/DOOM64EX-II (public)
**Objectif :** un moteur Doom 64 moderne, comparable au Doom 64 KEX Engine de
Nightdive (2020), en partant du code de Doom64EX.
**Licence :** GPL v2 or later (héritée de Doom64EX — non négociable).

**Auteur :** Dylan (pseudo GitHub Styd051). Développeur autodidacte, sans études
en programmation, déjà auteur d'un fork DOOM64 EX+ Enhanced (éclairage dynamique,
manette SDL3) et de Doom Builder 64 II.

---

## 2. Environnement

| | |
|---|---|
| OS | Windows 10 |
| IDE | Visual Studio 2026 Community (installé sur `D:`) |
| Compilateur | MSVC 19.51, SDK Windows 10.0.26100.0 |
| Projet | `D:\dev\doom64-modern` |
| vcpkg | `D:\dev\vcpkg` |
| GPU | NVIDIA GeForce GTX 1650, OpenGL 4.6 disponible |
| Branche de travail | `modern` |
| Remotes | `origin` = son dépôt, `upstream` = svkaiser/Doom64EX |

**Important :** toute commande `cmake` doit être lancée depuis
**Developer PowerShell for VS 2026**, pas le PowerShell normal.

---

## 3. Choix de la base — décision arrêtée, ne pas y revenir

Le dépôt `svkaiser/Doom64EX` contient trois branches et trois tags. Analyse
complète effectuée :

| | commits | dernier code | note |
|---|---|---|---|
| `master` | 289 | juillet 2018 | refactor C++ de dotfloat, v3.0.0 |
| `gfx` | 275 | août 2017 | **entièrement contenue dans `rom`**, sans intérêt |
| `rom` | **409** | **octobre 2018** | retenue |

Tags : `2.5-sourceforge` (2014, C), `before-mass-rename` (nov. 2015, le C pur de
Kaiser), `win32dep-2018-04-11` (instantané de `master` **avant** le support MSVC).

**`rom` a été choisie** : 133 commits d'avance sur `master`, absorbe tout `gfx`,
ne lui manque que 13 commits (12 documentaires + la correction d'URL de sous-module
que Dylan a refaite lui-même). Elle apporte : lecture directe de la ROM N64
(`wadgen` supprimé), support manette, nouveau système de cvars, GLAD.

---

## 4. Corrections déjà appliquées (8 commits)

Toutes committées et poussées. Ordre chronologique :

1. **`a29fcc9` — URL du sous-module FluidSynth**
   `.gitmodules` pointait vers `dotfloat/fluidsynth-lite`, dépôt supprimé.
   Corrigé vers `haleyjd/fluidsynth-lite`.

2. **`112862f` — CMake**
   `cmake_minimum_required(VERSION 2.8.12)` → `3.16` dans le CMakeLists racine.
   Bloc `if(WIN32)` de copie de DLL commenté dans `src/engine/CMakeLists.txt`
   (il cherchait les DLL dans `extern/bin/`, dossier vide sur `rom` ; vcpkg
   fournit les DLL automatiquement).

3. **`288264e` — compilation MSVC**
   - `src/engine/CMakeLists.txt` : `CXX_STANDARD` 14 → **17** (le code utilisait
     déjà des namespaces imbriqués C++17 partout)
   - `src/engine/app.cc` ligne 7 : `#include <cxxabi.h>` entouré de `#ifdef __GNUC__`
   - `src/engine/system/i_main.cc` ligne 439 : `#ifdef _MSC_VER` →
     `#if defined(_MSC_VER) && defined(_M_IX86)` (bloc `__asm` x86 refusé en x64)
   - `src/engine/game/g_actions.cc` ligne 1315 : `__builtin_trap()` → `__debugbreak()`
     sous MSVC
   - `src/engine/utility/binary_reader.hh` ligne 49 et `src/engine/image/doom.cc`
     ligne 47 : tableaux de taille variable (extension GCC) → `std::vector`
   - `src/engine/game/g_demo.cc` : ajout de `#include "wad/wad.hh"` (utilisait
     `wad::exists` et `wad::open` sans l'inclure)
   - **Conflit de nom `radix_tree`** : `prelude.hh` ligne 80 fait
     `using namespace imp;`, et `utility/radix_tree.hh` déclarait un alias
     `imp::radix_tree` homonyme du template global `::radix_tree` → symbole
     ambigu sous MSVC. Résolu en renommant l'alias en **`imp::RadixTree`**
     (6 lignes : `utility/radix_tree.hh`, `core/cvar/store.cc`,
     `core/cvar/store.hh` ×3, `core/cvar/store_iterator.hh`).
     Des `::` ont aussi été ajoutés aux `friend` dans
     `utility/radix_tree/radix_tree_it.hpp` et `radix_tree_node.hpp`.

4. **`d4e0d00` — bug du lecteur ZIP (le plus important)**
   `src/engine/wad/zip/zip_wad.cc`, `ZipLump::stream()` lisait la taille du lump
   dans **l'en-tête local du zip**, qui vaut 0 quand l'archive est écrite en mode
   flux — cas de `cmake -E tar --format=zip` (libarchive). Résultat : tous les
   lumps du pk3 revenaient vides, sans erreur (les deux vérifications d'erreur
   étaient court-circuitées par `avail_out == 0`).
   Symptômes : `MAPINFO size: 0kb`, `0 map definitions`, New Game renvoyait au
   menu, `ANIMDEFS`/`SKYDEFS` vides, police de console en rectangles blancs.
   **Corrigé** en utilisant `info_.size` (issue du répertoire central) au lieu de
   `header.uncompressed`, aux deux endroits (`String bytes(...)` et `zs.avail_out`).
   *Ce bug existe toujours dans le dépôt amont.*

5. **`3c287c4` — renommage**
   - `CMakeLists.txt` ligne 26 : `project(Doom64EX2 C CXX)` +
     `set(PROJECT_DISPLAY_NAME "DOOM 64 EX II")`
   - `src/config.hh.in` ligne 8 : `@PROJECT_NAME@` → `@PROJECT_DISPLAY_NAME@`
   - `src/engine/CMakeLists.txt` : `set_target_properties(doom64ex PROPERTIES
     OUTPUT_NAME doom64ex2)` — **la cible reste `doom64ex`**, seul le nom du
     fichier change
   - Le titre de fenêtre est assemblé dans `src/engine/system/sdl2/video.cc`
     ligne 267 à partir de `config::name` et `config::version_full`

6. **`0a6beab` — README**

**Modification temporaire non committée comme définitive :** le mot-clé `WIN32`
a été retiré de `add_executable(doom64ex MACOSX_BUNDLE WIN32 ${SOURCES})` dans
`src/engine/CMakeLists.txt`, pour obtenir une console de logs pendant le
développement. **À conserver tant qu'on développe.** Pour une version joueur,
utiliser plutôt `set_target_properties(doom64ex PROPERTIES WIN32_EXECUTABLE
$<CONFIG:Release>)`.

---

## 5. Procédure de build

```powershell
# Developer PowerShell for VS 2026
cd D:\dev\doom64-modern
cmake -B build -S . -A x64 -DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DENABLE_TESTING=OFF "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
cmake --build build --config Release
```

**Pièges connus :**
- Les guillemets autour de `"-DCMAKE_POLICY_VERSION_MINIMUM=3.5"` sont
  **obligatoires** : PowerShell coupe sinon l'argument sur le point décimal.
- Ce drapeau est nécessaire à cause du `CMakeLists.txt` du sous-module FluidSynth
  (ligne 23, `cmake_minimum_required` trop ancien).
- Chemins CMake avec des `/`, jamais des `\`.

**Paquets vcpkg installés :** `sdl2`, `sdl2-net`, `zlib`, `libpng`, `fmt`,
`boost-optional`, `boost-utility`, `boost-circular-buffer`, `boost-algorithm`,
`boost-functional`, `boost-container-hash`, `boost-lexical-cast`,
`boost-variant` — tous en `:x64-windows`.

**Filtre d'erreurs utile :**
```powershell
cmake --build build --config Release 2>&1 | Select-String ": error" | ForEach-Object { ($_.Line -replace ' \[D:.*', '').Trim() } | Sort-Object -Unique
```

**Pour lancer :** copier `build\doom64ex.pk3` et `doom64.rom` dans
`build\Release\`, puis `.\doom64ex2.exe`. Options utiles : `-devparm`
(statistiques + compteur FPS, visible seulement en jeu), `-warp 1`, `-nosound`.

---

## 6. État actuel

**Ce qui fonctionne :** compilation complète, jeu complet sur MAP01 testé, son,
~590 FPS. Le monde, le HUD, la console, les menus, l'automap, le ciel, le fondu
de fin de niveau et l'effet de melt passent tous par le chemin programmable
quand `r_Shaders 1`.

**Le moteur tourne en OpenGL 3.3 core**, sans une seule erreur GL, à ~620 FPS.
C'est le contexte par défaut depuis le 4 septembre 2026 ; `-gl14` reste comme
repli de comparaison.

**Le pipeline fixe et le pipeline programmable coexistent**, sélectionnés par la
cvar `r_Shaders`. Les deux rendent la même image ; c'est ce qui permet de
comparer à chaque étape.

**Attention aux mesures de cette passation :** tout relevé de FPS ou toute
capture étiquetés « 1920x1080 » avant le 6 septembre 2026 ont en réalité été pris
en **1536x864** — voir le bug 19, le processus n'était pas DPI-aware. Les
comparaisons restent valides, la résolution annoncée non.

**Réglages :** vsync et interpolation d'images sont désactivés par défaut dans
le `config.cfg` généré — les activer, sinon le rendu paraît saccadé malgré le
framerate élevé.

**Points ouverts constatés :**
- `DEPTH SIZE: 8` par défaut dans le menu Video (valeur normale : 24) — risque
  de z-fighting
- Gamma très bas par défaut
- ~~Audio : `SDL_OpenAudio` demande 128 échantillons et obtient 441~~ — **caduc
  depuis le 8 septembre 2026.** SDL ne possède plus le périphérique audio ; la
  sortie passe par OpenAL Soft. Voir §8, « La chaîne audio ».
- `native_ui/win32.cc` et `native_ui/win32/rom_select.cc` sont **commentés**
  dans `src/engine/CMakeLists.txt` → pas de dialogue de sélection de ROM sous
  Windows ; si `doom64.rom` est absent, `log::fatal` et arrêt
- Le `doom64ex.pk3` est généré dans `build\` et doit être copié à la main dans
  `build\Release\`
- ~~`wad::init()` ne cherche que `doom64.rom` en dur~~ — **caduc depuis le
  8 septembre 2026.** Le moteur démarre indifféremment sur `doom64.rom` ou sur
  le `doom64.wad` du remaster, textures et son compris. Voir §8, « Le
  `doom64.wad` du remaster comme IWAD » puis « La chaîne audio ».

- ✅ **Textures blanches — RÉSOLU le 5 septembre 2026.**

  **Cause.** `texturewidth[]` et `textureheight[]` ne sont écrits qu'à **un seul
  endroit** : la branche d'upload de `GL_BindWorldTexture` (`gl_texture.cc`,
  ~ligne 342). Et cette fonction commence par appliquer la traduction :

  ```c
  texnum = texturetranslation[texnum];
  ```

  Or `R_PrecacheLevel` appelle `GL_BindWorldTexture(i)` pour chaque texture du
  niveau. Pour une texture animée dont l'animation est **en cours** — ce qui est
  le cas de toutes dès qu'on a joué un niveau ou deux — la traduction envoie sur
  la frame affichée, disons `i+1`. La frame est montée, `texturewidth[i+1]` est
  renseignée, et **`texturewidth[i]` reste à zéro.**

  `R_RenderWall` (`r_bsp.cc:506`) lit `texturewidth[sidedef->midtexture]`,
  c'est-à-dire **l'index de base**, jamais la frame. Puis divise par lui :

  ```c
  v[1].tu = length / width + coloffs;          // length / 0
  v[2].tv = rowoffs + (top - bottom) / height; // / 0
  ```

  Division par zéro en flottant : coordonnées de texture infinies, le mur
  échantillonne un seul texel, **aplat pâle**. Dans les deux chemins de rendu,
  puisque `texturewidth` est lu dans le BSP et pas dans le rendu.

  **Ce que ça explique.** Seules les textures animées sont touchées (les seules
  dont la traduction n'est pas l'identité) ; seule la **frame de base** l'est ;
  le jeu de victimes change à chaque sauvegarde parce qu'il dépend de la phase
  de l'animation au moment du précache ; et `-warp 2` ne reproduisait rien parce
  qu'on arrive alors avec les animations encore à l'identité.

  **Correctif** (`r_main.cc`) : forcer l'identité le temps du précache, pour
  monter la texture elle-même et pas la frame qu'elle affiche.

  ```c
  word saved = texturetranslation[i];
  texturetranslation[i] = i;
  GL_BindWorldTexture(i, 0, 0);
  texturetranslation[i] = saved;
  ```

  **Démonstration.** L'option **`-animdesync`** avance toutes les animations
  d'une frame avant le précache, ce qui reproduit à volonté l'état d'un vrai
  parcours. Colonne SIZE d'`animdump`, MAP02 :

  | animations désynchronisées | SMONAA | SMONCA | SMONDA | SMONEA |
  |---|---|---|---|---|
  | sans le correctif | **0x0** | **0x0** | **0x0** | **0x0** |
  | avec le correctif | 64x64 | 64x64 | 64x64 | 64x64 |

  **Fragilité qui demeure.** `texturewidth[]` reste un cache rempli par effet de
  bord d'un upload : une texture rendue sans avoir été précachée diviserait
  encore par zéro. `InitWorldTextures` porte d'ailleurs un commentaire orphelin,
  « read PNG and setup global width and heights », dont le code a disparu — c'est
  là que les dimensions devraient être posées, pour les 503, une fois pour
  toutes. À faire un jour.

  **Deux fausses pistes, écartées par la mesure avant d'arriver là** (ne pas les
  refaire) : le créneau `textureptr[i][0]` laissé non initialisé par `Z_Malloc`
  (faux — `InitWorldTextures` le met à zéro explicitement trois lignes plus bas),
  et un nom GL périmé relu comme déjà chargé (`-texcheck` : 0 sur 95 sur MAP02).

  **Outils ajoutés :** `animdump` (état complet de chaque animation : index,
  frame affichée, palette, **taille**, nom GL, validité OpenGL, entrée d'atlas),
  `sectordump` (chaque ligne du secteur du joueur avec les textures de ses deux
  côtés), `-texcheck`, `-animdesync`.

- ~~**Texture SMONCA blanche sur la map02 — bug amont, hors périmètre.**~~
  *(Historique ci-dessous, conservé pour la méthode.)*
  Une surface s'affiche en aplat pâle là où SMONCA devrait être. Ce n'est ni
  le profil core ni le chemin programmable : le défaut se reproduit à
  l'identique avec `r_Shaders 0`, pipeline fixe pur, sans shader ni atlas.

  Ce qui a été écarté par la mesure, et qu'il est inutile de refaire :

  | | |
  |---|---|
  | SMONCA..SMONCD dans l'atlas | présentes, 64x64, valides |
  | Pixels blancs dans les images source | 0 sur 4096 (premier texel `ff5a6373`) |
  | Textures du monde sans entrée d'atlas valide | 0 sur 503 |
  | Textures que le chemin fixe n'arrive pas à monter en VRAM | 0 sur 503 |

  Donc la donnée est bonne, l'atlas la stocke fidèlement, et les deux chemins
  savent la charger.

  **La piste « la surface ne référence pas SMONCA » est fausse — corrigée le
  4 septembre 2026.** Les sidedefs de MAP02 ont été décodés directement depuis
  le `DOOM64.WAD` de Nightdive : les références de texture y sont des `LumpHash`
  16 bits, et les 8 940 du niveau se résolvent toutes. **SMONCA y apparaît cinq
  fois, et les cinq sont des `midtexture`** (sides 2549, 2550, 2560, 2771, 2773).

  C'est la vraie piste : le chemin des textures médianes sur ligne à deux côtés,
  pas la donnée. Test décisif à faire — **MAP03 utilise SMONCA neuf fois en
  `toptexture`/`bottomtexture`** (sides 120, 122, 573, 910, 912, 1379, 1443,
  1516, 1526). Si elle s'y affiche correctement, le défaut est isolé au chemin
  midtexture. Ailleurs : MAP04 (2, mid), MAP08 (8, mid), MAP25 (4, mid).

  Réserve : ces données viennent du WAD de Nightdive, pas de la ROM. Les deux
  décrivent le même niveau et le WAD en dérive, mais ce n'est pas une preuve
  formelle pour le chemin ROM.

  **Session du 5 septembre 2026 — ce qui est désormais écarté par la mesure.**

  Dylan a fourni l'observation décisive : sur MAP02, cinq SMONCA et une SMONEA
  sortent blanches, et **après une sauvegarde puis un rechargement, SMONEA
  redevient correcte tandis que les cinq SMONCA restent blanches.** Les six sont
  des midtextures, groupées dans le même couloir.

  Deux pistes ont été suivies et **toutes deux invalidées** :

  1. *« Le créneau `textureptr[i][0]` est laissé non initialisé par `Z_Malloc`. »*
     **Faux.** `InitWorldTextures` fait bien `textureptr[i][0] = 0;` trois lignes
     plus bas. J'avais lu la fonction jusqu'à la ligne de l'allocation et pas
     au-delà, et annoncé la cause trop vite. Le code est revenu à l'identique.
  2. *« Un nom GL périmé est relu comme déjà chargé. »* Mesuré avec la nouvelle
     option **`-texcheck`**, qui vérifie après le précache que chaque nom stocké
     dans `textureptr` est bien un objet texture connu d'OpenGL :
     **0 sur 95 sont faux sur MAP02.**

  La nouvelle commande **`animdump`** imprime l'état complet de chaque animation
  ANIMDEFS : index, frame affichée, palette, nom GL, validité OpenGL, entrée
  d'atlas. Au précache de MAP02, **tout est correct** — les groupes d'index sont
  contigus et justes (SMONCA 249-252, SMONEA 257-260), les textures sont
  montées, les entrées d'atlas valides en 64x64.

  **`animdump` exécuté en jeu, face aux murs blancs, dans les deux profils.**
  Dylan l'a fait en `-gl33` et en `-gl14`. Résultat : **le système d'animation
  est sain dans les deux cas.**

  - toutes les entrées d'atlas sont valides, en 64x64 (16x128 / 32x16 / 32x32
    pour les strakes et les interrupteurs) ; **aucune entrée vide** ;
  - les groupes d'index sont contigus et corrects : SMONAA 241-244,
    SMONBA 245-248, SMONCA 249-252, CFACEA 111-113, SMONDA 253-256,
    SMONEA 257-260, HTELA 223-226 ;
  - la colonne SHOW avance bien d'une frame à l'autre entre deux relevés :
    l'animation tourne.

  Deux choses dans ce dump ressemblent à des anomalies et n'en sont pas :

  1. **`GLNAME 0` sur certaines lignes**, dont SMONCA. Avec l'atlas actif,
     `GL_BindWorldTexture` n'est jamais appelé pour les murs, donc `textureptr`
     n'est rempli que pour les textures touchées par un autre chemin. Un zéro y
     signifie « jamais montée en VRAM par le chemin fixe », pas « cassée ».
  2. **L'entrée d'atlas d'une ligne de base est celle de la frame affichée**,
     pas la sienne : `atlas_world()` applique `texturetranslation` lui-même.
     C'est un défaut de présentation de l'outil, pas du moteur.

  **Et le blanc est présent dans les deux profils**, `-gl33` comme `-gl14`, au
  même endroit de l'écran. Cela écarte l'atlas et le chemin programmable en
  entier : la cause est en amont du rendu.

  **La question qui reste est donc antérieure à tout ça :** cette surface
  porte-t-elle vraiment SMONCA ? L'affirmation vient du `DOOM64.WAD` de
  Nightdive, pas de la ROM que le moteur charge. La commande **`sectordump`**
  imprime, pour le secteur où se tient le joueur, chaque ligne et les trois
  textures de chacun de ses côtés, avec leurs noms. C'est la prochaine mesure.

- **Test de régression sur les 32 maps : fait le 4 septembre 2026**, par Dylan,
  en `-gl33 -glcheck`. Trois bugs remontés : la diagonale des menus, les textures
  animées, la fin de MAP28 — les trois corrigés (voir §7ter, bugs 14 à 16). Seul
  SMONCA reste, et il est amont.

---

## 7. Architecture à connaître

**Couches :** `playloop/` (~20 k lignes, logique de jeu, du C dans des `.cc`),
`common/` (11 k), `misc/` (7 k), `net/` (6 k), `renderer/` (5,6 k), `opengl/`
(4,1 k) + `opengl/shader/` (3,9 k, nouveau), `wad/` (sous-dossiers `doom/`,
`rom/`, `zip/`), `image/`, `core/cvar/`, `system/sdl2/`, `utility/`, `platform/`.

**Chemin de rendu :** `r_main` → `r_bsp` → `r_clipper` (culling de frustum) →
`r_drawlist` (trois listes : `DLT_WALL`, `DLT_FLAT`, `DLT_SPRITE`) →
`dglDrawGeometry` → soit le pipeline fixe, soit `shader::draw_geometry`.

**Densité C++ réelle** (hors bibliothèque tierce) : ~0,99 classe pour
1000 lignes, 0,70 `virtual`/`override`. L'essentiel du moteur reste du C
procédural compilé en C++, et **c'est normal** — mesuré sur UZDoom (fork actif
de GZDoom, 613 k lignes), son `playsim` de 96 689 lignes ne compte que
74 classes et 28 `virtual`. Personne ne convertit le playloop de Doom en objets.
Écrire du C++ moderne dans les **nouveaux** modules, ne pas réécrire l'ancien
sans raison.

---

## 7bis. Le sous-système shader — `src/engine/opengl/shader/`

Écrit du 1er au 5 septembre 2026. **5 890 lignes, non committées à ce jour.**

| fichier | lignes | rôle |
|---|---|---|
| `postprocess.cc/.hh` | 1 849 | le framebuffer hors écran et toute la chaîne : FXAA, G-buffer, flou de mouvement, SAO, SMAA, flou de menu, mise à l'échelle |
| `preprocessor.cc/.hh` | 895 | aplatit les `#include` depuis le pk3, évalue les conditionnelles, garde une carte des lignes |
| `draw.cc/.hh` | 820 | les quatre programmes monde, VBO/IBO en anneau, uniformes, matrices de l'image précédente |
| `atlas.cc/.hh` | 740 | tableau de textures 1024×1024, TMEM N64, palettes |
| `program.cc/.hh` | 543 | `Program::build()`, compilation des deux étages, commandes console |
| `gl33.cc/.hh` | 391 | points d'entrée GL 3.3 résolus par SDL |
| `matrix.cc/.hh` | 387 | pile de matrices du moteur |
| `glstate.cc/.hh` | 368 | miroir de l'état fixe (fog, alpha test, `GL_TEXTURE_2D`) |
| `immediate.cc/.hh` | 234 | émulation du mode immédiat |

### Le préprocesseur

GLSL n'a pas de `#include` : les sources doivent être aplaties avant d'atteindre
le pilote. C'est presque tout ce que fait ce module — le pilote reste maître de
l'expansion des macros, `##` compris (vérifié par `tools/glslpp_test`, 11 tests,
tous passent sur NVIDIA).

Les conditionnelles font exception : `common.inc` choisit son backend avec
`#if defined(__ORBIS__)` / `#elif defined(HLSL)` / `#else`, et l'un des fichiers
visés (`common_pssl.inc`) **n'est pas livré**. Suivre toutes les branches
échouerait. Elles sont donc évaluées ici — uniquement pour décider quel
`#include` suivre ; les directives elles-mêmes passent intactes.

Deux mécanismes ajoutés :

- **`fragment_outputs(n)`** — sur le chemin GL, `common_glsl.inc` laisse
  `def_var_fragment` et `def_var_pixelTarget` **vides** (seul Vulkan déclare),
  alors que les shaders écrivent bien dans `outFragment0..N`. Les déclarations
  sont injectées dans le prologue.
- **`inject_after(after, inject)`** — splice un second fichier juste après qu'un
  `#include` donné est résolu. C'est ce qui permet d'adapter un shader KEX
  **sans en modifier une ligne** : `progs/d64ex/overrides.inc` est inséré
  derrière `progs/common.inc`, et redéfinit `outReturn` pour y placer le
  `discard` de l'alpha test.

`translate_log()` réécrit les diagnostics du pilote (`0(651)` chez NVIDIA,
`0:651` chez Mesa/AMD) en `progs/doomSceneMain.shader:274`.

### L'atlas de textures

`doomSceneMain` lit un `sampler2DArray` où chaque texture est rangée
**linéairement**, ses lignes bout à bout dans une page de 1024×1024 — c'est
l'adressage TMEM de la N64. `GetRemappedCoordinates` reconstruit les
coordonnées de page. L'atlas du jeu : **1813 images, 9 couches, 36 Mo**,
vérifié octet par octet via un aller-retour GPU (`texatlasverify`).

### Les miroirs

Méthode appliquée à chaque sous-système du pipeline fixe : **dupliquer, puis
autotester contre l'original, puis basculer.** `dgl.h` porte trois blocs de
macros (MATRIX STACK, FIXED-FUNCTION STATE MIRROR, IMMEDIATE MODE) où chaque
macro alimente les deux à la fois.

Le garde `D64_NO_FIXED_FUNCTION` / `D64_FF()` coupe la moitié OpenGL de chacune.
Le build `build-noff` est configuré avec ce drapeau :

```bash
MSYS2_ARG_CONV_EXCL='*' cmake -B build-noff -S . -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=OFF "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" \
  -DCMAKE_CXX_FLAGS_RELEASE="/O2 /Ob2 /DNDEBUG /DD64_NO_FIXED_FUNCTION"
```

(`MSYS2_ARG_CONV_EXCL='*'` est obligatoire sous Git Bash, qui transforme sinon
`/O2` en chemin.)

**Résultat de la répétition générale :** le jeu rend correctement, monde
compris, **sans un seul appel au pipeline fixe**. 590 FPS (`build`) contre
598 FPS (`build-noff`), 0 erreur GL des deux côtés.

`dglEnable` / `dglDisable` sont volontairement **hors** de `D64_FF` : ils
servent aussi à `GL_BLEND`, `GL_DEPTH_TEST`, `GL_CULL_FACE`, `GL_SCISSOR_TEST`,
tous valides en core.

### Les shaders dans `distrib/doom64ex.pk3/progs/`

Les **36 fichiers du `Doom64.kpf` copiés verbatim**, plus trois fichiers du
moteur dans `progs/d64ex/` :

- `generic.shader` — tout ce qui n'est pas de la géométrie de monde : HUD,
  console, menus, automap, ciel, melt. Ces chemins lient une texture 2D simple
  et s'appuient sur l'environnement de texture (`GL_ADD`, `GL_REPLACE`) et le
  brouillard du moteur, que les shaders KEX ignorent. C'est pour cela que
  `doomSceneMain` ne peut pas les servir.
- `overrides.inc` — la redéfinition de `outReturn` (alpha test → `discard`).
- `world.shader` — shader de transition, **remplacé** par
  `progs/doomSceneNoFilter.shader`. Conservé comme référence.

Le monde est rendu par **`progs/doomSceneNoFilter.shader`, le shader KEX
inchangé**. `progs/doomSceneFilter.shader` (filtre 3 points de la N64) est un
changement d'une ligne dans `draw.cc`.

### Outils de diagnostic ajoutés

| | |
|---|---|
| `shaderdump <chemin> [pixel]` | aplatit un shader et l'imprime, chaque ligne étiquetée de son origine |
| `shaderbuild` | recompile les programmes |
| `texatlasinfo` / `build` / `dump` / `verify` | inspection de l'atlas |
| `-logfps` | recopie le compteur FPS dans le log |
| `-screenshot <n>` | écrit `sshot000.png` à la n-ième image présentée |
| `-mtxcheck` | autotest de la pile de matrices |
| `-shaderalpha` | coupe le vrai `GL_ALPHA_TEST` autour du draw : seul le `discard` du shader filtre |
| `-gl14` | repli sur le contexte 1.4 de compatibilité ; le core 3.3 est le défaut |
| `r_N64Filter` | cvar : filtre 3 points de la N64 (`doomSceneFilter` au lieu de `doomSceneNoFilter`) |
| `r_FXAA` | cvar : 0 aucun, 1 `progs/fxaa.shader`, 2 `progs/fxaa_fast.shader` |
| `r_GBuffer` | cvar : écrit le G-buffer couleur / profondeur / vélocité |
| `r_GBufferShow` | cvar : 0 normal, 1 profondeur, 2 vélocité, 3 masque — **non sauvegardée** (`Flag::noconfig`) |
| `r_MotionBlur` | cvar : 0 aucun, 1 flou par objet, 2 avec la grille de tuiles |
| `r_MotionBlurScale` | cvar : portée du flou |
| `r_SAO` | cvar : 0 aucun, 1 occlusion ambiante, 2 affiche l'occlusion brute |
| `r_MaxOcclusionUnit` | cvar : rayon d'échantillonnage (nom repris de KEX) |
| `r_SAOIntensity` | cvar : force de l'assombrissement |
| `r_SMAA` | cvar : 0 aucun, 1 SMAA, 2 affiche les contours, 3 les poids de mélange |
| `r_MenuBlur` | cvar : rayon en pixels du flou derrière le menu, 0 éteint |
| `r_ResolutionScale` | cvar : fraction de la fenêtre à laquelle la scène est rendue, 1 = natif |
| `r_Brightness` | cvar : luminosité de l'environnement, multiplie l'image du monde, 1 = neutre |
| `-shottic <n>` | écrit `sshot000.png` au n-ième **tic du niveau**, puis quitte proprement |
| `-glcheck` | purge la file d'erreurs GL une fois par image et signale chaque code distinct la première fois |

`-screenshot` et `-glcheck` ont été décisifs plusieurs fois. `-screenshot`
surtout : il permet de **voir** le résultat sans dépendre d'une capture
manuelle.

**`-shottic` est la version comparable de `-screenshot`, et la seule utilisable
pour un avant/après.** Trois choses le distinguent, chacune apprise en se
trompant :

- Il compte des **tics**, pas des images. Le nombre d'images qui tiennent dans
  un tic dépend du framerate, donc activer un effet décale le moment du jeu où
  une image donnée tombe : les deux captures diffèrent alors pour des raisons
  qui n'ont rien à voir avec l'effet.
- Il compte `leveltime` et non `gametic`. `gametic` inclut tout ce qui précède
  le niveau — chargement du wad, montée de l'atlas, compilation des shaders — et
  rien de cela ne prend deux fois le même temps.
- **Il quitte après la capture.** Sans cela le processus doit être tué de
  l'extérieur, le moteur n'atteint jamais `M_SaveDefaults`, et `config.cfg` est
  laissé tronqué. Ce n'est pas un problème de propreté : chaque essai repart
  d'un état différent du précédent, ce qui détruit silencieusement toute
  comparaison. C'est arrivé, et cela a coûté une bonne partie d'une session.

Résultat mesuré : deux captures successives à `-shottic 140` sur MAP01 sont
**identiques au pixel près** (0 pixel différent sur 2 073 600). Avant tic 100
le fondu d'entrée du niveau laisse encore un peu de bruit ; à tic 40 il reste
0,03 % des pixels, ce qui suffit rarement.

**Deux réglages faussent la mesure et doivent être conscients :**
`v_VSync 1` plafonne tout à 60 FPS et rend toute comparaison de coût inutile ;
`i_InterpolateFrames 0` empêche le moteur de rendre plusieurs images par tic et
plafonne aussi. Pour mesurer un coût : vsync **éteint**, interpolation
**allumée**. Pour comparer deux images : l'inverse n'est pas nécessaire, le tic
suffit.

---

## 7ter. Bugs trouvés et corrigés pendant ce travail

Ceux qui ont demandé un vrai diagnostic, à ne pas réintroduire :

1. **Collisions de noms de lumps à 8 caractères.** `progs/common.inc`,
   `common_glsl.inc` et `common_hlsl.inc` sont tous les trois `COMMON`. Le
   dernier chargé écrasait les autres. Résolu par **`wad::open_path()`** :
   recherche par chemin réel, avec conservation des lumps évincés
   (`shadowed_lumps_` dans `wad/device.cc`) pour ne pas décaler les index de
   section.

2. **`ILump::read_bytes()` calculait `begin − end`** — lançait toujours.
   Opérandes inversés. Bug amont préexistant.

3. **GLAD généré avec `--omit-khrplatform`** : son `GLsizeiptr` retombe sur
   `long`, donc **32 bits sous MSVC x64**. Les tailles de buffer étaient
   tronquées. `gl33.hh` définit ses propres typedefs :
   ```cpp
   using sizeiptr = std::ptrdiff_t;
   using intptr   = std::ptrdiff_t;
   ```

4. **Deux chemins de présentation d'image** — `d_main.cc` contournait
   `GL_SwapBuffers`. Unifiés.

5. **`dglTranslated` manquant** dans `dgl.h` : le dôme du ciel (`r_sky.cc:183`)
   contournait silencieusement la pile du moteur → 25 % de divergence, toujours
   la même valeur, 651.89856. Les **six** variantes double précision ont été
   ajoutées.

6. **`GL_TEXTURE_2D` est par unité de texture**, pas global. Le miroir divergeait
   à 50 % avant l'ajout du crochet sur `glActiveTextureARB` et du tableau par
   unité (`MAX_UNITS = 4`).

7. **Écrans blancs** (menu de skill, pause, berserk, console). Cause : j'avais
   *remplacé* `dglColor4ub` au lieu de le **doubler**, donc `glRect*` dessinait
   avec le blanc par défaut de GL. Neuf sites concernés. Leçon : les sommets
   pouvaient être remplacés (`glVertex` hors `glBegin` est une erreur), **les
   couleurs non**.

8. **Monde noir en build noff, `DRAW LIST WALL USAGE: 0 KB`.**
   `r_clipper.cc:288` relisait les matrices **depuis OpenGL** via
   `dglGetDoublev` pour le culling. Avec la pile fixe figée il recevait
   l'identité et éliminait le niveau entier. C'est une *lecture*, qu'aucun grep
   des macros d'écriture n'aurait montrée. Lit maintenant
   `shader::matrix_get()`.

9. **L'alpha test du shader n'a jamais existé.** `preprocessor.cc` faisait
   `m_injections.erase(inj)` après avoir splicé l'override : l'injection était
   consommée **définitivement**, alors que le même `Preprocessor` aplatit quatre
   sources (vertex + pixel × deux programmes). `overrides.inc` n'atteignait donc
   que l'étage *vertex* du premier programme, où il est entièrement sous
   `#ifdef SHADER_PIXEL` — donc vide. `uAlphaTest` valait `loc = -1` partout, et
   `glUniform2f(-1, …)` est un no-op silencieux.
   Invisible parce que le vrai `GL_ALPHA_TEST` faisait tout le travail. Révélé
   par le build noff, qui coupe `glAlphaFunc` : GL restait sur son défaut
   `GL_ALWAYS`/0, et les texels transparents des grilles sortaient en noir.
   L'injection se réarme désormais **par exécution** (`m_injected`, vidé dans
   `process()`).

10. **Anisotropie réglée à 0.** `GL_SetTextureFilter` passait 0 à
    `GL_TEXTURE_MAX_ANISOTROPY_EXT` quand `r_Anisotropic` est à 0, alors que
    la plage légale est [1, max]. `GL_INVALID_VALUE` à chaque appel, dans les
    **deux** profils — bug amont, invisible jusqu'à ce que `-glcheck` existe.
    Et `max_anisotropic` était interrogé *après* le premier appel, donc le
    premier passait 0 dans tous les cas. Les deux corrigés.

11. **Le scan d'extensions de GLAD.** Généré pour 1.4, il fait un seul
    `glGetString(GL_EXTENSIONS)` — `GL_INVALID_ENUM` en core, et **tous** les
    `GLAD_GL_*` à faux, ce qui désactivait le filtrage anisotrope sans raison
    et affichait « GL_ARB_multitexture not supported ». Les trois drapeaux
    consultés sont recalculés depuis `glGetStringi`.

12. **`glGetStringi` pas encore résolu.** Le recalcul ci-dessus s'exécute dans
    `GL_Init`, alors que `gl33::load()` n'était appelé qu'à la toute fin, dans
    `shader::init()`. Pointeur nul, crash à l'init — et le symptôme était un
    log qui s'arrête sans rien dire. J'ai d'abord lu ce silence comme un
    succès. `gl33::load()` est maintenant appelé au début de `GL_Init`, et
    `GL_ExtensionString` refuse poliment si le point d'entrée manque.

13. **Tous les sprites du jeu trop grands en core.** Cascade en trois temps,
    partie d'un correctif que j'avais fait à moitié : le scan d'extensions de
    GLAD laisse `GLAD_GL_ARB_texture_non_power_of_two` à faux en core, ce qui
    fait forcer `r_TexNonPowResize = 1` dans `GL_BindSpriteTexture`, ce qui
    fait compléter l'image à la puissance de 2 dans `SetTextureImage` — qui
    **réécrit les dimensions** dans `spritewidth[]` / `spriteheight[]`. Or ce
    sont elles qui donnent la taille du sprite **dans le monde**. Un sprite
    40x56 devient 64x64.

    J'avais recalculé quatre des six drapeaux `GLAD_GL_*` que le moteur
    consulte, en croyant traiter un problème cosmétique. Les six sont
    maintenant dans une table dans `GL_Init`, pour que l'oubli suivant ne soit
    pas possible.

    **`r_TexNonPowResize` est une cvar sauvegardée.** Une seule session en core
    avant ce correctif suffit à l'écrire à 1 dans `config.cfg`, et les sprites
    restent grands *dans les deux profils* jusqu'à ce qu'on la remette à 0.
    C'est ce qui rend ce bug déroutant : le symptôme survit à la cause. Une
    sonde de capacité matérielle qui écrit une préférence utilisateur
    persistante est un défaut de conception amont, à revoir un jour.

14. **Diagonale blanche dans les panneaux des menus Save/Load.**
    `M_DrawSaveGame` passe en `glPolygonMode(GL_LINE)` puis dessine trois
    `dglRecti`. Le pipeline fixe traçait le contour d'un quad ; l'émulation du
    mode immédiat en fait **deux triangles**, et en filaire l'arête partagée
    devient visible. `imm_end` émet maintenant un `GL_LINE_LOOP` quand le mode
    de polygone est `GL_LINE` — le contour est tracé une fois, et l'arête ne
    peut plus réapparaître.

15. **Toutes les textures animées figées, certaines en aplat de couleur.**
    ANIMDEFS anime de deux façons : en avançant vers le lump suivant
    (`texturetranslation`) ou en changeant de palette **dans le même lump**
    (`GL_SetNewPalette` → `palettetranslation`). `GL_BindWorldTexture` applique
    les deux ; `atlas_world()` n'en appliquait aucune, et l'atlas ne stockait
    même que la palette 0 des textures du monde.

    D'où les deux symptômes : animation figée, et pour les textures
    palette-animées — écrans d'ordinateur, visages de démon — un **aplat**,
    parce que leur palette 0 est l'état éteint.

    L'atlas porte désormais les palettes des textures du monde comme il le
    faisait déjà pour les sprites (1813 → **1856 images**, toujours 9 couches
    et 36 Mo), et `atlas_world()` applique les deux indirections.
    `texatlasverify` passe sur les 1856.

16. **Fin de MAP28 : ni texte de cluster, ni cast sequence.** Deux défauts
    amont superposés, aucun lié au rendu.

    D'abord, `WI_Ticker` et `IN_Ticker` renvoient `dboolean`, qui est un
    `typedef bool` : `return ga_victory` (6) et `return ga_finale` (7) se
    seraient écrasés en 1, c'est-à-dire `ga_loadlevel`. Les deux lignes avaient
    donc été **commentées** au lieu d'être adaptées, et l'écran de texte de
    cluster ne se déclenchait plus jamais, sur aucune map. `D_MiniLoop` relit
    `gameaction` juste après le ticker et le laisse gagner, donc l'action passe
    par là.

    Ensuite, `IN_Start` prenait toujours `P_GetCluster(nextmap)`. Après MAP28,
    `nextmap` vaut 29, et MAP29 appartient au **cluster 2** : le moteur
    affichait le texte d'arrivée du cluster 2 au lieu du texte de fin du
    cluster 6. Comme le cluster 2 n'a pas `scrolltextend`, `IN_Finish`
    demandait un niveau, jamais le cast. `IN_Start` fait maintenant la même
    distinction que `WI_Ticker` : le cluster quitté quand il porte un
    `EXITTEXT`, sinon le cluster rejoint.

17. **`GL_INVALID_VALUE` pendant le rendu du monde, après quelques secondes de
    jeu.** Bug amont, révélé par `-glcheck` et localisé au rendu du monde par
    une sonde temporaire. Il se produisait dans les trois configurations —
    core + shaders, 1.4 + shaders, 1.4 + pipeline fixe pur — donc ni le profil
    core ni le chemin programmable n'y étaient pour quelque chose.

    Le délai avant apparition (2 à 15 s selon la configuration) désignait les
    textures animées : rien d'autre ne change dans une scène immobile.

    `P_InitSpecials` agrandit `textureptr[lump]` par `Z_Realloc` pour les
    textures animées par palette. `InitWorldTextures` n'avait alloué qu'un seul
    emplacement, mis à zéro ; les suivants arrivaient donc avec le contenu
    précédent de cette mémoire. `GL_BindWorldTexture` les lit comme des noms de
    texture déjà chargés :

    ```c
    if(textureptr[texnum][palettetranslation[texnum]]) {
        dglBindTexture(GL_TEXTURE_2D, textureptr[texnum][...]);
    ```

    Les nouveaux emplacements sont mis à zéro. Cela a suffi pour le chemin
    programmable, **mais pas pour le pipeline fixe** : j'ai d'abord annoncé le
    contraire, sur la foi d'un test dont la sortie avait été avalée par le
    `timeout` avant d'être écrite. Il restait une seconde cause — bug 18.

18. **La texture d'environnement n'est jamais reliée avant d'être écrite.**
    Seconde moitié du bug 17, et la vraie cause dans le pipeline fixe.

    `GL_UpdateEnvTexture` bascule sur l'unité de texture 1 et y fait un
    `glTexSubImage2D` de 4x4 — sans jamais y lier `envtexture`. Celle-ci n'est
    reliée qu'une fois, au précache du niveau ; tout ce qui lie une texture
    pendant que l'unité 1 est active prend sa place, et la sous-image tombe
    alors sur un objet sans stockage. `GL_INVALID_VALUE`, à chaque image.

    Visible uniquement avec `r_TextureCombiner 0`, la branche qui appelle
    `GL_UpdateEnvTexture` par batch. `GL_BindEnvTexture()` est maintenant
    appelé juste avant l'écriture.

    Isolation obtenue en faisant varier une cvar à la fois. **Piège rencontré :**
    `+cvar valeur` est *sauvegardé* dans `config.cfg`, donc chaque essai
    contamine le suivant. Mes trois premières mesures étaient fausses pour
    cette raison ; il faut passer la valeur explicitement à chaque lancement.

    Vérifié après correction : aucune erreur GL en core + shaders, core +
    G-buffer, ni 1.4 + pipeline fixe.

19. **Le jeu démarrait sur une vue zoomée, et se corrigeait tout seul au premier
    passage par le menu vidéo.** Bug amont, sans rapport avec le rendu, signalé
    par Dylan le 6 septembre 2026.

    `video.cc` demande la fenêtre avec **`SDL_WINDOW_ALLOW_HIGHDPI`**, ce qui
    déclare que le moteur travaille en pixels réels. Sur un écran à mise à
    l'échelle Windows, la taille de la fenêtre et celle de son dessinable GL sont
    alors **deux nombres différents** : demander 1920x1080 à 125 % rend un
    dessinable de **1536x864**.

    Or le chemin de création prenait la taille **demandée** :

    ```cpp
    video_width = copy.width;
    video_height = copy.height;
    ```

    `GL_Init` posait donc `dglViewport(0, 0, 1920, 1080)` sur un tampon de
    1536x864 : on voyait un **recadrage zoomé** du jeu.

    Pourquoi le menu vidéo réparait : le chemin de changement de mode, lui, prend
    ses dimensions de `SDL_GetClosestDisplayMode`, **déjà en pixels réels**. Le
    premier aller-retour dans ce menu remettait les deux d'accord.

    Corrigé par `SdlVideo::m_query_drawable()`, qui pose `video_width`,
    `video_height`, `video_ratio`, `ViewWidth` et `ViewHeight` depuis
    **`SDL_GL_GetDrawableSize`**, et qui est appelé sur les deux chemins. Il
    conserve les valeurs précédentes si SDL renvoie zéro — un zéro là diviserait
    le rapport d'aspect et dimensionnerait toutes les cibles du rendu.

    Validé en jeu par Dylan : l'écran-titre remplit l'écran dès le démarrage,
    sans passer par le menu vidéo.

    **Suite, le même jour : rendre le processus DPI-aware.** Lire le dessinable
    rendait le moteur *cohérent* ; il restait bridé. Un processus qui ne déclare
    rien est **DPI unaware**, et Windows lui sert un bureau virtualisé : à 125 %,
    le moteur croyait l'écran en 1536x864, dessinait 1,3 million de pixels, et le
    compositeur réétirait le tout sur les 1920x1080 réels de la dalle. 80 % des
    pixels, puis un flou pour combler.

    Corrigé d'une ligne dans le constructeur de `SdlVideo`, **avant**
    `SDL_INIT_VIDEO` — l'awareness est une propriété de processus fixée une fois,
    et SDL l'applique en démarrant le pilote vidéo :

    ```cpp
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    ```

    `"permonitorv2"` est le niveau recommandé par SDL. Surtout, il **ne** bascule
    pas SDL sur un système de coordonnées virtuel : une coordonnée SDL reste un
    pixel, ce dont le rendu a besoin puisqu'il dimensionne chaque framebuffer sur
    ces nombres. (`SDL_HINT_WINDOWS_DPI_SCALING` fait l'inverse — ne pas
    l'utiliser.) SDL retombe sur le meilleur niveau que le Windows courant offre,
    et les autres plateformes ignorent l'indice. SDL 2.32.10 ici.

    **Mesures, MAP01 tic 200, vsync éteint :**

    | | 1536x864 (avant) | 1920x1080 (après) |
    |---|---|---|
    | sans effet | 614 FPS | **613 FPS** |
    | avec `r_SAO 1` | 400 FPS | 378 FPS |

    **+56 % de pixels pour rien du tout** sans effet — nouvelle confirmation que
    le moteur est limité par le CPU et non par le remplissage — et pour environ
    5 % avec l'occlusion ambiante.

    **Correction à porter sur toutes les mesures antérieures de cette passation.**
    Le bug étant présent depuis toujours, chaque relevé de FPS et chaque capture
    étiquetés « 1920x1080 » — FXAA, SMAA, SAO, flou de mouvement, flou de menu,
    mise à l'échelle — ont en réalité été pris en **1536x864**. Les comparaisons
    restent valides, les deux côtés ayant été mesurés dans les mêmes conditions ;
    c'est la résolution annoncée qui est fausse. Les captures `-shottic` faisaient
    d'ailleurs 1920x1080 avec 1536x864 de contenu, ce que j'avais vu, mesuré, et
    attribué au réglage de taille d'écran du jeu.

    Validé en jeu par Dylan : image en 1920x1080 pleine, textures nettement plus
    fines — rivets des piliers, grille du sol, détail des lampes de plafond.

    **Réserve à connaître :** en fenêtré vrai (`v_Windowed 0`), une fenêtre de
    1920x1080 sur un écran 1920x1080 à 125 % occupe désormais toute la dalle,
    barre de titre comprise — c'est le comportement normal d'un processus
    DPI-aware, et le joueur choisit une taille plus petite. `v_Width` / `v_Height`
    sont maintenant en pixels réels.

    **Comment il a été trouvé, et ce que ça apprend :** mes captures `-shottic`
    faisaient 1920x1080 avec seulement **1536x864 de contenu et des bandes
    noires**. J'avais pris ces bandes pour le réglage de taille d'écran du jeu et
    je les avais écartées (§8 point 10). C'était le symptôme. 1920/1,25 = 1536 et
    1080/1,25 = 864 : le rapport donnait le diagnostic. **Une bande noire dans une
    capture est une mesure, pas un décor.**

20. **Deux écarts sur la luminosité, trouvés en comparant à DOOM64-RE.** Signalés
    indirectement par Dylan le 6 septembre 2026, qui trouvait le rendu des
    couleurs de secteur différent de celui de KEX.

    **a) Le `+ 100` était du mauvais côté de la comparaison.**

    L'original (`p_misc.c:661`) :

    ```c
    factor = brightness + 100;
    if (factor < infraredFactor) factor = infraredFactor;
    ```

    soit `max(brightness + 100, infraredFactor)`. Nous faisions
    `max(infraredFactor, brightness) + 100`.

    Les deux coïncident en jeu normal, où `infraredFactor` vaut 0. Ils divergent
    dès que les lunettes d'amplification de lumière sont ramassées : elles le
    posent à **300**, et le monde sortait donc à 400 au lieu de 300 — **un tiers
    trop clair**, pendant les vingt secondes du bonus et les cinq de sa
    décroissance.

    **b) Le fondu de luminosité n'atteignait jamais sa valeur finale.**

    L'original (`p_misc.c:758`) écrête **puis applique**, toujours :

    ```c
    fb->factor += 2;
    if (fb->factor >= (brightness + 100)) {
        fb->factor = (brightness + 100);
        P_RemoveThinker(&fb->thinker);
    }
    P_SetLightFactor(fb->factor);
    ```

    Nous appliquions **ou** retirions, jamais les deux : au tic qui termine le
    fondu, le thinker partait sans que la valeur atteinte soit posée. Le niveau
    se stabilisait un cran sous la pleine luminosité et y restait. Deux unités
    sur deux cents — mais définitivement.

    #### Ce qui n'est *pas* un bug, vérifié à la mesure

    Notre mise à l'échelle des lumières est **exacte**. `R_SetLightFactor` est
    l'algorithme de `P_SetLightFactor` au détail près, et la mesure le confirme :
    luminance moyenne de MAP01 au tic 200, capture entière,

    | `i_Brightness` | 0 | 50 | 100 |
    |---|---|---|---|
    | luminance | 7,6 | 11,4 | 15,3 |

    soit exactement les rapports 1,0 / 1,5 / 2,0 attendus de
    `f = (brightness + 100) / 100`.

    **Piège de mesure, à ne pas refaire.** Le premier relevé, pris au **tic 40**,
    donnait une luminance rigoureusement identique aux trois réglages, et j'en ai
    conclu que le réglage ne servait à rien. Faux : le fondu de début de niveau
    monte de 2 par tic, donc au tic 40 le facteur vaut 80 **quelle que soit la
    cible**, et les trois images sont légitimement identiques. Il faut mesurer la
    luminosité **après** le fondu — tic 200 convient. C'est le même piège que
    `-shottic` en général : le tic choisi fait partie de la mesure.

    #### Les trois moteurs n'ont pas le même défaut

    | | cvar | défaut | facteur |
    |---|---|---|---|
    | original N64 | `brightness` | **0** | 1,0 |
    | KEX | `g_brightness` | **50** | 1,5 |
    | nous | `i_Brightness` | **100** | 2,0 |

    Notre défaut est donc le maximum de l'échelle, là où l'original ships à zéro.
    Ce n'est pas un bug — le joueur règle — mais c'est un choix hérité de
    Doom64EX qui n'est celui de personne d'autre.

    #### Ce qui reste ouvert : le second réglage de KEX

    Leur menu Affichage a **deux** curseurs, et le `kexengine.cfg` donne les deux
    valeurs :

    | curseur | cvar | défaut |
    |---|---|---|
    | Luminosité générale (`$m_overall_brite`) | `g_brightness` | 50 |
    | Luminosité de l'environnement (`$m_enviro_brite`) | `r_brightness` | 1.0 |

    L'appariement vient du binaire, où `r_brightness` est immédiatement suivi de
    `$m_enviro_brite`. Sa description est « Applies overall display brightness »,
    et elle est rangée dans la zone `R_Main` du binaire — entre
    `progs/doomSceneNoFilter` et `PostProcess_DoomFog` — donc **appliquée dans le
    rendu du monde**, pas dans une passe de post-traitement séparée.

    **Nous n'avons pas d'équivalent.** Candidat plausible, mais pas le premier :
    voir le brouillard ci-dessous.

**Hypothèses de performance qui se sont révélées fausses** (mesurées, gain nul) :
cache des `glGet`, buffer en anneau, changements de VAO/programme, coût des

    #### La piste sérieuse : le brouillard est approximé deux fois

    Comparaison contrôlée obtenue le 6 septembre 2026 — MAP31, même point de vue,
    notre moteur à `i_Brightness` 100 contre KEX à ses deux curseurs par défaut.
    Notre image porte un voile teal uniforme que la leur n'a pas, **y compris sur
    le ciel**, qui ne passe pourtant pas par les lumières de secteur. L'écart est
    donc global, pas dans le chemin d'éclairage.

    La chaîne, pour MAP31 (`F_SKYH`, `fogfactor = 975`) :

    | étape | valeur |
    |---|---|
    | `SetupFog` : `max = 128000 / 25` | **5120** |
    | `SetupFog` : `min = ((975-500)*256) / 25` | **4864** |
    | `dglFogf(GL_FOG_DENSITY, 14.0f / (max+min))` | 0,001402 |
    | `draw.cc` : `uFogNear` | **0** |
    | `draw.cc` : `uFogFar = 3.0 / densité` | 2139 unités |

    **5120 et 4864 sont exactement les valeurs du registre de brouillard de la
    N64** — l'original calcule `Fnear = (128000/FnearA) << 16 | (FnearB/FnearA)`
    (`r_main.c:194`) et obtient le même couple. Jusque-là, Doom64EX est fidèle.

    Ensuite il les jette : il en fait une **densité `GL_EXP` avec une constante
    magique `14.0f`**, parce que le pipeline fixe n'a pas de brouillard linéaire
    en Z écran. Puis notre `draw.cc` reconvertit cette densité en un couple
    proche/lointain pour le `smoothstep` de `doomSceneMain`. **Deux approximations
    empilées**, dont la seconde pose `uFogNear = 0` : le brouillard commence à la
    caméra, et le mur juste devant le joueur est déjà teinté.

    KEX, lui, alimente `uFogNear` / `uFogFar` directement, sans passer par un
    brouillard de pipeline fixe qu'il n'a jamais eu.

    **Ce qu'il fallait faire :** dériver `uFogNear` / `uFogFar` directement de
    `fognear` et de la formule de l'original, au lieu de l'aller-retour par la
    densité `GL_EXP`.

    C'est une piste forte, pas une cause prouvée : je n'ai pas pu mesurer les
    captures de KEX, seulement les comparer à l'œil.

    **Fait le 6 septembre 2026 — l'aller-retour par la densité est supprimé.**

    `SetupFog` calcule maintenant les distances directement et les pousse au
    chemin programmable par `shader::set_world_fog()` ; `draw.cc` les utilise
    telles quelles au lieu de les relire dans la densité `GL_EXP`.

    ```c
    const float n64_near = 8.0f;      // guFrustum(-8, 8, -6, 6, 8, 3808)
    const float n64_far  = 3808.0f;
    float s     = fognear / 1000.0f;  // FOG_MIN, en millièmes de la plage écran
    float denom = 1.0f - s * ((n64_far - n64_near) / n64_far);
    set_world_fog(n64_near / denom, n64_far);
    ```

    Première version, calée sur les bornes de l'original : 296 → 3808 pour
    `fognear` 975. **Fausse** — voir la correction plus bas.

    Le `GL_EXP` reste posé pour le miroir d'état ; il n'alimente plus le shader
    du monde.

    **Ce qui reste approximé, et qui ne peut pas l'être moins :** `doomSceneMain`
    fait `smoothstep` en distance **linéaire monde**, là où la N64 rampe
    linéairement en **profondeur écran**, courbe fortement pondérée vers le fond.
    Les bornes sont désormais justes, la forme entre les deux ne l'est pas — et
    le shader est celui de Nightdive, il reste inchangé.

    **Vérifié :** compilation propre, zéro erreur GL, brouillard nettement reculé
    sur MAP31. **Non vérifié :** que le résultat soit *plus proche* de KEX. Cela
    change l'aspect de toutes les cartes et demande un jugement à l'œil, carte par
    carte.

    **Réserve honnête sur le diagnostic.** La comparaison contrôlée fournie par
    Dylan — notre moteur à `i_Brightness` 100 contre KEX à luminosité générale 0 %
    — donne deux images très proches. Si les courbes de brouillard divergeaient
    vraiment, aucun réglage de luminosité ne les réconcilierait : l'écart serait
    fonction de la distance. Ce que cette comparaison désigne, c'est un **décalage
    scalaire** — notre maximum vaut à peu près leur minimum — et non le brouillard.
    Le correctif ci-dessus vaut donc pour sa propre fidélité à l'original, pas
    comme réponse à l'écart de couleurs.

    **Correction du même jour — les bornes n'étaient pas ce qu'il fallait caler.**

    Dylan a comparé en jeu : le brouillard était devenu **trop lointain**, bien
    moins présent que celui de KEX. Il avait raison, et la mesure le confirme.

    Le piège est que l'original rampe linéairement en profondeur **écran**, et que
    la division perspective rend cette rampe violemment concentrée près du joueur
    une fois traduite en distances monde :

    | distance | brouillard N64 (fognear 975) | bornes 296→3808 |
    |---|---|---|
    | 500 | **44 %** | 1 % |
    | 700 | 63 % | 4 % |
    | 1000 | **76 %** | 10 % |
    | 1500 | 87 % | 27 % |

    Les bornes étaient justes et le résultat presque sans brouillard. **Un
    `smoothstep` symétrique ne peut pas suivre cette courbe** — son point à 50 %
    tombe au milieu de ses bornes, alors que celui de la N64 est à 549 unités pour
    des bornes de 296 et 3808.

    Il faut donc caler sur **la courbe**, pas sur les bornes. Ajustement par
    moindres carrés sur les 2500 premières unités :

    | réglage | erreur RMS |
    |---|---|
    | bornes 296 → 3808 | pire que tout |
    | Doom64EX d'origine, 0 → 2139 | 0,197 |
    | début à `w0`, fin ajustée | 0,135 |
    | **0 → 2,4 × w50** | **0,096** |

    où `w50` est la distance à laquelle l'original atteint la moitié du
    brouillard. C'est ce qui est en place :

    ```c
    float s50   = ((fognear / 1000.0f) + 1.0f) * 0.5f;   // profondeur écran a 50 %
    float denom = 1.0f - s50 * ((n64_far - n64_near) / n64_far);
    set_world_fog(0.0f, 2.4f * (n64_near / denom));
    ```

    | `fognear` | `w50` | fin |
    |---|---|---|
    | 975 (MAP31) | 549 | 1318 |
    | 985 (défaut) | 835 | 2004 |
    | 995 | 1741 | 4178 |

    **Le début revient à zéro**, et il faut savoir pourquoi : cela met environ 6 %
    de brouillard sur une surface collée au joueur, ce que l'original n'a pas.
    C'est le prix d'une courbe symétrique, et il achète tout le milieu de champ,
    là où le brouillard se voit réellement. Le gain net sur la courbe entière est
    de 0,197 à 0,096, soit deux fois plus fidèle que Doom64EX.

    **Validé en jeu par Dylan, comparaison contrôlée MAP31.** Sur la cour à la
    croix, la densité et le dégradé du brouillard correspondent maintenant à ceux
    de KEX : même fondu sur le mur du fond, la croix et les corps suspendus. Le
    brouillard est réglé.

    **Ce que la comparaison isole enfin.** Sur une vue dégagée, là où le
    brouillard sature, le fond de KEX est un cyan nettement plus clair que le
    nôtre. Or `uFogColor` est la valeur brute de SKYDEFS des deux côtés — et
    l'original ne la met pas à l'échelle non plus (`gDPSetFogColorD64(GFX1++,
    FogColor)`, `r_main.c:200`). Si la leur est plus claire, **c'est que KEX
    applique un relèvement global qui touche aussi la couleur du brouillard**.
    C'est leur second curseur, `r_brightness`, et cela ferme la question :
    l'écart résiduel n'est ni l'éclairage de secteur ni le brouillard.

    **Leçon.** Faire coïncider les bornes d'une courbe ne fait pas coïncider la
    courbe. J'ai livré une version « fidèle aux nombres de l'original » qui était
    visiblement pire que ce qu'elle remplaçait, et c'est Dylan qui l'a vu en jeu.
    Quand une transformation est non linéaire — ici la division perspective —
    c'est la forme qu'il faut mesurer, pas les extrémités.
uploads. Seules deux choses ont compté : la respécification de buffer (+44 FPS)
puis la **fusion des batchs** quand l'atlas est actif — le vrai correctif, qui a
ramené l'écart de 132 FPS à environ 17.

---

## 8. Feuille de route

### La feuille de route d'origine

Issue #74 du dépôt amont, « Roadmap / Wishlist », écrite par pinkwah (dotfloat)
en février 2018 : https://github.com/svkaiser/Doom64EX/issues/74

Restent non cochés, entre autres : tokenisation des points-virgules entre
guillemets dans la console (marqué critique), complétion automatique, nouvelle
API multi-manettes, DECORATE basique, suppression de `LumpHash`, menu de
sélection de PWAD, `GL_NEAREST` sur le HUD, isolation du code réseau.

Les deux points « Isolate GL 1.4 code » et « Experimental GL 3.3 » sont
**en cours** — voir ci-dessous.

### La licence des shaders KEX

Le `Doom64.kpf` du remaster Nightdive est une archive ZIP de 156 fichiers. Son
dossier `progs/` contient 30 points d'entrée sur 36 fichiers, et **les 36
portent une en-tête GPL v2 or later**, copyright Samuel Villarreal et Night
Dive Studios. **Ils sont donc légalement réutilisables ici.** Le reste du kpf
(81 PNG, polices, localisation) n'a aucune licence : **ne pas redistribuer.**

**Contrainte posée par Dylan, répétée plusieurs fois :** les shaders KEX sont
utilisés **sans aucune modification de leur code**. « Le code reste tel qu'il
est. » Tout ce qui doit être adapté passe par `inject_after` et
`progs/d64ex/overrides.inc`.

### Ce qui est fait

1. ✅ Test du préprocesseur du pilote (`##` supporté) — `tools/glslpp_test`
2. ✅ Préprocesseur du moteur (`#include`, conditionnelles, carte des lignes)
3. ✅ Les 30 points d'entrée KEX compilent
4. ✅ Atlas de textures construit et vérifié
5. ✅ Le monde est rendu par `doomSceneNoFilter.shader`, inchangé
6. ✅ Pile de matrices, fog, alpha test, `GL_TEXTURE_2D` : miroirs + autotests à
   divergence nulle
7. ✅ Mode immédiat et `glRect*` émulés
8. ✅ Écart de performance refermé (fusion des batchs)
9. ✅ Répétition générale sans pipeline fixe : le jeu tient
10. ✅ **Contexte OpenGL 3.3 core** — `-gl33`. Zéro erreur GL, ~620 FPS.
11. ✅ **Test de régression des 32 maps**, fait par Dylan le 4 septembre 2026 en
    `-gl33 -glcheck`. Trois bugs remontés (14, 15, 16 ci-dessus), tous corrigés.

### Le contexte 3.3 core — `-gl33`

`src/engine/system/sdl2/video.cc` demande le profil core quand `-gl33` est
passé. **Un drapeau, pas une bascule définitive** : le même binaire tourne dans
les deux profils, ce qui est la seule position confortable pour traquer une
divergence. Il deviendra le défaut une fois le test des 32 maps passé.

Le mécanisme central est `imp::gl33::core_profile()`, qui interroge
`GL_CONTEXT_PROFILE_MASK` une fois et met le résultat en cache. Deux macros de
`dgl.h` s'appuient dessus :

| macro | rôle |
|---|---|
| `D64_LEGACY(call)` | l'appel a lieu en compatibilité, pas en core |
| `D64_FF(call)` | la moitié pipeline fixe des macros à miroir — définie comme `D64_LEGACY(call),` |

**`D64_FF` était un garde de compilation ; il est maintenant runtime.** C'était
le vrai blocage : en core, toute la pile de matrices, le fog, l'alpha test et
les couleurs du mode immédiat partaient encore vers OpenGL et levaient erreur
sur erreur. `D64_NO_FIXED_FUNCTION` existe toujours et coupe la même chose à la
compilation — c'est ainsi que les miroirs ont été validés avant qu'un contexte
core existe.

Ce que le profil core refuse, et comment c'est traité :

| | traitement |
|---|---|
| `glMatrixMode`, `glRotatef`, `glFog*`, `glAlphaFunc`, `glColor*`, `glTexCoord*` | `D64_FF` — miroir seul |
| `glEnable/Disable(GL_TEXTURE_2D, GL_FOG, GL_ALPHA_TEST)` | filtrés dans `shader::state_enable` |
| `glTexEnv*`, `glShadeModel`, tableaux clients, `glLockArraysEXT` | `D64_LEGACY` |
| `glActiveTextureARB` | → `glActiveTexture` (core depuis 1.3) via `state_select_texture` |
| `glHint(GL_PERSPECTIVE_CORRECTION_HINT)`, `glHint(GL_FOG_HINT)` | `D64_LEGACY` |
| `glGetString(GL_EXTENSIONS)` | → `glGetStringi`, via `GL_ExtensionString()` |
| `GL_MAX_TEXTURE_UNITS_ARB` | → `GL_MAX_TEXTURE_IMAGE_UNITS` |
| `GL_CLAMP` | déjà `GL_CLAMP_TO_EDGE` depuis 1.2, rien à faire |
| autotests matrices et état | sautés — il n'y a plus rien à comparer |
| `r_Shaders 0` | ignoré en core : le chemin fixe dessine avec des tableaux clients |

**Ce qui a été vérifié en 3.3 core :** monde, HUD, arme du joueur, ciel, console,
et les textures masquées (grilles) — dont la transparence ne tient plus qu'au
`discard` du shader, `GL_ALPHA_TEST` n'existant plus.

**Performance :** core 3.3 ~620 FPS, compatibilité + shaders ~618, pipeline fixe
~600. Le chemin programmable est à parité, voire légèrement devant.

### Après le basculement

1. **Le test de régression sur les 32 maps** — c'est le moment que Dylan a choisi.
   Lancer `doom64ex2.exe -gl33` et comparer avec le lancement sans drapeau.
2. ✅ **Le core 3.3 est le contexte par défaut** depuis le 4 septembre 2026.
   `-gl14` reste comme repli, uniquement pour comparer une image suspecte au
   pipeline qu'elle remplace. À retirer quand plus rien n'en a besoin.
3. ✅ **`doomSceneFilter.shader`** — le second point d'entrée KEX compile et
   tourne, inchangé. Les deux programmes monde sont construits au démarrage et
   la cvar `r_N64Filter` choisit entre eux à chaque draw. Coût mesuré :
   ~619 FPS contre ~621, soit rien. Éteint par défaut : ça change l'aspect de
   tous les murs du jeu, et ce choix revient au joueur.
4. ✅ **FXAA** — et surtout, l'infrastructure de post-traitement qui manquait.

   `src/engine/opengl/shader/postprocess.cc` : un framebuffer à la taille de
   la fenêtre (couleur en texture, profondeur en renderbuffer), reconstruit
   quand la fenêtre change. `post_begin()` / `post_end()` encadrent le rendu
   du monde dans `P_Drawer` ; l'automap et le HUD sont dessinés **après**, à
   la fenêtre, pour qu'aucun anticrénelage ne touche au texte.

   Les deux points d'entrée KEX sont construits, inchangés, et `r_FXAA` choisit
   entre eux. Rien n'est alloué tant que la cvar vaut 0.

   | | |
   |---|---|
   | `r_FXAA 0` | ~600 FPS |
   | `r_FXAA 1` (`fxaa.shader`) | ~565 FPS |
   | `r_FXAA 2` (`fxaa_fast.shader`) | ~604 FPS |

   La variante rapide est gratuite à cette résolution ; la complète coûte
   environ 6 %. Zéro erreur GL dans les trois modes.

   Le quad plein écran est en coordonnées de clip avec les deux matrices à
   l'identité — le shader KEX les multiplie quand même, et il n'est pas
   question de le modifier. Le FBO est effacé en entier à chaque `post_begin` :
   le moteur n'efface que le rectangle de vue, et tout ce qui est autour
   reviendrait de l'image précédente.

   `postprocess.cc` est la fondation du G-buffer, de SAO et du flou de
   mouvement : il leur suffira d'attacher d'autres cibles au même FBO.
5. ✅ **G-buffer à 3 cibles** (`HAS_MRT 1`). Pas de tampon de normales — elles
   sont reconstruites par `dFdx`/`dFdy`, c'est le choix de KEX.

   `doomSceneMain` est construit une seconde fois avec `HAS_MRT` défini et
   `fragment_outputs(3)`, pour chacun de ses deux points d'entrée : quatre
   programmes monde au total, filtré ou non, G-buffer ou non. Le FBO reçoit
   deux cibles RGBA16F de plus et `glDrawBuffers` les active.

   **Les matrices de l'image précédente.** La cible de vélocité est la
   différence entre la position d'un fragment maintenant et à l'image d'avant,
   donc `uPrevProjection` et `uPrevModelView` doivent exister. `draw.cc`
   retient les matrices avec lesquelles le monde a été dessiné, et
   `world_frame_done()` — appelé depuis `P_Drawer` — les fait passer au rang
   de « précédentes ».

   **`uZFar` est devenu un vrai plan lointain.** Il valait 1, ce qui faisait
   mesurer la profondeur en unités monde : commode pour le brouillard, sans
   conséquence tant que rien d'autre ne la lisait. La cible de profondeur du
   G-buffer, elle, doit être dans [0, 1] — SAO et le flou de mouvement
   l'attendent ainsi. `WORLD_FAR = 8192` dans `draw.cc`, et les distances de
   brouillard sont divisées par la même constante en chemin vers le shader :
   c'est le rapport qui compte, donc le brouillard est inchangé.

   **Vérification.** `progs/d64ex/gbuffer.shader` ramène le buffer à l'écran et
   sait afficher chaque cible — c'est le seul moyen de contrôler la profondeur
   et la vélocité avant que quoi que ce soit en dépende. Constaté : la
   profondeur donne un dégradé linéaire correct ; la vélocité est grise
   uniforme quand le joueur est immobile et colorée sur l'arme, qui bouge ; le
   masque vaut 1 sur l'opaque et suit le contour de l'arme.

   Coût mesuré : nul (~607 FPS contre ~609). Zéro erreur GL.
6. ✅ **Flou de mouvement par objet** — la chaîne à quatre passes de KEX, plus
   son visualiseur. Cinq points d'entrée sur `motionBlurMain.shader`, tous
   inchangés.

   | passe | cible | rôle |
   |---|---|---|
   | `velocityPack` | pleine résolution | vélocité encodée sur 8 bits, longueur et profondeur empaquetées à côté |
   | `velocityTileGen` x2 | W/20 x H, puis W/20 x H/20 | réduction en tuiles, un axe à la fois, en gardant la plus longue |
   | `velocityTileNeighborhood` | W/20 x H/20 | chaque tuile prend la plus longue de ses neuf, pour que le mouvement déborde |
   | `motionBlur` | fenêtre | le flou lui-même, le long de la vélocité de la tuile |

   Une tuile fait 20 pixels : c'est la portée maximale du flou, et le facteur
   de réduction des deux passes de tuiles. `postprocess.cc` a maintenant un
   type `Target` (texture + FBO), sans quoi trois images intermédiaires de
   trois tailles différentes deviendraient douze variables globales.

   `r_MotionBlur` allume le G-buffer tout seul : personne ne devrait avoir à
   régler deux cvars pour un effet.

   **Vérification.** `r_MotionBlur 2` superpose `motionBlurVisualize` — grille
   des tuiles en violet, vecteur de vélocité de chaque tuile en blanc.
   Constaté sur une caméra immobile : des vecteurs **uniquement** sur l'arme
   du joueur, qui bobe, et sur le ciel, qui défile. Rien ailleurs. C'est une
   confirmation de la cible de vélocité par le code de KEX lui-même,
   indépendante de ma propre vue de débogage.

   Coût mesuré : ~598 FPS contre ~620, soit environ 3,5 %.

7. ✅ **SAO — occlusion ambiante.** Cinq shaders KEX coordonnés, inchangés :
   `copyDepthMip`, `downSampleDepth`, `sao`, `bilateralBlur_H`,
   `bilateralBlur_V`.

   | passe | cible |
   |---|---|
   | `copyDepthMip` | niveau 0 de la pyramide, R32F |
   | `downSampleDepth` x4 | niveaux 1 à 4, chacun depuis le précédent |
   | `sao` | l'occlusion, onze taps en spirale |
   | `bilateralBlur` x2 | séparable, et sensible à la profondeur pour ne pas déborder d'un bord |

   La pyramide est une texture R32F à cinq niveaux avec un framebuffer par
   niveau : `sao.shader` descend dans la pyramide dès que le rayon dépasse
   seize pixels. La composition est un mélange `GL_ZERO`/`GL_SRC_COLOR`.

   Coût mesuré : **~408 FPS contre ~620**, soit environ 34 %. C'est de loin
   l'effet le plus cher — onze taps en pleine résolution, plus la pyramide,
   plus deux passes de flou.

   **Ce qui a pris du temps, et pourquoi.** L'occlusion est sortie blanche
   pendant plusieurs itérations. Quatre causes distinctes, dans l'ordre où
   elles ont été levées :

   1. `sao.shader` fait `C.z = -C.z;` juste après `UVToEyePos` (ligne 206).
      `C.z` est donc négatif, et l'expression du rayon se réduit à
      **`uParams1.x / 2`** — un rayon constant en pixels. `uParams1.w` n'est que
      le plancher pour une surface collée à la caméra.
   2. `copyDepthMip` ne fait la copie directe que si **`uMipIndex == -1`**.
      Avec 0 il prend une branche qui multiplie `inFragCoord.xy` — déjà en
      pixels — par la taille de l'image puis écrête au coin, ce qui remplissait
      le niveau 0 d'une constante.
   3. `uShowMode` était posé **avant** `glUseProgram`. Un uniforme appartient au
      programme courant, donc l'écriture partait ailleurs. La valeur passe
      maintenant par `pass_()`, qui la pose après `use()`.
   4. **L'estimateur est dépendant de l'échelle.** Il calcule
      `(vn - biais) / (eps + vv)` avec `vv` en unités monde **au carré** : à
      l'échelle de Doom 64 ce dénominateur atteint les milliers, donc le rapport
      est minuscule. Avec `uParams1.y = 1` l'occlusion valait 0,91 partout,
      c'est-à-dire rien. L'intensité est multipliée par 12 et le biais exprimé
      en unités monde (2,0) et non en petits nombres.

   Un essai à `WORLD_FAR = 2048` n'a rien changé et a été annulé : la constante
   est restée à 8192, valeur vérifiée bonne pour le brouillard et pour la cible
   de profondeur du G-buffer.

   `sao.shader` écrit `vec4(occlusion, depth, 0, 0)` — **l'occlusion est dans le
   rouge seul**. Le mode 4 de `progs/d64ex/gbuffer.shader` l'étale sur les trois
   canaux, ce qui sert à la fois à l'afficher et à la composer.

   Aides de débogage laissées en place : `r_SAO 2` affiche l'occlusion,
   `+ r_GBufferShow 1` la pyramide de profondeur, `+ r_GBufferShow 2`
   l'occlusion **sans** le flou. C'est ce dernier qui a débloqué le diagnostic :
   il sépare une passe qui ne produit rien d'un flou qui efface tout.


8. ✅ **SMAA — anticrénelage morphologique.** Les trois shaders KEX de
   `progs/SMAA/`, inchangés.

   | passe | entrée | sortie |
   |---|---|---|
   | `SMAA_edgeDetection` | la scène | contours de luminance, R et G |
   | `SMAA_blendWeights` | les contours + `tArea` + `tSearch` | poids de mélange |
   | `SMAA_neighborhoodBlending` | la scène + les poids | l'image finale |

   Là où FXAA devine un contour depuis un voisinage, SMAA **le cherche dans une
   table** : `tArea` contient la couverture précalculée pour chaque forme et
   chaque distance qu'une ligne peut faire contre une grille de pixels. C'est ce
   qui lui permet de garder le détail que FXAA étale.

   `uRTSize` vaut `vec4(1/w, 1/h, w, h)` : les shaders décalent des coordonnées
   de texture avec la première paire et retrouvent une coordonnée pixel avec la
   seconde.

   **Les deux textures de référence ne sont pas dans le kpf, et n'y seront
   jamais.** Vérifié : les 156 entrées de l'archive ne contiennent que 81 PNG
   d'interface. Dans `DOOM64_x64.exe` on trouve `_areaTex` et `_searchTex` avec
   les classes `kexSMAAAreaTexture` / `kexSMAASearchTexture` — chez Nightdive ce
   sont des ressources compilées dans le binaire. Le préfixe `_` est leur
   convention pour une ressource interne au moteur, pas un fichier.

   Elles viennent donc de la **distribution officielle SMAA**
   (https://github.com/iryoku/smaa), extraites de `AreaTex.h` et `SearchTex.h`
   sans retouche, en données brutes :

   | fichier | dimensions | format | octets |
   |---|---|---|---|
   | `progs/d64ex/smaa_area.raw` | 160 x 560 | RG8, filtrage linéaire | 179 200 |
   | `progs/d64ex/smaa_search.raw` | 64 x 16 | R8, filtrage au plus proche | 1 024 |

   Les dimensions ne sont pas devinées : elles sont fixées par les constantes de
   `SMAA_blendWeights.shader` lui-même (`SMAA_AREATEX_PIXEL_SIZE`,
   `SMAA_SEARCHTEX_PACKED_SIZE`). L'échantillonnage en `.rg` désigne la variante
   DX10 du fichier officiel. Licence MIT, redistribution explicitement
   autorisée — voir `progs/d64ex/SMAA-LICENSE.txt`. Précédent : GZDoom embarque
   les mêmes textures dans son pk3.

   Le linéaire pour `tArea` est délibéré : le shader lit **entre** les texels,
   c'est à cela que servent les aires précalculées. Le plus proche pour
   `tSearch`, dont la lecture est déjà biaisée au centre des texels.

   **Les deux cibles intermédiaires sont effacées à chaque image.** Leurs
   shaders font `discard` là où ils ne trouvent rien, au lieu d'écrire zéro :
   ce qui restait de l'image précédente serait relu comme un vrai contour.

   Construction isolée dans son propre `try` : c'est le seul effet qui peut
   légitimement manquer, et son échec ne doit pas coûter FXAA, SAO et le flou.

   **Mesures** (MAP01, tic 140, 1920x1080, `-glcheck` : zéro erreur GL) :

   | | FPS | pixels modifiés |
   |---|---|---|
   | rien | 619 | — |
   | `r_SMAA 1` | **550** (-11 %) | **1,17 %** |
   | `r_FXAA 1` | 562 (-9 %) | 18,71 % |
   | `r_FXAA 2` | 580 (-6 %) | 23,23 % |

   Le chiffre qui compte est la seconde colonne. Pour un coût comparable à
   FXAA, SMAA touche **seize fois moins de pixels** : il corrige les arêtes
   géométriques et laisse le grain des textures tranquille. Vérifié au zoom —
   un escalier franc devient une pente continue, et la pierre et les lampes à
   côté sont identiques au pixel près.

   Aides de débogage : `r_SMAA 2` affiche les contours (rouge horizontal, vert
   vertical), `r_SMAA 3` les poids de mélange. Le second est le plus utile :
   des poids épars le long des seules arêtes géométriques prouvent que `tArea`
   et `tSearch` sont lues correctement — une texture d'aires mal orientée
   donnerait du bruit ou un aplat.

   **Ordre dans la chaîne.** `present_scene_()` fait passer le flou de mouvement
   avant l'anticrénelage : le flou appartient à la scène, conséquence de ce qui
   a bougé ; l'anticrénelage est une propriété de l'image finalement montrée.
   Quand les deux sont actifs, le flou écrit dans une cible au lieu de la
   fenêtre, parce qu'un anticrénelage échantillonne une image et pas une
   fenêtre. Vérifié : zéro erreur GL avec `r_MotionBlur 1` + `r_SMAA 1`.

   **Piste laissée ouverte :** KEX n'a qu'une cvar, `r_antialiasing`, avec
   quatre valeurs (rien, FXAA, FXAA rapide, SMAA) — lisible dans les chaînes de
   leur exécutable, à côté de `progs/fxaa_fast` et `progs/fxaa`. C'est plus
   propre que nos deux cvars indépendantes, qui admettent un état indéfini où
   les deux sont actifs. Unification à faire un jour ; `r_SMAA` gagne
   actuellement sur `r_FXAA`.

9. ✅ **Le flou d'arrière-plan des menus** — `progs/simpleBlur.shader`, inchangé.
   C'est `kexRenderPostProcessBlur`, le dernier des cinq systèmes de
   post-traitement de KEX qui nous manquait vraiment.

   **Ce n'est pas un effet sur la scène** : la scène n'a pas changé. Il recule
   l'image finie pour que le menu devant elle se lise. D'où sa place — après le
   flou de mouvement et après l'anticrénelage — et d'où le fait que le HUD,
   dessiné plus tard et directement à la fenêtre, reste net.

   **Ce que le shader attend.** `uParams` = (largeur, hauteur, rayon, seuil au
   carré). Il parcourt un carré de taps espacés de deux pixels jusqu'au rayon et
   pondère chacun par `sqrt(uParams.w)` moins sa distance au centre.

   Le quatrième terme est un choix, pas une lecture de KEX. Les taps les plus
   éloignés sont les quatre coins, à `rayon · √2`. Mettre le seuil exactement là
   leur donnerait un poids nul — et **à rayon 1, où les coins sont les seuls
   taps, tous les poids seraient nuls et l'image sortirait noire** : le shader
   écrête alors le diviseur à 1 et renvoie du zéro. Le seuil est donc porté un
   anneau plus loin, à `(rayon + 1) · √2`. La passe est en plus sautée dès que le
   rayon effectif tombe sous 1.

   **`r_MenuBlur` est le rayon en pixels, 8 par défaut, 0 éteint.** KEX n'a pas
   de cvar pour cet effet — il est simplement actif — mais le coût est
   quadratique en rayon, alors il vaut mieux pouvoir le dire.

   #### La leçon : d'où vient le déclenchement

   La première version lisait `menuactive` depuis `postprocess.cc`. **C'était
   faux, et pour une raison qui vaut d'être retenue :** cette variable est écrite
   depuis sept endroits de `m_menu.cc` et `d_main.cc` pour leurs propres raisons,
   et je n'arrivais pas à prédire sa valeur en lisant le code. Un état de jeu
   n'est pas une entrée de rendu.

   La dépendance est inversée. `postprocess.hh` expose :

   ```cpp
   void post_scene_blur(float amount);   // 0 net, 1 le rayon plein
   ```

   et c'est `M_Ticker` qui déclare son intention, à l'endroit même où
   `menufadefunc` fait avancer `menualphacolor` — donc exactement au même
   rythme :

   ```c
   imp::shader::post_scene_blur(menuactive ? menualphacolor / 255.0f : 0.0f);
   ```

   Deux bénéfices en plus de la correction : le flou **monte avec le fondu du
   menu** au lieu de surgir devant un panneau encore transparent, et n'importe
   quel autre écran pourra le demander sans que le rendu ait à connaître un cas
   de plus. La valeur est un **niveau, pas une impulsion** — chaque tic passe par
   `M_Ticker`, y compris ceux qui l'éteignent, donc rien n'a besoin de deviner
   quand elle est périmée.

   #### Ce que ça a demandé dans la chaîne

   `present_scene_` renvoie désormais **la cible où la scène a atterri**
   (`nullptr` si elle est déjà à la fenêtre). Chaque étape sauf la dernière écrit
   dans une cible, puisque la suivante échantillonne une image et qu'une fenêtre
   ne s'échantillonne pas. `run_smaa_` prend une destination, comme
   `run_motion_blur_` le faisait déjà, et une seconde cible `menu_blur_src_`
   s'ajoute à `scene_` — le flou de mouvement peut occuper celle-ci.

   **L'occlusion ambiante est composée avant le flou**, dans la cible et non à la
   fenêtre. Sinon elle resterait nette par-dessus une image adoucie.

   #### Vérification

   | | |
   |---|---|
   | compilation | propre |
   | erreurs GL (`-glcheck`) | 0 |
   | image en jeu, MAP01 tic 40, `r_MenuBlur` 8 contre 0 | **576 pixels sur 2 073 600 (0,028 %), écart max 5** |

   Ce 0,028 % est le plancher de bruit que ce tic donne de toute façon (voir
   §7bis) : **le flou ne tourne pas pendant le jeu**, ce qui était la question à
   trancher après l'erreur de conception ci-dessus. Le chemin sans flou est par
   ailleurs prouvé identique à l'ancien par construction : `menu_blur = false`
   met `tail` à `nullptr`, et chaque `pass_(…, tail, …)` redevient
   `pass_(…, nullptr, …)`.

   Dylan a validé le visuel en jeu : monde flou derrière le menu de pause, texte
   du menu et curseur nets, image nette à nouveau dès Échap.

   **Fausse alerte notée pour ne pas la refaire :** sur la capture, le HUD paraît
   flou lui aussi. Il ne l'est pas. `ST_Drawer` n'a qu'un appelant,
   `p_tick.cc:417`, **après** `post_end()` ligne 413 — il ne peut pas passer dans
   le flou. Ce qui le rend mou, c'est le `ST_FlashingScreen(0, 0, 0, 96)` que
   `M_Drawer` pose sur tout l'écran : un voile noir à 38 % qui écrase le
   contraste du HUD, pendant que les textes du menu sont dessinés par-dessus.

   **Reste à mesurer :** le coût. Il n'a pas été relevé, parce que le seul moment
   où l'effet tourne demande d'ouvrir le menu à la main et que `-shottic`
   n'atteint pas cet état. À rayon 8 en 1920x1080 c'est 81 taps par pixel, soit
   environ 168 millions de lectures par image — sans importance sur un jeu en
   pause, mais c'est bien pour cela que le rayon est réglable.

10. ✅ **Mise à l'échelle de la résolution — `r_ResolutionScale`**, première moitié
    de `r_resolutionscale` chez KEX : l'échelle fixe. Le régulateur dynamique
    reste à faire, voir plus bas.

    La scène est rendue dans une cible plus petite, **toute la chaîne de
    post-traitement tourne à cette taille**, et une dernière passe étire le
    résultat jusqu'à la fenêtre. L'interface est dessinée après, directement à la
    fenêtre : HUD, automap, console et menus restent à pleine résolution quoi
    qu'on règle. C'est tout l'intérêt de le faire là plutôt qu'en changeant la
    taille de la fenêtre.

    `r_ResolutionScale` est une fraction, 1 = natif, écrêtée à [0,25 ; 1].

    #### Ce qui a rendu la chose facile, et ce qu'il faut savoir

    **Un seul point d'entrée pour la taille de la scène.** `width_` et `height_`
    étaient déjà la source unique de toutes les cibles et de toutes les passes ;
    il a suffi de les calculer à partir de l'échelle. Deux nouvelles variables,
    `out_width_` / `out_height_`, portent la fenêtre, et `pass_` les utilise
    quand la destination est `nullptr`.

    **Un seul point d'entrée pour le viewport du monde.** `ViewWidth`,
    `ViewHeight`, `ViewWindowX` et `ViewWindowY` ne servent **qu'aux viewports** —
    vérifié, quatre sites, tous dans `gl_main.cc` (`GL_SetOrtho`,
    `GL_ResetViewport`, `GL_ClearView`, `GL_CalcViewSize`), et rien d'autre dans
    le moteur ne les lit. `post_begin` les emprunte le temps de la passe monde et
    `post_end` les rend. Aucun site d'appel n'a eu à savoir que la mise à
    l'échelle existe.

    **La chaîne se termine forcément dans une cible quand on étire.** Sinon FXAA,
    SMAA ou le flou de mouvement tourneraient à la taille de la fenêtre en
    échantillonnant une texture plus petite, avec des décalages de texel faux.
    Le mécanisme `tail` écrit pour le flou de menu (point 9) a servi tel quel :
    `present_scene_` prend maintenant `to_target` au lieu de `menu_blur`, et
    `post_end` enchaîne occlusion → flou de menu → étirement.

    `gbuffer.shader` fait l'étirement : il échantillonne par les coordonnées de
    texture du quad et non par la position du fragment, donc plaquer une petite
    texture sur un grand viewport est exactement ce qu'il fait, et le `GL_LINEAR`
    de la cible filtre.

    #### Deux bugs, et deux leçons

    **Le sprite de l'arme, à moitié et dans un coin.** `GL_Set2DQuad`
    (`gl_main.cc:491`) place les quads 2D ainsi :

    ```c
    left = ViewWindowX + x * ViewWidth / video_width;
    ```

    Ce rapport vaut 1 quand la vue remplit l'écran. En n'empruntant que
    `ViewWidth`/`ViewHeight`, il tombait au facteur d'échelle, et l'arme — qui va
    déjà dans une cible deux fois plus petite — était divisée **une seconde
    fois**. D'où l'emprunt de `video_width`/`video_height` avec elles : tant que la
    cible est liée, la cible **est** l'écran, et les deux paires la décrivent.

    #### Le bug qui a coûté un compteur ARMOR

    `GL_ClearView` règle **aussi le scissor** depuis ces quatre globales. Pendant
    l'emprunt il tombait donc à la taille de la scène réduite, et `post_end`
    rendait les globales sans remettre la boîte. Résultat : la barre d'état,
    dessinée ensuite et à la fenêtre, était **coupée à 960 px** — `ARMOR 0`
    disparaissait et `50` devenait `5`. `post_end` réapplique maintenant
    `dglScissor` avec les valeurs restaurées.

    Trouvé en regardant la capture, pas en relisant le code. **Et la bande noire de ces captures
    n'était pas le réglage de taille d'écran** comme je l'ai d'abord conclu :
    c'était le bug 19, la fenêtre à 125 % de mise à l'échelle Windows. Voir §7ter.

    #### Mesures — et le résultat est instructif

    MAP01, tic 200, 1920x1080, vsync éteint, interpolation allumée :

    | | échelle 1 | 0,75 | 0,5 |
    |---|---|---|---|
    | sans effet | 614 FPS | 605 | 607 |
    | **avec `r_SAO 1`** | **400 FPS** | — | **590 FPS** |

    **Sans effet, la mise à l'échelle ne rapporte rien.** À 600 FPS sur une
    géométrie Doom 64, le moteur n'est pas limité par le remplissage : c'est le
    CPU qui décide — liste d'affichage, parcours BSP, fusion des batchs. Réduire
    le nombre de pixels ne touche pas au goulot.

    **Avec l'occlusion ambiante, elle rapporte +47 %**, et 590 contre 614 en
    référence : à demi-résolution, l'effet le plus cher de la chaîne devient
    presque gratuit. C'est là que cette fonctionnalité sert — pas à faire tourner
    le jeu, mais à faire tenir les effets.

    Zéro erreur GL aux trois échelles.

    Validé en jeu par Dylan à 0,5 : arme centrée et à la bonne taille, HUD complet
    et net, monde adouci.

    #### Ce qui reste : le régulateur dynamique

    KEX pilote l'échelle depuis le temps de dessin, avec neuf cvars dont les
    descriptions donnent toute la boucle (voir §8, l'analyse du binaire) : deux
    seuils (`_targetdrawtime` à 1.125, `_gooddrawtime`), deux vitesses
    (`_lowerspeed`, `_increasespeed`), deux compteurs d'images
    (`_numframesbeforelowering`, `_numframesbeforeraising`) et un mode
    `_aggressive`. C'est un régulateur à hystérésis.

    **Deux choses à savoir avant de s'y mettre :**

    1. Il faut une mesure du temps de dessin. KEX utilise des requêtes de
       chronométrage GPU (`kexRHITimerQuery`, groupe `stat rhi`). Nous n'avons pas
       cette infrastructure ; le temps CPU par image suffirait probablement,
       puisque c'est lui qui plafonne ici.
    2. **Sur cette machine il n'y aurait rien à réguler** — 614 FPS sans effet.
       Le régulateur vaut pour une carte plus faible, ou pour le jour où la chaîne
       d'effets s'allonge. L'échelle fixe, elle, est déjà utile aujourd'hui.


11. ✅ **`r_Brightness` — la luminosité de l'environnement**, second curseur de
    KEX, et la réponse à l'écart de couleurs poursuivi toute la journée du
    6 septembre 2026.

    #### Ce que les mesures de Dylan ont établi

    Trois comparaisons contrôlées sur MAP31, même point de vue :

    | KEX | contre nous à `i_Brightness` 100 |
    |---|---|
    | générale 0 %, environnement **50 %** | à peu près identique |
    | générale 0 %, environnement **0 %** | **plus sombre** |
    | générale 50 %, environnement 50 % (défauts) | plus clair |

    L'environnement est donc un **multiplicateur**, neutre à 50 %. Et en le
    croisant avec l'échelle de l'original, le tableau devient clair :

    | | facteur min | défaut | max |
    |---|---|---|---|
    | original N64 | 1,0 | **1,0** | 2,0 |
    | nous | 1,0 | **2,0** | 2,0 |
    | KEX (déduit des mesures) | ~2,0 | ~2,5 | ~3,0 |

    **Notre plage est exactement celle de l'original ; celle de KEX est
    entièrement au-dessus.** Sur ce point le moteur est déjà *plus fidèle à
    l'original que KEX ne l'est*. Il ne manquait donc pas un correctif mais un
    réglage — celui qu'ils ont et que nous n'avions pas.

    #### La mise en œuvre

    Un multiplicateur sur l'image finie du monde, dans **notre**
    `progs/d64ex/gbuffer.shader` — le shader de présentation, qui nous appartient.
    Aucun shader de Nightdive n'est touché.

    ```glsl
    uniform float uEnvBrightness;
    ...
    result = sampleLevelZero(tBase, uv);
    result.rgb = saturate(result.rgb * uEnvBrightness);
    ```

    Seul le mode couleur l'applique : une vue de profondeur ou de vélocité est une
    **mesure**, et la mettre à l'échelle d'une préférence d'affichage la ferait
    mentir.

    `r_Brightness` vaut **1 par défaut, donc strictement neutre** : la fidélité à
    l'original ne coûte rien, et qui veut le rendu KEX monte le curseur.
    `env_brightness_on()` force le passage par le framebuffer dès qu'elle s'écarte
    de 1, exactement comme `r_MenuBlur`. Écrêtée à [0,05 ; 4] — un multiplicateur
    de zéro noircirait le monde sans retour possible hors console.

    C'est la place de KEX : leur `r_brightness` est rangé dans la zone `R_Main` du
    binaire, entre `progs/doomSceneNoFilter` et `PostProcess_DoomFog`, donc
    appliqué au monde et non dans une passe séparée. Le HUD, l'automap, la console
    et les menus sont dessinés après, à la fenêtre, et restent intacts.

    **Mesuré**, MAP31 tic 120, luminance moyenne :

    | `r_Brightness` | 0,5 | 1 | 1,5 | 2 |
    |---|---|---|---|---|
    | luminance | 18,4 | 33,2 | 44,1 | 55,6 |

    Zéro erreur GL aux quatre valeurs.

    #### Plage confirmée par l'expérience de Dylan

    Deux `kexengine.cfg` comparés, l'un les deux curseurs à 50 %, l'autre à 100 % :

    | curseur | 50 % | 100 % | plage |
    |---|---|---|---|
    | Luminosité générale | `g_brightness "50"` | `g_brightness "100"` | **0 → 100** |
    | Luminosité de l'environnement | `r_brightness "1.0"` | `r_brightness "2.0"` | **0 → 2,0** |

    **`r_brightness` est donc un multiplicateur de 0 à 2, neutre à 1,0** — la
    sémantique exacte de notre `r_Brightness`, retrouvée sans voir leur code. Notre
    écrêtage `[0,05 ; 4]` est un sur-ensemble du leur ; le plancher à 0,05 au lieu
    de 0 est délibéré (voir plus haut).

    **Calibration visuelle**, MAP31, contre KEX à ses deux défauts : Dylan trouve
    la correspondance autour de **1,5 à 2,0**, pas 1,2 comme je l'avais estimé.

    **Différence de caractère qui subsiste, et qu'il faut connaître :** notre
    multiplication porte sur l'image finie et passe par `saturate`, donc elle
    **écrête les hautes lumières**. KEX applique la sienne dans le rendu du monde.
    À valeur égale, nos zones claires s'aplatissent un peu plus tôt que les leurs.
    C'est inhérent à une multiplication en fin de chaîne, et le corriger
    demanderait de porter le facteur en amont — là où il ferait double emploi avec
    `i_Brightness`.


    #### Les deux réglages au menu Display, comme chez KEX

    Fait le 6 septembre 2026. Le moteur avait déjà les deux commandes ; il leur
    manquait l'interface.

    | entrée | cvar | plage | équivalent KEX |
    |---|---|---|---|
    | **Overall Brightness** | `i_Brightness` | 0 → 100 | `$m_overall_brite` / `g_brightness` |
    | **Environmental Brightness** | `r_Brightness` | 0 → 2, neutre à 1 | `$m_enviro_brite` / `r_brightness` |

    L'ancienne entrée « Brightness » est renommée **Overall Brightness**, le nom
    de KEX, parce qu'il y en a désormais deux et que « Brightness » ne dirait plus
    laquelle.

    La barre de l'environnementale compte **20 crans de 0,1** (`ENVBRIGHTSTEPS`),
    ce qui place le neutre exactement au cran du milieu — là où KEX met son défaut.
    Pas de rafraîchissement à déclencher : le rendu lit la cvar à chaque image, à
    la différence de `i_Brightness` qui doit rappeler `R_RefreshBrightness()`.

    **Deux vérifications de géométrie faites avant de livrer**, l'interface ne
    pouvant pas être capturée automatiquement :

    - *Largeur.* « Environmental Brightness », 24 caractères, est le plus long
      libellé du moteur — 5 de plus que l'ancien record. Vérifié aux proportions
      d'une capture du menu : « Crosshair Opacity » (17) occupe 29 % de la largeur,
      donc 24 caractères en prennent 41 %. Il reste de la marge.
    - *Hauteur.* Les deux lignes ajoutées portent le menu à 15 entrées, et cela
      a demandé deux essais. Remonter le menu de `y=65` à `y=55` ne suffisait
      pas : **la ligne d'aide au pied de ce menu est tracée à 90 % fixes de la
      hauteur d'écran**, et à l'échelle 0,715 les quinze entrées la dépassaient —
      « Default » et « Return » encadraient l'aide. Corrigé en passant l'échelle à
      **0,625** (une échelle plus petite est une boîte ortho plus haute) : le
      titre finit à 8,9 %, « Return » à 87,2 %, l'aide a le bas pour elle seule.
      **Validé en jeu par Dylan :** l'aide a le bas pour elle seule, et les deux
      barres se lisent correctement — curseur à fond à droite pour
      `i_Brightness` 100 comme pour `r_Brightness` 2,0, ce qui vérifie du même
      coup la conversion en crans.

    #### La plage du curseur étendue, la formule intacte

    Dernier écart mesuré par Dylan : à curseurs égaux (les deux à 50 %), notre
    rendu reste plus sombre que celui de KEX. Les facteurs d'éclairage ne
    coïncident pas :

    | curseur | notre facteur | facteur KEX (déduit) |
    |---|---|---|
    | 0 % | 1,0 | ~2,0 |
    | 50 % | 1,5 | ~2,5 |
    | 100 % | 2,0 | ~3,0 |

    C'est un **décalage additif constant de +1,0**, pas un multiplicateur — donc
    `r_Brightness`, qui multiplie l'image, ne peut pas le reproduire sur toute la
    plage : il ne coïnciderait qu'en un point.

    **Ce qu'il ne fallait surtout pas faire :** changer la formule en
    `brightness + 200`. Elle vient de l'original (`P_RefreshBrightness`,
    DOOM64-RE `p_misc.c:661`) et venait d'être vérifiée contre lui le même jour.
    L'aligner sur KEX aurait rendu la plage du N64 **inatteignable** — le réglage
    le plus sombre serait passé de 1,0 à 2,0.

    **Ce qui a été fait :** `MAXBRIGHTNESS` passe de 100 à **200**, et le défaut
    d'`i_Brightness` de 100 à **150**. La formule ne bouge pas ; c'est la course du
    curseur qui s'allonge.

    | curseur | valeur | facteur | |
    |---|---|---|---|
    | 0 % | 0 | 1,0 | minimum de l'original |
    | 25 % | 50 | 1,5 | |
    | 50 % | 100 | 2,0 | **maximum de l'original** |
    | 75 % | 150 | 2,5 | **défaut de KEX** — notre défaut |
    | 100 % | 200 | 3,0 | maximum de KEX |

    Le pas passe à 2 (`BRIGHTNESSINC`) pour garder les cent pressions d'avant sur
    une course deux fois plus longue.

    **Mesuré**, MAP01 tic 200, luminance moyenne :

    | facteur | 1,0 | 1,5 | 2,0 | 2,5 | 3,0 |
    |---|---|---|---|---|---|
    | luminance | 7,4 | 11,0 | 14,8 | 18,4 | 22,1 |

    Rapports contre la base : 1,49 / 2,00 / 2,49 / 2,99. Linéaire à 1 % près.

    **Conséquence à connaître :** les positions de curseur ne correspondent plus à
    celles de KEX — leur 50 % est notre 75 %. En échange, **les deux plages
    entières sont accessibles** : celle de la N64 dans la première moitié, celle du
    remaster dans la seconde. C'était le seul arrangement qui n'obligeait pas à
    sacrifier l'une des deux.

    **Comment comparer numériquement les deux moteurs** (méthode trouvée le
    6 septembre 2026, pas encore employée) : KEX écrit ses captures sur le disque
    avec la touche F5, dans le dossier `Saved Games\Nightdive Studios\DOOM 64\`
    sous la forme `shot000.png` — vérifié, le dossier voisin `Powerslave EX` en
    contient déjà. Les nôtres sortent en `sshot000.png` dans `build\Release\`.
    Deux captures du même point de vue, et les outils de mesure s'appliquent aux
    deux fichiers : c'est le seul moyen de **chiffrer** l'écart au lieu de le juger
    à l'œil, ce qui a coûté plusieurs allers-retours dans cette séance.

    MAP17 « Watch Your Step » est un bien meilleur banc d'essai que MAP31 : en
    extérieur, éclairée, et le joueur n'y est pas attaqué au point de départ.


    #### La cause réelle, mesurée : c'est un gamma, pas une luminosité

    6 septembre 2026. Deux captures F5 du même point de MAP17, l'une de KEX à ses
    défauts, l'autre de nous à `i_Brightness` 150 / `r_Brightness` 1, **lues
    directement sur le disque** — KEX écrit la sienne dans son dossier
    d'installation, `shot000.png`, pas dans Saved Games.

    Première mesure, moyennes par canal :

    | canal | KEX | nous | rapport |
    |---|---|---|---|
    | R | 52,4 | 33,2 | 1,578 |
    | V | 31,1 | 20,0 | 1,555 |
    | B | 15,6 | 10,2 | 1,529 |

    Rapports quasi identiques : cela ressemblait à une multiplication. **Faux.** Le
    même rapport pris par percentiles monte de **1,08 dans les ombres à 2,08 dans
    les hautes lumières**. Une multiplication donnerait un rapport constant, une
    addition un rapport qui *décroît* avec la valeur. C'est donc une courbe.

    Fonction de transfert relevée pixel à pixel puis ajustée :

    ```
    KEX ≈ 1,134 × nous^1,092        résidus ≤ 8 % de 10 à 100
    ```

    Or notre gamma est exactement de cette forme — `pow(c, 1 + 0.01 * i_Gamma)`
    (`i_png.cc`, `s_rgb_gamma`) — et un exposant de 1,092 correspond à
    **`i_Gamma` ≈ 9**. **Notre défaut est 0, c'est-à-dire l'identité :** nous
    n'appliquons aucune correction gamma. Le point « Gamma très bas par défaut »,
    ouvert dans cette passation depuis le premier jour, était la réponse.

    **Réserve sur la portée.** Notre gamma est appliqué à la **palette au
    chargement** (`I_TranslatePalette`), donc aux textures seules — ni la couleur
    du brouillard ni les couleurs de sommets issues des lumières de secteur n'y
    passent. La courbe de KEX, elle, se lit sur toute la plage tonale de l'image, y
    compris le ciel. Un `i_Gamma` bien réglé approche donc le résultat sans
    l'égaler ; la mise en œuvre propre serait une courbe sur l'image finie, dans
    `progs/d64ex/gbuffer.shader`, à côté de `uEnvBrightness`. **Non fait.**

    **Leçon de méthode.** Trois hypothèses successives — facteur d'éclairage,
    brouillard, multiplicateur — ont été formulées à l'œil sur des captures collées
    dans la conversation, et chacune n'expliquait qu'une part de l'écart. La
    première mesure faite sur les fichiers eux-mêmes a donné la réponse en deux
    commandes. **Lire les fichiers, pas les images.**

    **Corrigé le même jour — le gamma déplacé sur l'image du monde.**

    Dylan a monté `i_Gamma` et signalé que **le logo de l'écran-titre blanchissait**.
    Il avait raison, et cela ne contredit pas la mesure : c'était la *place* du
    gamma qui était fausse, pas sa forme. Baké dans la palette au chargement, il
    atteignait tout ce que le moteur charge — barre d'état, menus, écran-titre.

    La courbe est maintenant dans `progs/d64ex/gbuffer.shader`, à côté de
    `uEnvBrightness`, sur l'image finie du monde et sur rien d'autre :

    ```glsl
    result.rgb = saturate(pow(result.rgb * 255.0, vec3(uGamma)) / 255.0);
    ```

    **Le facteur 255 n'est pas décoratif.** La formule du moteur travaille sur des
    octets, où un exposant supérieur à 1 *éclaircit* ; le même exposant sur une
    couleur normalisée *assombrit*. `0,39^1,14 = 0,34` contre `100^1,14 = 188`,
    soit 0,74. Même nombre, effet opposé.

    Retiré au passage : `I_TranslatePalette` et son helper `s_rgb_gamma`
    (`i_png.cc`, conservés en commentaire comme trace), et le callback
    `GL_DumpTextures` d'`i_Gamma` — la valeur est lue à chaque image, plus besoin
    de recharger toutes les textures quand elle bouge.

    **Vérifié :** MAP17 au point de départ, luminance 15,6 → 23,7 entre gamma 0 et
    14 ; **écran-titre identique aux deux valeurs** (8,1 / R 12,0 / V 7,4 / B 3,6
    au chiffre près). Zéro erreur GL.

    **Reste à caler.** Le gamma sur image finie porte aussi sur le brouillard et
    les couleurs de sommets, que la version palette ne touchait pas : il est donc
    un peu plus fort à valeur égale (rapport 1,52 contre 1,46 à gamma 14 au même
    point de vue). La valeur qui correspondait à KEX avec l'ancienne
    implémentation — 14 — doit donc être reprise, autour de **12 à 14** ; une
    capture F5 de plus au même point que la référence KEX suffira à la calculer.
    Le défaut reste **0**, c'est-à-dire le rendu brut.

    **Ce que fait l'original, et il est catégorique.** Doom 64 sur N64 **coupe le
    gamma matériel** (`i_main.c:553`) :

    ```c
    osViSetSpecialFeatures(OS_VI_GAMMA_OFF | OS_VI_GAMMA_DITHER_OFF |
                           OS_VI_DIVOT_OFF | OS_VI_DITHER_FILTER_OFF);
    ```

    L'interface vidéo de la N64 offre une correction gamma câblée ; le jeu la
    désactive, avec le dithering du gamma, le divot et le filtre de dithering.
    **Aucune courbe n'est appliquée : le framebuffer brut est ce qui sort.**

    Donc, définitivement :

    | | gamma |
    |---|---|
    | original N64 | **aucun** (matériel explicitement coupé) |
    | nous à `i_Gamma 0` | **aucun** — identique à l'original |
    | KEX | une courbe d'exposant ~1,1, mesurée |

    KEX **s'écarte de l'original** sur ce point, comme il s'en écarte sur la plage
    de luminosité (§ ci-dessus, sa plage entière est au-dessus de celle de la N64).
    Le défaut d'`i_Gamma` reste donc **0** : c'est la fidélité. `13` donne le rendu
    du remaster, et c'est un choix, pas une correction.

    **Calage mesuré à `i_Gamma 13`**, MAP17, capture F5 des deux moteurs au même
    point, lues sur le disque :

    | | KEX | nous | écart |
    |---|---|---|---|
    | R | 52,4 | 52,3 | −0,2 % |
    | V | 31,1 | 29,6 | −4,8 % |
    | B | 15,6 | 13,8 | −11,5 % |
    | luminance | 34,5 | 33,3 | −3,5 % |

    Par percentiles, les rapports oscillent autour de 1,0 à ±25 %. Le résidu est du
    même ordre que l'écart de point de vue entre les deux captures — les cadrages
    ne sont pas identiques au pixel. **Aller plus loin demanderait un point de vue
    rigoureusement identique**, ce qui n'est atteignable qu'en plaçant les deux
    moteurs au point d'apparition d'une carte sans bouger.

    #### Pourquoi les deux moteurs ne peuvent pas coïncider exactement

    Dylan a relevé qu'**il n'existe aucune option de gamma dans KEX** — ni dans
    leur menu, ni dans `kexengine.cfg`. Vérifié dans le binaire : aucune cvar
    gamma parmi les 142, aucune clé de menu, et les seules occurrences de
    « gamma » ou « sRGB » sont **les messages d'erreur de libpng**. Leur courbe
    n'est donc pas un réglage : elle est cuite dans leur pipeline.

    Et une cause au niveau des **données** a été trouvée. Une texture extraite de
    leur `DOOM64.WAD` (`C1`, PNG 4 bits indexé) a une palette dont **toutes les
    valeurs sont multiples de 8** : 136, 120, 104, 88, 80… C'est du RGBA5551
    étendu par `x << 3`, sans réplication de bits.

    Notre chemin ROM fait autre chose. `resize_component<5,8>`
    (`image/detail/pixel.hh:56`) calcule `(x << 3) + (x >> 2)` — la réplication de
    bits, celle qui envoie 31 sur **255** et non sur 248 :

    | 5 bits | nous | KEX | écart |
    |---|---|---|---|
    | 8 | 66 | 64 | +3,1 % |
    | 16 | 132 | 128 | +3,1 % |
    | 31 | **255** | **248** | +2,8 % |

    Nous sommes donc environ **3 % plus clairs sur les textures**, et notre
    conversion est la plus correcte des deux — le blanc plein vaut vraiment 255.
    L'écart va dans le sens inverse du résidu mesuré, il ne l'explique donc pas ;
    mais il établit que **les deux moteurs ne rendent pas les mêmes données**. Nous
    lisons la ROM N64 ; eux lisent leur WAD réexporté, étendu autrement.

    **Le plafond est là.** Après `i_Gamma 13` le résidu est R exact, V −5 %,
    B −11,5 %, ce qui reste du même ordre que la somme de : cette conversion de
    palette, des assets réexportés, et un point de vue non identique au pixel entre
    les deux captures. Chercher plus loin reviendrait à poursuivre du bruit.

    **Position finale, et elle est confortable :**

    | | fidèle à la N64 | rendu KEX |
    |---|---|---|
    | `i_Brightness` | 0–100 | 150 |
    | `r_Brightness` | 1 | 1 |
    | `i_Gamma` | **0** | **13** |

    Les défauts restent ceux de la fidélité, puisque c'est démontré sur trois axes
    indépendants — plage de luminosité, gamma coupé par l'original, conversion de
    palette. Un seul réglage sépare le joueur du rendu du remaster.
    MAP31 s'est révélée inutilisable comme point de mesure — le joueur y est
    attaqué, donc l'état diffère d'une exécution à l'autre et les luminances ne
    sont pas monotones. MAP01 au tic 200 reste la référence stable.

      `y` est revenu à 65. Vu par Dylan sur une capture ; je ne peux pas naviguer
      dans les menus pour le vérifier moi-même, et le calcul de hauteur seul avait
      manqué la ligne d'aide, qui n'est pas une entrée du menu.

    À savoir : `M_DoDefaults` est un stub qui joue un son et ne réinitialise rien.
    L'entrée « Default » de tous les menus est décorative — bug amont, pas
    introduit ici.
    #### Piège de build rencontré — à retenir

    `cmake --build build --config Release --target doom64ex` **ne reconstruit pas
    le pk3.** La première mesure a donné trois valeurs rigoureusement identiques,
    et j'ai failli conclure que le câblage était cassé : le moteur tournait avec
    l'ancien `gbuffer.shader`, sans le nouvel uniforme. Après un changement de
    shader il faut :

    ```
    cmake --build build --config Release --target pk3
    copy build\doom64ex.pk3 build\Release\
    ```

    Le journal disait pourtant « Post-process target » — donc le chemin
    s'activait bien. C'est ce qui a orienté vers le pk3 plutôt que vers le code.

### Ce que `kexengine.cfg` apprend

Le fichier de configuration du remaster, analysé le 4 septembre 2026. Quatre
lignes nous concernent :

| ligne KEX | ce qu'elle dit |
|---|---|
| `r_maxocclusionunit "0.010000"` | **notre défaut est exact** : le nom avait été repris, la valeur tombe juste |
| `r_ambientOcclusion "0"` | un simple oui/non — **pas d'équivalent de `r_SAOIntensity`**, leur intensité est câblée en C++ à leur échelle monde. Cohérent avec la cause n°4 du bug SAO |
| `r_motionBlurShutterSpeed "250.000000"` | ils paramètrent en **vitesse d'obturation** (1/250 s), pas en échelle abstraite comme notre `r_MotionBlurScale`. Reparamétrage à faire un jour |
| `gl_useUniformBuffers "1"` | sur GL, KEX utilise de **vrais UBO**. Nous sommes sur `EMULATE_UNIFORM_BUFFERS`, leur propre échappatoire. Voie d'optimisation, pas un défaut |

Détail amusant : `r_rhimaxanisotropic "0"` — ils stockent eux aussi 0 comme
« désactivé », exactement le motif qui a causé notre bug 10. Ils doivent
l'écrêter côté C++.

### ✅ GLAD 3.3 core — fait le 5 septembre 2026

Le moteur ne porte plus une seule ligne de pipeline fixe.

**Étape A — couper les appels pendant que l'ancien en-tête les déclare encore.**
C'est le bon ordre : une erreur se manifeste alors comme un changement d'image,
pas comme un mur de trente-quatre erreurs de compilation.

```c
#define D64_LEGACY(call)     // plus rien
#define D64_FF(call)         // plus rien, et plus conditionnel
```

plus `DGL_UseShaders()` toujours vrai — l'alternative était des tableaux clients
et un `glDrawElements` sans VAO, que le core ne peut pas honorer — et les deux
`glHint` (`PERSPECTIVE_CORRECTION`, `FOG_HINT`) retirés. Le périmètre était
minuscule : **deux sites hors `dgl.h`**, le mode immédiat étant déjà émulé.

Mesure décisive : core et `-gl14` rendent dès lors **la même image à 0,03 %**,
le plancher de bruit. Le pipeline fixe ne contribuait donc plus rien, ce qui
était la condition pour retirer l'en-tête.

**Étape B — le swap.**

```
glad 0.1.36 — APIs: gl=3.3 — Profile: core
Extensions: GL_EXT_texture_filter_anisotropic
Omit khrplatform: False
```

Des six extensions d'origine, trois sont intégrées au core, deux disparaissent
avec le pipeline fixe ; seule l'anisotropie reste. Et **`--omit-khrplatform` est
abandonné** : c'est ce drapeau qui faisait retomber `GLsizeiptr` sur `long`,
donc 32 bits sous MSVC x64 — le bug 3. Le contournement de `gl33.hh` disparaît
avec lui.

| | avant | après |
|---|---|---|
| `glad.h` + `glad.c` | 3 779 lignes | 3 276 |
| `gl33.hh` + `gl33.cc` | 385 | **139** |
| appels `gl33::glXxx` | 201 | **0** |

`gl33` ne garde que `core_profile()`, `loaded()`, `load()` et deux typedefs :
GLAD déclare lui-même les cinquante et un points d'entrée qu'il résolvait à la
main via SDL.

**Trois choses à savoir pour la suite :**

1. **Le rescan d'extensions est supprimé** de `GL_Init`. Son propre commentaire
   annonçait « delete the whole block the day glad is regenerated for 3.3 ». Le
   loader 3.3 scanne avec `glGetStringi`, ce que le core sait répondre.
2. **Cinq des six drapeaux `GLAD_GL_*` n'existent plus** et sont devenus des
   `constexpr` dans `gl_main.h`, avec la valeur juste : `ARB_multitexture` et
   `ARB_texture_non_power_of_two` à vrai (intégrés au core), les trois autres à
   faux. Un `constexpr` replie la branche morte.
3. **Quarante-quatre constantes du pipeline fixe sont reprises** dans
   `gl_main.h` — `GL_ALPHA_TEST`, `GL_FOG_COLOR`, `GL_MODULATE`, `GL_PROJECTION`,
   `GL_EXP`, la famille du combineur. Ce ne sont plus des enums OpenGL mais **le
   vocabulaire du moteur** : `glstate.cc` y indexe son miroir de brouillard, la
   pile de matrices parle encore de `GL_MODELVIEW`. Les valeurs sont extraites
   de l'en-tête 1.4 remplacé, pas écrites de mémoire.

**`matrix_selftest()` est retiré.** Il comparait la pile du moteur à celle
d'OpenGL après chaque image — c'est lui qui avait révélé le `dglTranslated`
manquant et le `dglGetDoublev` de `r_clipper`. La pile GL ne recevant plus rien,
il ne mesurerait que sa propre absence. La fonction subsiste, renvoyant vrai.

**Vérification.** Compilation propre ; zéro erreur GL en core comme en `-gl14` ;
deux exécutions identiques au md5 ; et surtout **l'image d'avant et d'après le
swap se rejoignent à 0,03 %**.

*Piège de mesure rencontré :* un premier relevé donnait 58 % d'écart. `-glcheck`
ajoute un `glGetError` par image, ce qui décale le rythme et donc le chemin du
joueur dans la démo. C'était un écart d'**état de jeu**, pas de rendu. Comparer
deux captures suppose des drapeaux identiques, pas seulement le même tic.

### ✅ Le nettoyage — fait le 5 septembre 2026

Dylan a validé trois maps de la campagne sur GLAD 3.3 core sans un défaut. Le
pipeline fixe a donc été retiré pour de bon.

**`-gl14` n'existe plus.** L'enum `OpenGLVer` est supprimé de `ivideo.hh`,
`SdlVideo` ne prend plus de paramètre, et `m_init_gl()` demande 3.3 core sans
alternative. Il n'y avait plus rien à comparer : les deux profils rendaient la
même image à 0,03 %.

**Le titre de la fenêtre disait « OpenGL 1.4 »** — codé en dur depuis toujours,
et faux depuis la bascule. Il annonce maintenant « OpenGL 3.3 core ».

**`r_Shaders` est supprimée.** Elle choisissait entre le chemin programmable et
le chemin fixe ; il n'y a plus de chemin fixe. Une cvar qui ne commande plus rien
est pire que pas de cvar du tout, parce que la régler donne l'illusion d'agir.

**Cinq branches mortes retirées**, toutes gardées par `core_profile()` :

| | |
|---|---|
| `GL_ExtensionString` | la branche `glGetString(GL_EXTENSIONS)` de la 1.x |
| `GL_Init` | la purge d'erreur qui rattrapait le scan d'extensions de GLAD 1.4 |
| `GL_Init` | la requête `GL_MAX_TEXTURE_UNITS_ARB`, remplacée par `GL_MAX_TEXTURE_IMAGE_UNITS` |
| `state_enable` | le test de profil : les capacités du pipeline fixe s'arrêtent au miroir, toujours |
| `init_draw` | `atlas_build()` devient inconditionnel — il n'y a plus de second chemin |

**`state_selftest()` est retiré**, comme `matrix_selftest()` avant lui. C'est ce
test qui avait révélé que `GL_TEXTURE_2D` est **par unité de texture** et non
global, après que la moitié des comparaisons eurent divergé. Il ne compare plus
rien : OpenGL ne détient plus ni le brouillard, ni l'alpha test, ni
`GL_TEXTURE_2D`.

`core_profile()` subsiste, appelé une fois dans `GL_Init` pour la ligne de
journal. Plus rien ne branche dessus — c'est un constat sur le contexte obtenu,
un pilote restant libre de rendre autre chose que ce que SDL a demandé.

#### Les shaders de `progs/d64ex/` — ce qui part et ce qui reste

`world.shader` est **supprimé** : shader de transition, zéro référence, remplacé
depuis longtemps par `progs/doomSceneNoFilter.shader`.

**Les deux autres ne sont pas temporaires et ne peuvent pas partir.** Le kpf de
Nightdive ne couvre pas ces cas, et c'est précisément pourquoi ils existent :

- **`generic.shader`** est le programme de **tout ce qui n'est pas de la
  géométrie de monde** — HUD, console, menus, automap, ciel, melt
  (`draw.cc:599`, `use_world ? world : generic_`). Ces chemins lient une texture
  2D simple et s'appuient sur l'environnement de texture et le brouillard du
  moteur, trois choses que `doomSceneMain` ignore : il est écrit pour lire un
  `sampler2DArray` en adressage TMEM avec des métadonnées empaquetées dans les
  sommets. Le supprimer, c'est perdre l'interface entière.
- **`gbuffer.shader`** ramène le framebuffer hors écran jusqu'à la fenêtre
  (13 usages dans `postprocess.cc`). Sans lui, rien n'arrive à l'écran dès que le
  G-buffer est actif, et son mode 4 est ce qui compose l'occlusion ambiante.
- **`overrides.inc`** injecte le `discard` de l'alpha test dans chaque shader
  KEX. Sans lui, toutes les textures à trous redeviennent opaques.

Le dossier contient donc exactement ce qui sert : ces trois-là, plus les deux
textures SMAA et leur licence.

#### Reste

- ✅ **`D64_NO_FIXED_FUNCTION` et le dossier `build-noff` sont supprimés**
  (5 septembre 2026). Les macros étant inconditionnelles, le build noff n'était
  plus qu'une copie de 54 Mo du build normal. `D64_FF` subsiste comme macro vide,
  uniquement pour que chaque ligne à miroir dise encore quel appel du pipeline
  fixe elle remplace — c'est le seul témoignage qui en reste.
- `FindExtension` / `GL_ExtensionString` servent encore à l'anisotropie, alors
  que GLAD répond lui-même.

#### Le kpf n'a pas d'équivalent de `generic.shader` ni de `gbuffer.shader`

Question posée par Dylan le 5 septembre 2026, vérifiée en relisant les trente
shaders du `Doom64.kpf` un par un :

| famille | shaders |
|---|---|
| monde | `doomSceneMain` + 2 variantes |
| anticrénelage | `fxaa_main` + 2 variantes, SMAA ×3 |
| flou de mouvement | `motionBlurMain` + 5 variantes |
| occlusion ambiante | `sao`, `copyDepthMip`, `downSampleDepth`, `bilateralBlurMain` + 2 |
| 2D / interface | `default`, `doomSimpleNoAlpha`, `doomDisplayTexArray`, `bnetDisplayGrayscale`, `distanceField`, `colorPicker` |
| vidéo et divers | `yuvMovieDisplay`, `deinterleave`, `simpleBlur` |

**Pour `generic.shader`**, le plus proche est `default.shader`, qui fait
exactement une chose :

```glsl
outVarFragment(output, fragment) = sampleLevelZero(tBase, out_texcoord) * out_color;
```

soit `GL_MODULATE` seul. Notre `generic.shader` en fait quatre — modulate, add,
replace, et sans texture du tout — plus la lumière de secteur et le brouillard
fixe, linéaire comme exponentiel. Les cinq autres shaders 2D de KEX sont chacun
spécialisés ailleurs : niveaux de gris pour l'interface Bethesda.net, champ de
distance signé pour leurs polices TrueType, tableau de textures pour leur propre
chemin d'affichage. **Aucun ne traite l'environnement de texture ni le
brouillard.**

La raison est structurelle : notre `generic.shader` sert le code de dessin de
**Doom64EX**, qui parle en `dglTexEnv` et `dglFog`. Les shaders de KEX servent le
code de dessin de **KEX**, où Nightdive a réécrit l'interface et n'a rien à
émuler.

**Pour `gbuffer.shader`**, `default.shader` ferait la copie simple du framebuffer
avec une couleur de sommet blanche. Mais il ne fait pas le reste : afficher la
profondeur, la vélocité et le masque, et surtout **étaler le rouge sur les trois
canaux** pour composer l'occlusion. Vérifié : `sao.shader` écrit
`vec4(occlusion, depth, 0, 0)` et `bilateralBlurMain` transmet cette disposition
telle quelle — **aucun shader du kpf ne l'étale**. KEX fait cette étape dans son
C++, qui n'est pas dans l'archive.

*(`dxShaders.bin` (332 Ko) et `vkShaders.bin` (394 Ko) sont des binaires D3D et
Vulkan précompilés : inexploitables en OpenGL.)*

### Ce que `DOOM64_x64.exe` révèle — analyse complète du 5 septembre 2026

Analyse de surface : ce que le binaire **déclare de lui-même** — en-tête PE, imports,
classes RTTI, cvars et leurs descriptions, chaînes de log, chemins source laissés
par leurs `assert`. **Aucun désassemblage.** Leur code est fermé, et le transcrire
dans un moteur GPL v2+ serait une faute de licence pour Dylan. Tout ce qui suit est
de l'information sur *le produit*, pas de l'algorithme.

#### En-tête

```
PE32+ x64, sous-système GUI, 6 sections
compilé le 10 juin 2022 à 11:13:07 UTC
.text 3 534 223  .rdata 1 362 714  .data 2 953 064  .pdata 168 876  .rsrc 370 464
```

`CRASHLOG.TXT` donne **KEX Engine 3.8 / Doom 64 4.2.2**. Nom de code interne du
moteur : **`osiris`** (`osiris.dmp`, `kex3_osiris`).

#### Leur arborescence source — le fait le plus instructif

Les macros d'assertion ont laissé **133 chemins de fichiers** dans le binaire.
Racine : `F:\devstuff\doom64ex\kex3_osiris\`. Deux couches nettement séparées :

**`source/` — la couche jeu.** Et ses noms de fichiers sont ceux de la source N64
d'origine, pas ceux de Doom64EX :

```
d_screens.cpp  g_game.cpp  m_main.cpp  m_menus.cpp  m_signin.cpp
p_base.cpp  p_misc.cpp  p_mobj.cpp  p_pspr.cpp  p_savemgr.cpp
p_setup.cpp  p_tick.cpp
r_data.cpp  r_sky.cpp  r_sprite.cpp  r_vram.cpp
s_seq.cpp  w_merge.cpp  w_wad.cpp   + bnetUI/ (9 fichiers)
```

`p_base.cpp`, `p_misc.cpp`, `d_screens.cpp`, `r_data.cpp` sont **des noms de
DOOM64-RE**, pas de Doom64EX. Conclusion : Kaiser a rebâti la couche jeu du
remaster sur la structure de la source N64 — la même que celle que [GEC] a
retrouvée. C'est ce qui rend DOOM64-RE utile comme référence : c'est aussi leur
référence.

**`kexEngine/source/` — le framework**, qui n'a rien de Doom :

| dossier | contenu relevé |
|---|---|
| `framework/` | `engine`, `cvar`, `actions`, `archive`, `kpf`, `localization`, `memPool`, `object`, `parallelJobs`, `parser`, `signal`, `json.h`, `interpolants.h`, `dataResource.h` |
| `common/` | `array.h`, `bitMask.h`, `bufferStream`, `delegate.h`, `filePath`, `hash`, `kstring`, `lockFreeList.h`, `memoryUtils.h`, `refCount.h`, `stack.h`, `typeMap.h`, `unzip`, `async.h` |
| `rhi/` | `opengl/` (7), `d3d11/` (10), `vulkan/` (18 + `platform/`) |
| `rendering/` | `image`, `renderFont`, `renderResources`, `renderTargetCache`, `imageFormats/` |
| `platform/` | `sdl/`, `windows/`, `generic/` |
| autres | `audio/` + `audio/fmod/`, `movie/theora/`, `music/stream/`, `network/`, `social/steam/`, `tactile/generic/`, `math/` |

Plus `kexEngine-BDK/social/` (Bethesda.net) et `kexengine-bdk/thirdparty/bnet-sdk-cpp/`.

#### Les dépendances, lues dans la table d'imports

| DLL | imports | ce que ça dit |
|---|---|---|
| **SDL2.dll** | **96** | toute la couche plateforme est SDL2, **comme la nôtre** |
| KERNEL32 | 88 | |
| fmod.dll | 35 | `Sound`, `Channel`, `ChannelGroup`, **`DSP`**, `set3D*` |
| MSVCP140 | 36 | STL |
| WS2_32 | 25 | sockets |
| VCRUNTIME140 | 19 | |
| WINHTTP | 14 | Bethesda.net |
| steam_api64 | 11 | intégration Steam minimale |
| **OPENGL32.dll** | **1** | uniquement `wglGetCurrentContext` |
| d3d11.dll | 1 | `D3D11CreateDevice` |
| D3DCOMPILER_47 | 2 | `D3DCompile`, `D3DReflect` |

**Aucun import de `vulkan-1.dll`** : Vulkan est chargé dynamiquement
(`SDL_LoadObject` / `SDL_LoadFunction`). Idem pour OpenGL, dont un seul point
d'entrée est lié statiquement — tout le reste passe par `SDL_GL_GetProcAddress`,
**exactement notre approche avec GLAD**.

Ce que les imports SDL2 révèlent, et que le RTTI seul ne disait pas :

- **12 fonctions `SDL_Haptic*`**, dont `NewEffect` / `RunEffect` / `UpdateEffect` :
  du retour de force sur mesure, pas du simple rumble. C'est ce que jouent les
  16 fichiers `tactile/*.bnvib` du kpf (données de vibration HD Switch).
- `SDL_GetNumVideoDisplays`, `GetDisplayName`, `GetNumDisplayModes`,
  `GetClosestDisplayMode`, `SetWindowDisplayMode`, `GetDisplayUsableBounds` :
  tout le menu moniteur / résolution / mode fenêtre.
- `SDL_GameControllerAddMappingsFromRW` : ils lisent le `gamecontrollerdb.txt`
  livré dans le kpf.
- `SDL_GetCPUCount` : le nombre de fils de tâches.
- `SDL_LoadBMP_RW` + `GetWindowSurface` + `UpdateWindowSurface` : un chemin de
  blit logiciel — l'écran de démarrage avant que le contexte GPU existe
  (`kexPlatformSplashSDL`).
- **Aucun `SDL_Audio*`** : tout l'audio passe par FMOD.

**Bibliothèques tierces identifiées :** zlib 1.2.7, libpng 1.6.12,
**MCPP 2.7.2**, FreeType, libjpeg, dr_flac, moodycamel `concurrentqueue`,
SPIRV-Reflect, minizip, `glslangValidator.exe`.

#### MCPP — ils préprocessent leurs shaders avec un vrai préprocesseur C

Le binaire embarque **MCPP V.2.7.2 (2008/11)** en entier, avec
`mcpp_setopencallback` pour que les `#include` soient résolus depuis le kpf et
non depuis le disque. Ses diagnostics sont là mot pour mot (« Macro with mixing
of ## and # operators isn't portable », « Currently defined macros »…).

C'est la réponse à une question de conception restée ouverte chez nous : notre
`preprocessor.cc` (699 lignes) aplatit les `#include` et laisse le pilote GLSL
faire l'expansion des macros. Eux font tout en amont, avec un préprocesseur
conforme. Cela explique pourquoi `common.inc` peut supposer du C ISO complet.
**Notre approche reste valide** — `tools/glslpp_test` l'a vérifiée sur NVIDIA —
mais MCPP est sous licence BSD 2 clauses, donc compatible GPL si l'on veut un
jour cesser de dépendre du préprocesseur du pilote.

#### Le RHI : quatre backends, pas trois

**263 classes `kex*`** dans le RTTI (794 noms au total avec les templates).
Le RHI en compte 48, et les groupes de statistiques révèlent un quatrième
backend : `stat rhi.gl`, `stat rhi.d3d11`, `stat rhi.vulkan`,
`stat rhi.vulkan.mem` et **`stat rhi.gnm`** — GNM est l'API graphique de la
PlayStation 4, cohérent avec le `#if defined(__ORBIS__)` de `common.inc`.

Couverture réelle, inégale : `kexRHI3DTexture` existe en GL et VK mais **pas en
D3D11** ; `kexRHICubeMapTexture` **n'existe qu'en GL**.

**Ne pas copier cette abstraction.** Elle n'a de sens qu'en visant quatre cibles.

**Leur plancher OpenGL est la 3.2**, message d'erreur à l'appui : « OpenGL 3.2 is
required. Missing vertex attribute array and/or uniform buffer support », et
« GL shader version %i is below minimum requirements (150 or above is required) ».
Nous sommes en 3.3 core / GLSL 330 : **au-dessus de leur exigence.**

Sur PC leur défaut est **Vulkan** (`r_rhirenderfamily "vulkan"` dans
`default.cfg`), avec repli automatique : « Vulkan has already been removed from
adaptor list - falling back to OpenGL… ».

#### `r_vram.cpp` — leur atlas, et le nôtre

Leur système de textures s'appelle **virtual VRAM** : pages nommées
`DoomVirtualTexture%04d`, deux bancs (`vram1`, `vram2`), une commande `dumpvram`,
une cvar `r_showvirtualvram` (« Displays the virtual VRAM texture ») affichée par
**`progs/doomDisplayTexArray`**. Ils interrogent `GL_MAX_ARRAY_TEXTURE_LAYERS`.

C'est **exactement ce que fait notre `shader/atlas.cc`** : émulation de la TMEM
N64 dans un `sampler2DArray`. Notre `texatlasdump` est leur `dumpvram`. Cette
convergence n'est pas un hasard — `doomSceneMain.shader` impose l'adressage, et
nous utilisons leur shader inchangé.

Ce que ça règle aussi : `doomDisplayTexArray.shader` n'est pas « leur propre
chemin d'affichage » comme la passation le disait, c'est **le visualiseur de
VRAM**.

#### Les cvars, avec leur description exacte

**142 cvars** relevées. `r_` (30), `cl_` (21), `in_` (13), `bnet_` (12), `g_` (10),
`v_` (8), `snd_` (7), `gl_`/`con_` (4), `st_`/`jobs_`/`vk_` (3), `d3d11_`/`dem_`/
`win_` (2), `sys_`/`p_`/`am_` (1).

Les descriptions sont dans le binaire. Les plus instructives :

| cvar | description |
|---|---|
| `r_fov` | « Field of view », **défaut `74.0`** — identique au nôtre |
| `cl_engineHZ` | « Frames per second to run the game logic at » |
| `cl_engineFPS` | « Frames per second **the renderer** runs at » (défaut 60) |
| `cl_interpolation` | « Enables interpolation (for high FPS and 144hz users) » |
| `cl_maxlatetics` | « Maximum number of late gametics to run » |
| `r_clipspritefragments` | « sprites will be clipped as fragments for every subsector crossed » |
| `r_clipallspritetypes` | « If disabled, only corpses and shootable sprites will be clipped » |
| `r_greenblood` | « Use green blood, as in the Japanese release » |
| `r_colorizesubsectors` | « Applies a unique color to every subsector when drawn » |
| `r_maxsubsectordraw` | « Limits the number of subsectors drawn. Useful to see the order in which the view traverses the BSP » |
| `r_force_interpolatedanglereset` | « Completely reset view angle interpolation every frame » |
| `r_forcebufferclear` | « Clear framebuffer before drawing scene » |
| `r_filtering` | « If disabled, then nearest filtering will be used » |
| `r_brightness` | « Applies overall display brightness » |
| `st_hudlinearfilter` | « Applies linear filter to HUD graphics » — c'est l'entrée « GL_NEAREST sur le HUD » de la feuille de route amont |
| `p_fixlineskips` | « If 1, avoid skipping linedefs when player is moving fast. 0 is canonically allowed for speedrunners. » |
| `gl_forcePipelineSync` | « Syncs CPU by stalling until GPU has finished… may result in input lag » |
| `gl_useUniformBuffers` / `gl_useBindlessTextures` | « requires restart » |
| `vk_compileShaders` | « compile spirv shaders at RHI initialization (requires Vulkan SDK) » |
| `jobs_concurrentThreads` | « if none specified then all jobs are executed immediately (Requires restart) » |

`r_resolutionscale` et ses huit compagnons restent le plus gros morceau autonome
qui nous manque ; leurs descriptions décrivent la boucle entière (temps de dessin
cible, temps « bon », vitesses de montée et de descente, nombre d'images d'attente
dans chaque sens, mode agressif, échelle fixe).

#### Commandes de console

`bind`, `unbind`, `listbinds`, `listcmds`, `listvars`, `seta`, `exec`, `clear`,
`quit`, `restart`, `connect`, `screenshot`, `showmap`, `noclip`, `demigod`,
`freezeplayloop`, `giveweapons`, `giveartifact`, `infiniteammo`, `weapon <##>`,
`reloadshaders`, `listopenglextensions`, `dumpvram`, `buildDX11ShaderArchive`,
`bugit <title for the bug to report>`, et `stat <groupe>` avec les groupes
`engine`, `rmain`, `resolutionscale`, `taggedmem`, `rhi.gl`, `rhi.d3d11`,
`rhi.vulkan`, `rhi.vulkan.mem`, `rhi.gnm`.

Compteurs relevés : « Game Thread Time », « Game Thread Stall Time »,
« Game Draw Time », « Game Idle Time », « Interpolate Time », « Total
Interpolants », « Render Time », « Setup Sprites Time », « Prep Vertices Time »,
« Visible Leafs », « Active Scale », « Width/Height Percentage », « Time Fraction ».

Options de ligne de commande : `-basepath`, `-userpath`, `-file`, `-warp`,
`-skill`, `-fast`, `-nomonsters`, `-nosound`, `-nomusic`, `-skipmovies`,
`-fullscreen`, `-window`, `-width`, `-height`, `-xpos`, `-ypos`, `-setvar`,
`-safemode`, `-stdout`.

#### Le menu Développement, tel qu'il est câblé

Entrées et commandes associées : God Mode, Demigod Mode (`demigod`), Noclip Mode
(`noclip`), Freeze Playloop (`freezeplayloop`), Give Weapons (`giveweapons`),
Lock Monsters, Nightmare!, Change Map, Warp to finale, Test Demos, Show Console
Log, Show Virtual VRAM (`r_showvirtualvram`), Stat Engine / Stat R_Main / Stat
RHI, Enable Features (`g_unlockfeatures`), Enable Lost Levels
(`g_unlocklostlevels`), Enable Hectic Demo (`g_unlockhecticdemo`).

#### MAPINFO / SKYDEFS / ANIMDEFS — nous en sommes à 17 mots-clés sur 18

Les tables de mots-clés de leur `kexDefManager` sont en clair :

| fichier | mots-clés |
|---|---|
| `mapinfo.txt` / `MAPINFO` | `levelnum`, `cluster`, `classtype`, `music`, `exitdelay`, `nointermission`, `clearcheats`, `continuemusiconexit`, `forcegodmode`, `compat_collision`, `allowjump`, `allowfreelook`, `entertext`, `exittext`, `scrolltextend`, `pic_x`, `pic_y`, **`rewind`** |
| `skydefs.txt` / `SKYDEFS` | `pic`, `backpic`, `basecolor`, `highcolor`, `lowcolor`, `fogcolor`, `fogfactor`, `cloud`, `fire`, `thunder`, `void`, `fadeinbackground` |
| `animdefs.txt` / `ANIMDEFS` | `animpic`, `frames`, `restartdelay`, `cyclepalettes` |

Comparé à `P_InitMapInfo` (`p_setup.cc:1076` et suivantes) : **`rewind` est le
seul mot-clé que nous ne lisons pas.** Tous les autres sont couverts, dans
`mapdatatable`, `clusterdatatable` ou les cas particuliers.

**Correction :** la passation affirmait plus bas que `classtype` était « un champ
que notre parseur ignore sans doute ». C'est faux — il est la première ligne de
`mapdatatable` (`p_setup.cc:1077`).

#### Les marqueurs de section du `DOOM64.WAD` — vérifiés dans le fichier

Structure réelle des 1 668 lumps, décodée depuis le répertoire :

| lumps | section |
|---|---|
| 0 – 952 | `S_START` / `S_END` — 951 sprites |
| 953 – 971 | 19 lumps hors section (palettes) |
| 972 – 1476 | `T_START` / `T_END` — 503 textures |
| **1477 – 1497** | **21 graphismes, dans aucune section** : SYMBOLS, USLEGAL, TITLE, EVIL, FIRE, CLOUD, IDCRED1/2, WMSCRED1/2, FINAL, SFONT, STATUS, SPACE, MOUNTA/B/C, STDE, STES, STFR, STIT |
| 1498 – 1592 | `DS_START` / `DS_END` — 93 sons |
| 1593 – 1618 | **`DM_START` / `DM_END` — 24 musiques** (`MUSAMB01`…) |
| 1619 – 1658 | MAP01 … MAP40 |
| 1659 – 1662 | DEMO1LMP … DEMO4LMP |
| 1663 – 1665 | MAPINFO, ANIMDEFS, SKYDEFS |
| 1666 – 1667 | CHECKSUM (16 o), ENDOFWAD (0 o) |

Notre `wad/doom/doom_wad.cc` connaît `T_`, `G_`, `S_`, `DS_`. Donc **deux écarts** :
`DM_` (musique) n'est pas reconnu, et `G_START`/`G_END` **n'existent pas dans ce
WAD** — les 21 graphismes sont hors section. Les messages d'erreur du binaire
confirment leurs quatre sections obligatoires : « Textures / Sprites / Music /
Sounds section not found in IWAD ».

`w_merge.cpp` utilise en plus `SS_START`/`SS_END` et `TT_START`/`TT_END` : les
marqueurs de fusion de PWAD. Le support PWAD existe donc bien (`-file`,
`W_AddFile`), et c'est l'entrée « menu de sélection de PWAD » de la feuille de
route amont.

Autres détails du format : `sinetable.lmp` est livré dans le kpf et **requis**
(« R_InitSineTable: Required file sinetable.lmp not found ») — ils embarquent la
table de sinus de la N64 plutôt que de la calculer. Les sauvegardes sont
`saves/save_%02d.sav` et `saves/quicksave.sav`, magie **`KSAV`**. Les étiquettes
de zone sont `zone_static`, `zone_level`, `zone_levelspec`, `zone_cache`,
`zone_maplump`, `zone_audio`, `zone_mobj`.

#### Les 263 classes, par famille

| famille | classes |
|---|---|
| noyau objet | `kexObject`, `kexRefCountObject`, `kexNonCopyable`, `kexResource`, `kexRefResource`, `kexDelegateCallbackBase`, `kexTMapRefBase`, `kexException` |
| moteur et boucle | `kexEngine(Local)`, `kexGameLoop`, `kexDoomGameInstance` |
| fils d'exécution | `kexParallelJobManager(Local)`, `kexJobThread`, `kexWorkThread`, `kexGameThread`, `kexGenericSaveThread` |
| interpolation | `kexInterpolantBase`, `kexInterpolantManager(Local)` |
| RHI (48) | 1D/2D/3D/cube, contexte, état, shader, gestionnaire, cible, UBO, requêtes — en GL / D3D11 / VK |
| rendu au-dessus du RHI | `kexRenderScreen(Local)`, `kexRenderCommandBase`, `kexRenderResource`, `kexDoomRenderResource`, `kexDoomRenderTarget`, `kexTextureManager(Local)`, `kexTextureCacheKey`, `kexRenderTargetCacheKey`, `kexVertexBuffer`, `kexIndexBuffer` |
| post-traitement | `AA`, `AO`, `Blur`, `Deinterleave`, `MotionBlur` (+ un `Local` chacun) |
| textures spéciales | `kexDoomFireSkyTexture`, `kexSMAAAreaTexture`, `kexSMAASearchTexture`, `kexDefaultTexture` |
| thinkers (21) | `Object`, `Ceiling`, `FloorMove`, `Plat`, `VDoor`, `Glow`, `Strobe`, `LightFlash`, `FireFlicker`, `LightMorph`, `FadeBright`, `SequenceGlow`, `Combine`, `Delay`, `Quake`, `Laser`, `AimCamera`, `MoveCamera`, `MobjFade`, `MobjExp`, `SplitMove` |
| plateforme (29) | App, Console, DirEntry, Directory, File, Input, OnScreenKeyboard, Process, Splash, Thread, ThreadAffinity, Timer, UserData — en SDL / Windows / Generic |
| audio | `kexAudio(FMOD)`, `AudioBuffer(FMOD)`, `AudioSource(FMOD)`, `AudioData{Wave,Vorbis,Flac}`, `kexMusic`, `MusicStreaming`, `StreamedAudio{,Flac,Vorbis,Movie}` |
| images | `kexImagePolicy{BMP,DDS,JPG,PNG,TGA,**D64PNG**}` |
| données | `kexDefManager`, `kexPakFile(Local)`, `kexJSONValue{,Array,Bool,Number,Object,String}`, `kexBufferStream{,Direct,FileRead}` |
| réseau | `kexNetwork{Socket,Packet,UDP}` × `{BSD, Steam}`, `kexNetConnectInfo` |
| vibration | 7 classes + `kexTactileGenericBNVIBPlayer` |
| profilage | `kexStat{Base,BitstreamCounter,MemoryBlock,MemoryCounter,PerformanceTimer,SimpleCounter,SimpleFloatCounter}` |
| Bethesda.net / Steam | 27 classes `kexSocial*` |
| divers | `kexMoviePlayerTheora`, `kexTheoraMovieFile`, `kexDoomSaveManager(Local)`, `kexConsole`, `kexShader`, `kexFontResources` |

#### Ce qui reste actionnable pour nous, par ordre de valeur

1. ~~**`r_resolutionscale`**~~ — **terminé**. Échelle fixe le 6 septembre 2026,
   **régulateur dynamique et ses huit compagnons le 7 septembre 2026**, voir §8.
2. ~~`kexRenderPostProcessBlur`~~ — **fait le 5 septembre 2026**, voir §8 point 9.
3. ~~**Quatre correctifs de comportement**~~ — **traités le 7 septembre 2026**,
   voir §8 point 12. Deux implantés (`p_FixLineSkips`,
   `r_ForceInterpolatedAngleReset`), deux écartés avec preuve
   (`r_clipspritefragments`, `r_clipallspritetypes`).
4. ~~**`st_hudlinearfilter`**~~ — **fait le 7 septembre 2026**, voir §8. Le point
   « GL_NEAREST sur le HUD » de la feuille de route amont est coché.
5. ~~**Cinq vues de débogage**~~ — **faites le 7 septembre 2026**, voir §8. Trois
   écrites, deux qui existaient déjà sous le cran 2 de leur effet.
6. ~~**`gl_useUniformBuffers`**~~ — **fait le 7 septembre 2026**, voir §8.
7. **`snd_lowpassfilter` + `snd_hardwarereverb`** — chez eux ce sont des DSP FMOD ;
   à refaire autrement, mais l'idée est bonne.
8. **`rewind`** dans MAPINFO — le seul mot-clé qui nous manque.
9. **`DM_START`/`DM_END`** dans `doom_wad.cc`, et accepter les graphismes hors
   section — les deux écarts mesurés sur le WAD.

#### Ce qui est hors périmètre, et pourquoi

- **Le RHI à quatre backends** — utile seulement si l'on vise GL + D3D11 + Vulkan + GNM.
- **27 classes `kexSocial*`** — Bethesda.net, comptes, succès.
- **FMOD** — bibliothèque commerciale.
- **`kexParallelJobManager` + `kexGameThread` + `kexRenderCommandBase`** — un fil
  de jeu séparé du rendu avec un tampon de commandes. C'est une refonte, pas une
  amélioration.
- **21 `kexDoomThinker*`** — leurs thinkers sont des objets là où notre `playloop/`
  est du C. Ce n'est pas une dette : voir §7, personne ne convertit le playloop de
  Doom en objets, UZDoom compris.
- **`kexRenderPostProcessDeinterleave`** — rendu en damier, technique de console.

### Ré-analyse croisée du 5 septembre 2026 — moteur / kpf / DOOM64-RE

Passe de vérification demandée par Dylan après un compact. Rien n'a été modifié.
Ce qui suit n'est pas une relecture : ce sont des **comparaisons mesurées**, avec
leur méthode, pour ne pas avoir à les refaire.

#### Les shaders KEX sont intacts — vérifié par `cmp`

Les **36 fichiers** de `progs/` du `Doom64.kpf` (33 dans `progs/`, 3 dans
`progs/SMAA/`) comparés octet par octet à ceux de `distrib/doom64ex.pk3/progs/` :
**tous identiques.** La contrainte posée par Dylan tient formellement, et
`inject_after` + `overrides.inc` suffisent bien à tout adapter.

#### Ce que le dossier Steam apprend en plus du binaire

| source | fait |
|---|---|
| `CRASHLOG.TXT` | **KEX Engine 3.8, Doom 64 version 4.2.2** |
| `assert_0000.txt` | leur arborescence : `f:\devstuff\doom64ex\kex3_osiris\kexengine\source\common/array.h` — le projet du remaster s'appelle littéralement **`doom64ex`**, le moteur **`kex3_osiris`** |
| `Doom64.kpf/default.cfg` | 54 `bind` + **33 `seta`** : les défauts livrés |
| `Doom64.kpf/localization/loc_english.txt` | 689 lignes, dont **74 clés `m_*` avec leur texte anglais réel**, 40 noms de maps, 7 textes de cluster, 115 noms de touches |

Quatre lignes de `default.cfg` valent d'être retenues :

- `r_rhirenderfamily "vulkan"` — sur PC leur backend **par défaut est Vulkan**,
  pas OpenGL. Le `CRASHLOG.TXT` de Dylan est d'ailleurs un plantage dans
  `nvoglv64.dll`, donc dans leur chemin GL.
- `cl_engineFPS "60"` — le pas de simulation du moteur.
- `snd_hardwarereverb "1"`, `snd_lowpassfilter "1"`, `snd_lowpassgain "0.8125"`,
  `snd_lowpasscutofffreq "6833"` — un **filtre passe-bas** dont nous n'avons pas
  d'équivalent. Notre réverbération par secteur, elle, est déjà juste (ci-dessous).
- `jobs_enable "1"`, `jobs_concurrentThreads "2"` — leur système de tâches.

`loc_english.txt` remplace avantageusement la liste de clés `$m_*` relevée dans
le binaire : on y lit le libellé exact de chaque option. À noter, des entrées que
la liste de clés seule ne laissait pas deviner : `m_choose_campaign`,
`m_lost_levels`, `m_warp_lost`, `m_no_pitch_aim`, `m_adv_features`,
`m_health_boost`, `m_invulnerable`, `m_map_everything`, `m_gyroscope`,
`m_joycon_mode`.

#### Correction : `deinterleave.shader` n'est pas une lacune

La passation le listait comme un effet manquant, à côté de `simpleBlur`. Lecture
faite, c'est du **rendu en damier** : il échantillonne `tFrameN` sur les pixels
de parité courante et `tFrameNMin1` sur les autres, avec `uEventAtFrameN` qui
alterne. C'est une technique de moitié de charge pour consoles — le portage
Switch. Sur une GTX 1650 à 620 FPS, elle n'a **aucun intérêt** ; l'implémenter
serait dégrader l'image pour rien.

`simpleBlur.shader` en revanche était un vrai manque — comblé depuis, voir §8
point 9 — et sa nature est maintenant claire : flou plein écran à noyau pondéré (`uParams.z` taps,
poids `sqrt(uParams.w) − distance`). C'est le flou d'arrière-plan des menus.

**Le tableau du post-traitement devient donc :**

| classe KEX | chez nous |
|---|---|
| `kexRenderPostProcessAA` / `AO` / `MotionBlur` | ✅ |
| `kexRenderPostProcessBlur` | ✅ fait le 5 septembre 2026, voir §8 point 9 |
| `kexRenderPostProcessDeinterleave` | ⊘ hors périmètre — damier console |

#### DOOM64-RE comme référence : cinq vérifications passées

Le code de [GEC] est la décompilation de la ROM N64. Là où Doom64EX et lui
divergent, c'est **Doom64EX qui s'écarte de l'original**. Cinq points contrôlés :

1. **Les 11 ciels.** `R_SetupSky` (`r_phase2.c:59`) est la table codée en dur du
   jeu : type, nuage/feu/vide/espace, montagne, couleurs de sommets, `FogNear`,
   `FogColor`. Comparée ligne à ligne à notre `skydefs.txt` : **les 11 concordent
   exactement**, couleurs et brouillard compris (F_SKYA↔1 … F_SKYK↔11). Notre
   SKYDEFS est une transcription fidèle.

2. **Les tables d'acteurs.** `NUMSTATES` = 797 (`S_796`) et `NUMMOBJTYPES` = 131
   (`MT_GIB_HANGRIB`) **des deux côtés**, mêmes noms dans le même ordre.

3. **Le dégradé de couleur des murs.** RE fait un lerp :
   `c = up + (low − up) · t`, `t = (front_ceil − back_ceil) / (front_ceil − front_floor)`.
   Notre `R_SplitLineColor` fait `(c1·h1 + c2·h2) / height`. Comme
   `h1 + h2 = height`, c'est **la même expression**, écrite en somme pondérée.

4. **La réverbération.** `MS_REVERB` → 16, `MS_REVERBHEAVY` → 32, valeurs
   identiques des deux côtés.

5. **Le champ de vision.** `guFrustum(&R_ProjectionMatrix, -8, 8, -6, 6, 8, 3808, 1)`
   donne `tan(fovy/2) = 6/8` soit **73,74° vertical / 90° horizontal**. Notre
   `r_fov` vaut 74,0 par défaut. Le plan lointain de la N64 est **fini à 3808** ;
   nous projetons à l'infini (`dgl.cc:253`, `m[10] = -1`) et normalisons la
   profondeur sur `WORLD_FAR = 8192`.

**Une divergence réelle trouvée — le bit `0x800`.**

| | RE (original) | Doom64EX |
|---|---|---|
| nom | `ML_BLOCKPROJECTILES` | `ML_DONTPEGMID` |
| usage jeu | `p_base.c:648`, bloque le franchissement | `p_map.cc:239`, idem ✅ |
| usage rendu | *aucun* | `r_bsp.cc:517`, **décale la texture médiane** |

Doom64EX a donné deux sens au même bit : le blocage, qui est correct, plus un
calage de texture que l'original ne fait pas. Son propre commentaire l'admet
(« extremly hacky and it appears to be used only once in the entire game »).
À vérifier un jour sur la map concernée ; ce n'est pas un bug tant qu'on ne
sait pas ce que le décalage compense.

**Deux fausses pistes écartées avant d'être annoncées** (ne pas les rouvrir) :

- *Le ciel de feu refroidirait 16 fois trop lentement.* RE écrit
  `pixel − ((randIdx & 1) << 4)`, nous `pixel − (randIdx & 1)`. Mais
  `R_InitFire` fait `pixdata[i] >>= 4` et reconstruit une palette de 16 entrées :
  nous travaillons dans le domaine 0-15, eux dans 0-255. **Strictement
  équivalent.** Le tic aussi (`leveltime & 1` contre `gametic & 1`).
- *Les deux couches de sol liquide seraient triées à l'envers.* RE dessine
  `floorpic+1` opaque puis `floorpic` à alpha 160. `SortDrawList` renvoie
  `xb->texid − xa->texid`, donc **décroissant** : `floorpic+1` passe bien en
  premier. **Ordre conforme.**

**Cartographie des fichiers.** 28 des 77 `.c` de RE ont un homonyme chez nous, et
c'est tout le playloop : `p_ceilng`, `p_doors`, `p_enemy`, `p_floor`, `p_inter`,
`p_lights`, `p_macros`, `p_map`, `p_maputl`, `p_mobj`, `p_plats`, `p_pspr`,
`p_setup`, `p_sight`, `p_spec`, `p_switch`, `p_telept`, `p_tick`, `p_user`.
Les 49 autres se répartissent en trois familles :

| | |
|---|---|
| SDK N64 et audio WESS | `wess*` (17), `audio*`, `mem_heap`, `n64cmd`, `graph`, `cfb`, `seqload*`, `decodes`, `asci`, `vsprintf`, `funqueue`, `doomlib`, `c_convert` — sans objet chez nous |
| découpage différent du rendu | `r_phase1/2/3`, `r_data` → nos `r_bsp`, `r_scene`, `r_things`, `r_sky`, `r_drawlist`, `r_clipper`, `r_lights` |
| découpage différent du mouvement et de l'UI | `p_base`, `p_move`, `p_slide`, `p_shoot`, `p_change`, `p_misc` → nos `p_map` + `p_maputl` ; `m_main`, `st_main`, `am_main`, `in_main`, `f_main`, `d_screens` → nos `m_menu`, `st_stuff`, `am_map`, `wi_stuff`, `in_stuff` |

**Un détail d'architecture qui mérite d'être connu :** `R_BSP` (`r_phase1.c:35`)
n'utilise le tampon d'occlusion `solidcols[320]` que si `camviewpitch == 0` ;
sinon il descend le BSP **sans aucune élimination** et force `rendersky = true`.
Ce tampon de colonnes est un objet 1D, invalide dès que la vue est inclinée.
Nous n'avons pas ce problème : `r_clipper.cc` élimine par plages angulaires,
valides quel que soit le tangage.

#### Dérives de documentation relevées, à corriger un jour

- `shader/draw.hh`, en-tête : parle encore de `r_Shaders`, du « contexte de
  compatibilité » et de la relecture des matrices par `glGetFloatv`. Les trois
  ont disparu.
- `renderer/r_drawlist.cc` : toute la branche `!atlas` (liaison de texture,
  `GL_TEXTURE_WRAP_S/T`, `set_sector_light`, combineur) est morte —
  `atlas_build()` est inconditionnel depuis le nettoyage.

### Ce qu'UZDoom apprend — analyse du 6 septembre 2026

`D:\dev\UZDoom-trunk`, 745 000 lignes, 1 495 fichiers. Le point de comparaison le
plus utile qu'on ait eu, parce qu'il a résolu **exactement nos trois problèmes** —
le passage à OpenGL 3.3, l'assemblage des shaders, la chaîne de post-traitement —
et différemment de nous **et** de KEX.

#### La couche graphique

| | fichiers | rôle |
|---|---|---|
| `common/rendering/hwrenderer` | 39 | l'abstraction, indépendante de l'API |
| `common/rendering/gl` | 24 | backend OpenGL |
| `common/rendering/gles` | 27 | backend OpenGL ES |
| `common/rendering/vulkan` | 40 | backend Vulkan |
| `common/rendering/gl_load` | 4 | le chargeur de points d'entrée |

Trois backends comme KEX, mais une abstraction bien plus mince : 39 fichiers
contre 48 classes RTTI sur quatre backends. Même conclusion dans les deux cas —
**ne pas copier**, ça n'a de sens qu'en visant plusieurs API.

#### Leur OpenGL 3.3 : plancher bas, montée en gamme optionnelle

C'est **l'inverse de notre choix**, et il faut savoir pourquoi.

Leur chargeur est **glLoadGen**, pas GLAD (`gl_load/gl_extlist.txt`) :

```
lua LoadGen.lua -style=pointer_c -spec=gl -version=4.5 -profile=compatibility \
    -extfile=gl_extlist.txt load
```

**4.5 en profil compatibilité**, alors qu'ils exigent 3.3 à l'exécution :

```c
if (gl_version < 3.3f)
    I_FatalError("At least OpenGL 3.3 is required to run " GAMENAME ".\n");
```

Ils chargent donc tout jusqu'à 4.5, puis activent des drapeaux selon ce que la
carte offre réellement (`gl_interface.cpp`) :

| détecté | activé |
|---|---|
| ≥ 4.5 | `RFL_SHADER_STORAGE_BUFFER`, `RFL_BUFFER_STORAGE` |
| ≥ 4.3 ou extension | `RFL_INVALIDATE_BUFFER`, `RFL_DEBUG` |
| < 4.0 | GLSL écrêté à 3.31 |

Avec des contournements nommés : `gl_ClipDistance` cassé sur les pilotes ATI GL3
sous Windows (`RFL_NO_CLIP_PLANES`), SSBO limités au fragment sur Mesa.

Leur liste d'extensions ne compte que **dix entrées**.

**Notre choix — GLAD généré pour exactement 3.3 core — reste le bon** tant qu'on
ne vise qu'une cible : plus simple, et le `constexpr` replie les branches mortes.
Mais leur modèle est la voie à suivre le jour où l'on voudra de vrais UBO : leur
code lit `gl.flags` et bascule, il n'y a pas deux chemins de rendu.

**Une idée à prendre tout de suite :** le drapeau `-glversion` qui **simule** une
version plus basse pour tester le chemin bas de gamme sans changer de machine.
C'est ce que notre `-gl14` faisait, en mieux : ça ne duplique aucun code.

#### Leurs shaders : tout est assemblé en C++

30 shaders composés — un `main.fp` de 944 lignes, plus un `material_*.fp`
(modèle d'éclairage) et un `func_*.fp` (warp, paletted, pbr…) enfichés dedans —
plus 21 shaders de post-traitement dans `pp/`.

**Leurs fichiers ne contiennent ni `#version` ni `#include`.** Tout est concaténé
à l'exécution (`gl/gl_shader.cpp`, `FShader::Load`) :

```cpp
if ((gl.flags & RFL_SHADER_STORAGE_BUFFER) && screen->allowSSBO())
    vp_comb << "#version 430 core\n#define SUPPORTS_SHADOWMAPS\n";
else
    vp_comb << "#version 330 core\n";

vp_comb << defines << i_data;          // i_data : tous les uniformes, en dur dans le .cpp
vp_comb << "#line 1\n";
vp_comb << RemoveLayoutLocationDecl(GetStringFromLump(vp_lump), "out");
```

Même source, GLSL 330 ou 430 selon la carte.

Et **`stb_include` (209 lignes, domaine public) ne sert qu'aux shaders de mods
utilisateurs** — leur propre commentaire : `// skip includes processing for code
shaders`.

**Les trois réponses au même problème :**

| | mécanisme | taille |
|---|---|---|
| KEX | MCPP 2.7.2, préprocesseur C complet, `#include` résolus depuis le kpf par callback | une bibliothèque entière |
| UZDoom | concaténation de chaînes en C++ + `#line`, `stb_include` pour les mods seulement | 209 lignes |
| nous | `preprocessor.cc` : `#include`, conditionnelles, carte des lignes, `inject_after` | 699 lignes |

**Le nôtre est justifié**, et il faut savoir le dire : nous devons suivre les
conditionnelles de `common.inc` (`__ORBIS__` / `HLSL` / GLSL) **parce que nous
utilisons les shaders de KEX sans les modifier**. UZDoom n'a pas cette contrainte,
ce sont leurs shaders. Nos 500 lignes de plus sont le prix de cette fidélité.

**Deux idées à leur prendre :**

1. **`#line 1` avant chaque morceau concaténé** — laisser le pilote traduire les
   numéros de ligne. Notre `translate_log()` fait mieux (il donne le nom de
   fichier), mais un `#line` en plus rendrait justes les cas que la carte des
   lignes ne couvre pas.
2. **`RemoveLayoutLocationDecl` remplace par des espaces au lieu de supprimer**,
   pour que les numéros de ligne restent exacts. Trois lignes de code, et toute
   une classe de décalages évitée.

#### Leur post-traitement

`PPRenderState` (`hwrenderer/postprocessing/hw_postprocess.h`) décrit une passe —
shader, entrées, uniformes, viewport, mélange, sortie — et chaque backend
implémente `Draw()`. C'est notre `pass_(PostProgram&, Target*, params)`, en plus
abstrait. Leurs passes : bloom, blur, colormap, depthblur, exposure, fxaa, gamma,
lensdistortion, lineardepth, present (+3 variantes stéréo), shadowmap, ssao,
ssaocombine, tonemap.

**Une idée vraiment bonne : `PPTextureType::CurrentPipelineTexture`.** Une passe
déclare « je lis ce que la précédente a écrit », sans qu'aucune des deux ne nomme
de cible. Le chaînage devient automatique.

C'est exactement le branchement `tail` / `pending` écrit à la main pour insérer le
flou de menu après SMAA et le flou de mouvement (§8 point 9). Avec leur modèle,
cette restructuration n'aurait pas été nécessaire. **À reprendre si la chaîne
s'allonge encore** — pas avant, on n'a que cinq effets.

#### Licence — à vérifier avant toute reprise de code

```
SPDX-License-Identifier: GPL-3.0-or-later
Code written prior to 2026 is also licensed under: BSD-3-Clause
```

Notre moteur est en **GPL v2 or later**. Le code UZDoom postérieur à 2026 est en
**GPL-3.0-or-later** : l'incorporer forcerait tout le projet en GPL v3+, et ferait
perdre l'option « v2 ou ultérieure » héritée de Doom64EX.

Le code **antérieur à 2026 porte aussi BSD-3-Clause**, donc réutilisable ici en
conservant l'en-tête. `stb_include` est domaine public. Certains fichiers portent
Zlib (`gl_debug.cpp`).

**Règle : lire pour comprendre, partout. Copier, uniquement depuis un fichier dont
l'en-tête porte la double licence BSD ou Zlib — et vérifier cet en-tête à chaque
fois.**

### Autres pistes évoquées

- Accepter un `doom64.wad` **en plus** de la ROM (ne pas remplacer le chemin ROM,
  qui fonctionne). **Le `DOOM64.WAD` du remaster a été analysé le 5 septembre
  2026 ; ce qui bloque est identifié et tient en deux lignes.**

  Contenu : IWAD, 1 668 lumps, 14,4 Mo. **40 maps** (`MAP01`..`MAP40`),
  93 sons, 951 sprites, 503 textures, 24 musiques, 21 graphismes,
  19 palettes, 4 démos.

  | section | format réel |
  |---|---|
  | textures | **PNG** 4 bits indexé, 401 des 503 en 64x64, six tailles en tout |
  | sprites | **PNG** 4 et 8 bits indexé, 753 tailles distinctes |
  | graphismes | **PNG**, palette et RGBA mêlés |
  | sons | **RIFF/WAV** |
  | musiques | **MIDI** standard, jouées via `DOOMSND.DLS` |
  | `MAPINFO`, `ANIMDEFS`, `SKYDEFS` | **texte brut**, syntaxe que le moteur lit déjà |
  | chaque map | un **WAD imbriqué** (`IWAD` en tête) de 13 lumps non compressés : les classiques plus `LEAFS`, `LIGHTS`, `MACROS` |

  Nos `animdefs.txt` et `skydefs.txt` sont **identiques aux leurs** au caractère
  près, tabulations exceptées. Leur `mapinfo.txt` ne diffère que par les noms,
  qui passent chez eux par des clés de localisation (`$map_name_01`).

  **Ce qui bloque.** Les sidedefs ne stockent pas des index de texture mais des
  **`LumpHash` 16 bits** — vérifié : les 8 940 références de MAP02 se résolvent
  toutes. Or dans `src/engine/playloop/map.cc` (~ligne 95), la ligne qui
  remplit la table depuis la section textures est **commentée** :

  ```cpp
  for(auto& lump : wad::list_section(wad::Section::textures)) {
      //texturehashlist_.emplace(wad::LumpHash(lump->name()).get(), lump->section_index());
  }
  ```

  La table ne contient donc que l'identité `i → i` du chemin ROM. Pour un WAD
  chaque hachage retombe sur `return 0`, et l'index 0 est la texture nommée `?`,
  le placeholder — d'où « I SUCK AT MAKING MAPS » partout. **Ce n'est pas une
  histoire de marqueurs de section**, contrairement à ce que cette passation a
  longtemps dit. Il faut aussi ajouter `DM_START`/`DM_END` aux marqueurs connus
  de `doom_wad.cc`, qui ne les reconnaît pas.

  Curiosité relevée au passage : il y a **deux** textures nommées `?` (index 0 et
  354), et elles sont la seule collision de hachage des 503.

- **Les sept niveaux perdus** sont dans ce WAD et pas dans la ROM :
  `MAP34`..`MAP40` — Plant Ops, Evil Sacrifice, Cold Grounds, Wretched Vats,
  Thy Glory, Final Judgement, Panic. Ce sont les plus gros lumps du fichier
  (jusqu'à 395 Ko). Leur MAPINFO déclare un **cluster 7** avec `scrolltextend`,
  la même mécanique que le cluster 6 réparé au bug 16, et un champ `classtype = 3`, que notre parseur lit bien (`mapdatatable`, `p_setup.cc:1077`). Y accéder passe par le
  correctif ci-dessus.
- Support éventuel des fichiers du remaster KEX (`DOOM64.WAD` 14,75 Mo,
  `DOOMSND.DLS`). Le DLS demanderait FluidSynth 2.x — l'option
  `ENABLE_SYSTEM_FLUIDSYNTH` existe déjà dans le CMakeLists racine, et
  FluidSynth 2.x gère le DLS niveau 1 et 2. Modèle « l'utilisateur fournit ses
  fichiers », comme aujourd'hui avec la ROM.

### ✅ `r_AntiAliasing` et `gl_UseUniformBuffers` — 7 septembre 2026 (§8 points 4 et 6)

#### L'unification de l'antialiasing

KEX n'a qu'une cvar, `r_antialiasing`, « *Sets a antialiasing mode* », et les
chemins de shaders rangés à côté d'elle dans le binaire donnent l'ordre :
`fxaa_fast`, `fxaa`, puis les trois passes SMAA. Chez nous :

| `r_AntiAliasing` | |
|---|---|
| 0 | rien |
| 1 | FXAA |
| 2 | FXAA, variante rapide |
| 3 | SMAA |

`r_FXAA` et `r_SMAA` sont **supprimées**. Deux cvars pour une seule décision
était faux depuis le début : rien de sensé ne se produit quand les deux sont
allumées, si bien que l'ancien code devait désigner un gagnant dans le dos du
joueur (*« Where both are asked for, SMAA wins »*). **Un mode ne peut pas se
contredire lui-même.**

Les deux vues de débogage SMAA sont sorties dans **`r_SMAAShow`** (0 normal,
1 les contours, 2 les poids) plutôt que de devenir les modes 4 et 5. Une vue de
débogage n'est pas un réglage d'antialiasing, et `r_GBufferShow` avait déjà
établi où celles-là se rangent.

**Vérifié** : les quatre modes donnent quatre images distinctes, et
`r_SMAAShow 1` sur le mode 3 sort bien la carte de contours — rouge pour les
arêtes verticales, vert pour les horizontales — ce qui ne peut venir que du
chemin SMAA. Netteté RMS : 1,27 éteint, 1,30 FXAA, **1,21 FXAA rapide** (plus
flou, c'est ce qu'on lui demande), **1,52 SMAA** (plus net, c'est ce qui le
distingue de FXAA).

#### `gl_UseUniformBuffers`

`begin_cbuffer` dans `progs/common.inc` a deux expansions. Avec
`EMULATE_UNIFORM_BUFFERS` — ce que le moteur définissait depuis toujours —
chaque membre devient un `uniform` ordinaire. Sans, la même déclaration produit
un vrai bloc `std140`. La cvar choisit, et ne peut pas changer en cours de
route : elle décide comment **tous** les shaders du pk3 sont préprocessés, d'où
le « *requires restart* » de KEX, qui n'est pas une limitation qu'on a choisie.

**L'astuce qui évite de toucher tous les appelants.** Un membre de bloc n'a pas
de *location* — `glGetUniformLocation` répond −1 — donc le code existant aurait
cessé de fonctionner en silence. `Program::uniform` renvoie donc une **location
encodée** pour ces noms-là, et les helpers `set_mat4` / `set_float` / `set_vec2`
/ `set_vec3` / `set_vec4v` la décodent :

```
>= 0    une vraie location GL
-1      introuvable, rien n'est écrit
<= -2   un membre de bloc : indice de bloc et offset empaquetés
```

Un appelant écrit `set_mat4(p.u_projection, m)` et n'a pas à savoir dans lequel
des deux mondes il se trouve.

Les offsets sont **demandés au pilote** (`glGetActiveUniformsiv`,
`GL_UNIFORM_OFFSET`) et non calculés ici. std140 est une disposition spécifiée
et calculable à la main, mais un remplissage calculé à la main est exactement le
genre de chose qui est juste chez trois fabricants et faux chez le quatrième.
Le pilote a répondu **944 octets pour `RenderView`, 32 pour `PostProcess_DoomFog`,
112 pour `PostProcess_SAO`** — les trois valeurs que donne le calcul à la main,
ce qui valide les deux méthodes l'une par l'autre.

#### Trois bugs, et le troisième était le vrai

**1. Le cache suit l'état.** `apply_` ne réécrit une matrice que si elle a
changé *depuis que ce programme-là l'a écrite*. Avec un bloc, l'état est **un
tampon partagé** et non une copie par programme : A écrit sa matrice, B
l'écrase, A redessine, conclut que rien n'a bougé depuis sa propre écriture, et
rend avec celle de B. Symptôme : géométrie disparue, ciel intact. Corrigé par un
cache partagé quand le bloc est actif.

**2. `>= 0` au lieu de `!= -1`.** Deux gardes rejetaient les locations encodées,
puisqu'elles sont négatives. Les matrices de l'image précédente n'arrivaient
jamais.

**3. Il y a cinq cbuffers, pas un.** Celui-là a coûté le plus, et c'est le seul
intéressant. `RenderView` (registre 0), `PostProcess_SAO` (7),
`ColorPickerParams` (8), `PostProcess_Deinterleave` (10) et
**`PostProcess_DoomFog` (15)**, ce dernier déclaré à l'intérieur même de
`doomSceneMain.shader` et contenant `uFogColor`, `uFogNear`, `uFogFar`.

Or l'expansion GLSL de `begin_cbuffer` **jette le registre** : elle émet
`layout(std140) uniform Nom` et rien d'autre. Sur OpenGL, tous les blocs
retombent donc sur le point de liaison **zéro**. Un seul tampon attaché là et
**tous** le lisent : le bloc de brouillard de la scène trouvait les matrices de
`RenderView` à ses propres offsets et mélangeait le monde avec une couleur faite
de la matrice de projection. Le monde revenait **rouge uni**, silhouettes
correctes, ciel intact — parce que le ciel se dessine sans brouillard.

Chaque bloc reçoit maintenant sa liaison, tirée du registre que le shader
déclare, et son tampon. Les deux que rien ne pilote en reçoivent une aussi : il
faut simplement qu'ils ne restent pas sur zéro.

> *Un tampon partagé rend partagé tout ce qui le touche* — le cache par
> programme et le point de liaison par défaut sont le même bug vu deux fois.

**Vérifié** : `gl_UseUniformBuffers 1` contre `0`, MAP17, image 320 —
**écart moyen 0,032 par canal, maximum 3 sur 255**. Le résidu est de la
précision flottante et un tic d'écart entre deux exécutions ; il n'y a pas de
différence visible.

**Non mesuré, et il faut le dire : le gain.** Chaque écriture reste un
`glBufferSubData`, donc un appel pilote, exactement là où il y avait un
`glUniform`. Le gain d'un UBO vient du **groupage** — téléverser un bloc entier
une fois pour de nombreux programmes — et notre chaîne écrit `uViewWidth` et
`uViewHeight` à chaque passe avec des tailles différentes, ce qui l'interdit.
Sur un moteur dont on a déjà établi qu'il est limité par le CPU **mais pas sur
les appels d'uniformes** (§8 point 10 : l'échelle de résolution ne donne ~0 %
sans SAO), il ne faut rien en attendre. Défaut **0**, comme chez eux.

### ✅ Le régulateur dynamique de résolution — 7 septembre 2026 (§8 point 1)

La seconde moitié de `r_resolutionscale`. L'échelle fixe était faite le
6 septembre ; voici la boucle qui la pilote.

#### Les neuf cvars de KEX, avec leurs défauts

Les défauts sont **lisibles dans le binaire**, et on ne le savait pas avant
aujourd'hui : la signature `kexCvar(nom, flags, défaut, description)` trouvée
dans PowerslaveEX dit que la valeur par défaut est une chaîne, stockée dans le
pool à côté du nom. Il suffisait de savoir quoi chercher.

| cvar KEX | défaut | chez nous |
|---|---|---|
| `r_resolutionscale` | — (activation) | `r_ResolutionScaleDynamic` |
| `r_resolutionscale_fixedscale` | — | `r_ResolutionScale` (déjà là) |
| `r_resolutionscale_targetdrawtime` | **1.125** | idem |
| `r_resolutionscale_gooddrawtime` | **0.9** | idem |
| `r_resolutionscale_lowerspeed` | **0.1** | idem |
| `r_resolutionscale_increasespeed` | *(absent du pool)* | 0.02, à nous |
| `r_resolutionscale_aggressive` | — | idem |
| `r_resolutionscale_numframesbeforelowering` | **20** | idem |
| `r_resolutionscale_numframesbeforeraising` | **200** | idem |

**Une déviation, assumée.** Chez eux `r_resolutionscale` est l'interrupteur et
`_fixedscale` porte la fraction. Chez nous `r_ResolutionScale` **est** la
fraction depuis le 6 septembre, donc elle le reste et le régulateur reçoit son
propre interrupteur. Changer le sens d'une cvar déjà écrite dans des configs
serait pire que la divergence de nom.

#### Les deux temps sont des fractions, pas des millisecondes

Point décisif, et il est démontré par leur propre code : le groupe `stat
resolutionscale` du binaire déclare quatre colonnes — *Height Percentage*,
*Width Percentage*, **Time Fraction**, *Active Scale*. C'est donc une fraction
du budget d'image. Ça devait l'être : un moteur tenu de finir en 1,125 ms serait
un moteur qui ne réduit jamais rien.

Et **20 images pour descendre contre 200 pour remonter** n'est pas une faute de
frappe de leur part : tomber en un tiers de seconde, remonter en trois. C'est
l'hystérésis, et c'est ce qui empêche une échelle qui vient de réparer un
à-coup de le recréer aussitôt en oscillant. C'est le mode de défaillance de
tout régulateur de résolution dynamique, et la raison d'être des deux compteurs.

#### La mesure doit être celle du GPU

`GL_TIME_ELAPSED`, pas le temps mur autour des appels de dessin. Le temps mur
mesure la vitesse à laquelle le pilote a **accepté** les commandes ; sur une
image limitée par le GPU — la seule qui mérite d'être réduite — c'est une
fraction du coût réel. Un régulateur nourri de ce chiffre garderait la
résolution à 1 pendant que le nombre d'images s'effondre.

**Trois requêtes en rotation** (`timer_query_[3]`). Une s'écrit, les autres
mûrissent ; quand l'anneau revient, la plus ancienne est disponible depuis
longtemps et son résultat n'est **jamais attendu**. Un `glGetQueryObject`
bloquant ici provoquerait exactement le décrochage que le régulateur existe pour
éviter.

Le budget vient du **taux de rafraîchissement de l'écran**
(`SDL_GetCurrentDisplayMode`), avec repli à 60 Hz — SDL a le droit de ne pas
savoir, et c'est l'hypothèse que fait déjà tout le reste du moteur.

#### La bande morte

Entre `gooddrawtime` et `targetdrawtime`, une image ne compte ni pour ni contre
et **remet les deux compteurs à zéro**. Sans ça, une image qui n'est ni bonne ni
mauvaise resterait une preuve à charge pour l'un des deux, et l'échelle
dériverait sur du bruit seul.

`aggressive` divise les deux attentes par deux et fait croître la baisse avec le
dépassement : une image à deux fois le budget perd deux fois plus de résolution,
au lieu de grignoter 0,1 à la fois.

#### `statresolutionscale`

Leurs quatre colonnes, plus les deux compteurs. Sans ça la fonctionnalité n'est
pas vérifiable : l'échelle sur laquelle le régulateur se pose est un nombre que
rien d'autre ne rapporte, et la juger à l'œil sur le grain de l'image, ce n'est
pas la juger.

#### Deux bugs, dont un qui dormait là depuis le 6 septembre

**1. L'œuf et la poule.** À l'échelle 1, rien n'engageait la chaîne
post-process, donc aucune image n'était chronométrée, donc le régulateur ne
pouvait jamais apprendre qu'il fallait descendre. Corrigé en ajoutant
`dyn_scale_on()` à `post_active()` — ce qui a immédiatement produit le second.

**2. Le monde tout noir, et il n'était pas de moi.** La chaîne engagée sans rien
à faire tombait dans le chemin de présentation écrit à la main à la fin de
`post_end_body_`, lequel **ne posait pas `uEnvBrightness`**. Un uniform non posé
vaut zéro, et la première chose que `gbuffer.shader` en fait est une
multiplication. Monde noir, HUD par-dessus.

Ce chemin existait avant le régulateur : **`r_GBuffer 1` seul déclenchait déjà
le même écran noir**. Le bug attendait un second appelant pour se montrer.
Corrigé en extrayant `post_image_uniforms_()`, appelée par les deux chemins.

> *Ce qui peut être oublié à un endroit sur deux finit par l'être.* Le
> correctif n'est pas d'ajouter les deux lignes manquantes, c'est de supprimer
> l'endroit où on pouvait les oublier.

#### Mesuré

MAP17, 1920×1080, capture à l'image 400, netteté en gradient RMS sur le monde :

| | gradient RMS | rapport |
|---|---|---|
| régulateur éteint | 1,54 | référence |
| **régulateur aux défauts KEX** | **1,54** | **1,0042** |
| forcé à 0,25 (`targetdrawtime 0.001`) | 0,69 | 0,4509 |

Les deux moitiés de la démonstration :

- aux défauts, **zéro changement d'échelle enregistré** et une image nette au
  demi-pour-cent près de celle du régulateur éteint. Il est transparent quand il
  y a de la marge — pas d'oscillation, pas de dérive.
- forcé, il descend `1,0 → 0,25` par pas de 0,1, **bute sur la borne**, et la
  netteté tombe à 45 %. L'échelle atteint donc bien le rendu.

Fractions de temps réellement relevées sur cette machine : **0,735 à l'échelle 1**
et 0,10–0,25 aux échelles basses. Le GPU y consomme donc les trois quarts du
budget à pleine résolution — ce qui laisse peu de marge et explique pourquoi la
cible de 1,125 ne déclenche rien ici.

### ✅ Kex Engine 3.0 — le code source publié, analyse du 7 septembre 2026

Dylan a trouvé ce qui manquait depuis le début : **Kaiser a publié la première
itération de Kex 3 en source**, sous le nom de PowerslaveEX
(`D:\dev\PowerslaveEX-master`, `kex3_anubis/`). Doom 64 KEX tourne sur
**Kex Engine 3.8 « osiris »** (bandeau de la console, capture du 7 septembre).
C'est le même moteur, huit versions mineures plus tôt, et lisible.

#### La licence, et elle renverse tout

Le `LICENSE.txt` du dépôt contient le texte de la **GPL v3**, ce qui est un piège
au premier regard. Les **en-têtes des fichiers**, eux, disent autre chose :

> *either version 2 of the License, or (at your option) any later version*

Compté sur l'arbre entier : **178 des 181 fichiers `.cpp`/`.h` portent la GPL
v2-or-later, zéro porte la v3.** Les trois exceptions sont du tiers
(`framework/unzip.*` = minizip, `win32/opndir.*` = un pansement POSIX).

C'est **exactement notre licence**. Conséquence, et elle est majeure :

| source | ce qu'on peut en faire |
|---|---|
| `DOOM64_x64.exe` | lire ce que le binaire déclare de lui-même. Rien de plus. |
| UZDoom | copier **seulement** les fichiers en en-tête BSD-3/Zlib |
| **Kex 3 / PowerslaveEX** | **lire, comprendre et reprendre du code**, avec attribution, **sans aucune escalade de licence** |

Ce n'est plus de la rétro-ingénierie. C'est de la lecture de source sous la même
licence que la nôtre.

#### Ce que l'arbre contient

106 classes `kex*`, 181 fichiers, ~8 300 lignes rien que pour `renderer/`.
Répartition : `game/` (54 fichiers, logique Powerslave — sans intérêt pour nous),
`framework/` (35 : cvars, console, KPF, parseur, tas mémoire, RTTI),
`renderer/` (31), `system/` (17), `math/` (10), plus `script/` (AngelScript),
`movie/` (Theora), `opengl/`, `tools/`.

**Les données sont retirées** — `data/` ne contient que `anubis.ico`. Aucun
shader ne ship. La structure est là, pas le contenu.

#### La filiation, chiffrée

En croisant les 106 classes de PowerslaveEX avec les 344 symboles `kex*`
extraits de `DOOM64_x64.exe` : **16 noms identiques** — `kexAngle`, `kexArray`,
`kexConsole`, `kexDefManager`, `kexDict`, `kexFont`, `kexGameLoop`, `kexImage`,
`kexMoviePlayer`, `kexObject`, `kexPakFile`, `kexRenderScreen`, **`kexShaderObj`**,
`kexStr`, `kexTextureManager`, `kexVec3`.

Le chiffre est un **plancher, pas un total** : le binaire n'expose par RTTI que
les classes polymorphes, donc `kexCvar`, `kexFBO`, `kexRenderBackend` et les
autres classes concrètes n'y laissent aucune chaîne. La filiation est établie ;
son ampleur ne l'est pas.

#### Les quatre confirmations qui valent le déplacement

**1. `r_fov` vaut `74.0` par défaut, dans la source, en 2014.** La même valeur
que Doom 64 KEX et que chez nous. Notre dérivation depuis le `guFrustum` de la
N64 (73,74°) n'était donc pas une coïncidence heureuse : c'est la valeur que
Kaiser transporte de moteur en moteur.

**2. `uViewWidth` / `uViewHeight`.** Les noms d'uniformes exacts que notre chaîne
post-process interroge (`postprocess.cc`, `locate_`). Et le répertoire des
shaders s'appelle déjà **`progs/`** en 3.0. Ce que nous avons reconstruit depuis
`doom64.kpf` est la version 3.8 d'une convention vieille de dix ans.

**3. Les shaders de Kex 3.0 sont des fichiers séparés** :
`fxaaShader.Load("progs/fxaa.vert", "progs/fxaa.frag")`. Le dialecte à fichier
unique avec `SHADER_VERTEX`/`SHADER_PIXEL` et `common.inc` que nous avons
disséqué dans le kpf est donc une **évolution postérieure**. Kex 3.0 donne
l'ancêtre, pas la forme actuelle — utile à savoir avant d'y chercher une réponse
qui ne s'y trouve pas.

**4. Le format des cvars, et donc celui du binaire.**
`kexCvar(nom, flags, valeur_par_defaut, description)`, avec une surcharge à six
arguments pour les flottants bornés. Les 47 cvars de PowerslaveEX sont lisibles
avec leurs défauts : `gl_gamma` à `1`, `cl_maxfps` à `60`, `v_vsync` à `1`,
`gl_filter` à `0`. À noter : **Kex 3.0 a bien une cvar de gamma**. Elle a disparu
de Doom 64 3.8 — ce qui confirme, par un troisième chemin, que leur courbe y est
cuite et non réglable.

#### `r_fixspriteclipping` — et la correction qu'il impose

`kexCvar("r_fixspriteclipping", CVF_BOOL|CVF_CONFIG, "1", "Performs an extra
render pass to fix sprite clipping")`. **Défaut 1**, et c'est l'ancêtre direct
de `r_clipspritefragments` / `r_clipallspritetypes`, que j'avais écartés le matin
même sur une déduction fausse. Voir la correction en tête de §8.

Le mécanisme réel (`renderScene.cpp:1579`), par secteur contenant un sprite
visible :

1. stencil vidé, faces avant du secteur dessinées avec `REPLACE 128`, test de
   profondeur actif, **masque couleur coupé** ;
2. faces arrière des portails puis de la géométrie, en `INCR` — tout ce qui est
   derrière passe au-dessus de 128 ;
3. sprite redessiné avec `GLFUNC_EQUAL 128` et **`GLSTATE_DEPTHTEST` à false**.

Le sprite est donc dessiné **entier**, jamais tranché par la géométrie qu'il
traverse, et borné au volume visible de son propre secteur. Le commentaire juste
au-dessus décrit mot pour mot notre `DLT_SPRITE` : *« disable the depth mask
since we already have our sprites sorted out by distance »*.

Deux drapeaux d'acteur y échappent : `AF_STRETCHY` et `AF_NOSPRITECLIPFIX` — la
graine de ce qui devient `r_clipallspritetypes` en 3.8.

> À retenir, et c'est plus utile que le correctif : *une description de cvar dit
> ce qu'un correctif fait, jamais comment.* J'ai déduit un mécanisme de la
> formulation « clipped as fragments for every subsector crossed », puis bâti une
> démonstration en trois points dessus. La démonstration était rigoureuse ; sa
> prémisse était inventée. **Quand la source existe, on la lit avant de déduire.**

#### Périmètre de cette analyse

Lues intégralement : les 181 en-têtes de licence, les 106 déclarations de
classes, les 47 cvars, et `renderer/` en profondeur (`renderPostProcess`,
`renderScene`, `shaderProg`, `fbo`), plus `framework/cvar` et `framework/kpf`.
**Non lues** : les 54 fichiers de `game/` — logique de Powerslave, sans
équivalent chez nous — et `script/` (AngelScript, que nous n'embarquons pas).

`kexPakFile` confirme au passage que le KPF **est un ZIP** (minizip, `unzFile`),
ce que notre `zip_wad.cc` avait déjà établi par l'autre bout.

### ✅ Les cinq vues de débogage — 7 septembre 2026 (§8 point 5)

Trois étaient à écrire, deux existaient déjà sous un autre nom.

#### `r_ColorizeSubsectors` — écrit

Une couleur par sous-secteur, et la seule propriété qui compte est que deux
voisins n'en partagent jamais une. `R_SubsectorColor` ([r_bsp.cc]) marche sur la
roue des teintes par pas du **nombre d'or** : deux indices consécutifs tombent à
environ 137° l'un de l'autre, ce qui est le plus loin qu'un pas répété puisse
les mettre, et la suite ne retombe jamais dans une période comme le ferait un
pas fixe. Saturation 0,65, valeur 1 : assez vif pour lire les coutures, assez
pâle pour voir encore une porte au travers.

**Le problème de plomberie**, qui est l'essentiel du travail : la drawlist est
**triée par texture** avant d'être dessinée, donc au moment où `ProcessWalls` et
`ProcessFlats` s'exécutent, il n'existe plus aucun chemin de retour vers le
parcours BSP. `ProcessFlats` a bien son `subsector_t*`, mais `ProcessWalls` n'a
qu'un seg. D'où un champ `int subsector` dans `vtxlist_t`, tamponné une seule
fois dans `DL_AddVertexList` depuis un global posé par `R_Subsector` — un seul
point d'écriture, et les deux chemins (leaf et seg) le reçoivent gratuitement.

Côté murs, les **cinq** entrées de `bspColor[]` sont écrasées, pas une : n'en
changer qu'une laisserait la couture entre deux sous-secteurs lisible comme un
changement d'éclairage plutôt que comme la frontière qu'elle est.

#### `r_MaxSubsectorDraw` — écrit

Un compteur dans `R_Subsector`, remis à zéro dans `R_SetupFrame` (par image, pas
par niveau). Au-delà de la limite, retour immédiat. Monter la valeur d'un cran à
la fois fait dessiner l'arbre BSP tout seul, dans l'ordre où la vue le traverse.

#### `r_ShowVirtualVRAM` — écrit, et c'est leur propre visualiseur

`progs/doomDisplayTexArray.shader` **était déjà dans le pk3, inutilisé**. Il lit
le numéro de couche dans le canal rouge de la couleur de sommet (`fLayer =
colour.r * 255`) et échantillonne le `sampler2DArray`. Rien de leur code n'a été
touché : le VAO du post-process n'active que position et texcoord, donc
l'attribut 2 retombe sur sa **valeur générique courante** — un
`glVertexAttrib4f` suffit à choisir la couche, sans buffer de sommets dédié.

Deux pièges, tous deux trouvés à la mesure et non au raisonnement :

1. **`out_width_` valait 0.** Il n'est affecté qu'en même temps que les cibles de
   rendu, et cette vue doit marcher **tous effets éteints** — ce qui est
   précisément la façon dont on s'en sert. `dglViewport(0, 0, 0, 0)` ne dessine
   rien. Corrigé en lisant `video_width`/`video_height`.
2. **`vao_` n'existait pas.** `ensure_quad_()` n'est appelé que depuis
   `post_begin`, après que `ensure_targets_` a réussi. Même cause, même remède :
   la vue crée son quad elle-même.

Et la vue **remet tout en place** — viewport sauvegardé/restauré, cache
`GL_SetOrtho` invalidé par `GL_SetOrthoScale(1.0f)`. Vérifié : les chiffres du
status bar se dessinent bien par-dessus l'atlas.

#### `r_VisualizeAO` / `r_MotionBlurVisualize` — les avions déjà

Les deux existaient, repliés sur le cran 2 de l'effet auquel ils appartiennent :
`r_SAO 2` et `r_MotionBlur 2`. Pratique quand on travaille sur l'effet, inutile
pour qui lit la liste des cvars en cherchant l'interrupteur par son nom. Les deux
routes marchent maintenant, via `visualize_ao()` et `visualize_motion_blur()`.

**Et chacune allume ce qu'elle montre.** `r_VisualizeAO 1` avec `r_SAO 0` aurait
donné un écran vide : demander à voir une chose et recevoir du noir parce qu'une
seconde cvar était éteinte n'est pas de l'aide. Les deux cvars entrent donc dans
`sao_on()`, `motion_blur_on()`, `want_mrt` et `post_active()`.

**Vérifié** : les cinq activées seule depuis la ligne de commande, capture à
l'image 300 sur MAP17. `vues_debug.png` dans `build/Release`.

Les cinq sont en `cvar::Flag::noconfig`, pour la raison de `r_GBufferShow` : un
moteur qui redémarre en dessinant douze sous-secteurs en vert fluo ressemble
exactement à un bug de rendu, et envoie qui le voit en chasser un.

### ✅ `st_HudLinearFilter` — 7 septembre 2026 (§8 point 4)

Chez eux : « *Applies linear filter to HUD graphics* ». Chez nous il n'y avait
qu'un seul interrupteur pour tout, `r_Filter` (`gl_main.cc`), qui vaut 0 par
défaut et donne donc **`GL_LINEAR` à l'interface comme au monde**.

C'est le bon choix pour le monde et le mauvais pour l'interface. Un mur vu de
biais n'a pas de grille de pixels propre à préserver ; le lisser entre texels est
exactement ce qu'on veut. Le status bar, les polices et les graphismes de menu
sont l'inverse : dessinés au 320×240 de la N64 puis étirés d'un bloc vers la
fenêtre, une grille posée sur une autre. Les lisser ne fait que ramollir les
lettres et poser un halo autour des chiffres.

**Trois endroits du moteur avaient déjà tranché tout seuls** et codaient
`GL_NEAREST` en dur : la police de console (`gl_draw.cc`), les messages japonais
(`ST_DrawJMessage`, `st_stuff.cc`) et la capture d'écran (`GL_ScreenToTexture`,
`gl_texture.cc`). La décision existait déjà — elle n'était ni centralisée ni
réglable.

`GL_SetTextureFilterHud()` (`gl_main.cc`) la prend une fois et la rend
réglable. Elle est posée **à l'instant du dessin**, pas cuite au chargement :
d'où l'absence de rappel `GL_DumpTextures` que `r_Filter` traîne, et un effet
dès l'image suivante. L'anisotropie n'est volontairement pas touchée — elle ne
joue que sur une texture minifiée et de biais, ce qu'un quad d'interface plat
n'est jamais.

Appelée aux neuf sites d'interface : `Draw_Text` (SFONT), `Draw_BigText`
(SYMBOLS), `Draw_ConsoleText` (CONFONT), `Draw_Sprite2D`, les trois du menu
(BUTTONS, SYMBOLS, CURSOR), et `st_stuff.cc` (STATUS, CRSHAIRS, JMessage).

**Le piège de `Draw_Sprite2D`.** Il passe par `GL_BindSpriteTexture`, et une
texture de sprite est **partagée avec le monde** — le même objet GL que le
renderer liera pour un monstre. Le filtre est un état de l'objet, pas du dessin :
le poser sans le remettre contaminerait le monde jusqu'au prochain
rechargement. D'où le `GL_SetTextureFilter()` qui referme la fonction, tant que
la texture est encore liée.

`Draw_GfxImage` est **laissé au filtre du monde** de propos délibéré : il sert
l'écran-titre, les crédits, les images d'intermission et le fond de ciel
(`r_sky.cc:489`). Ce sont des images, pas de la chrome d'interface.

**Mesuré** — MAP17, 1920×1080, deux captures au même tic :

| | gradient RMS sur le texte du HUD |
|---|---|
| `st_HudLinearFilter 0` (`GL_NEAREST`) | **3,76** |
| `st_HudLinearFilter 1` (`GL_LINEAR`) | 1,95 |

Les bords sont **1,93 fois plus nets**. Et le diff pixel à pixel des deux
captures est **entièrement contenu dans la bande y 900–1080** : le monde n'a pas
bougé d'un canal. C'est la vérification qui compte — elle prouve à la fois que
l'effet porte et qu'il ne déborde pas.

> Piège de mesure noté : la **moyenne** des gradients ne distingue pas net de
> flou — elle valait 0,079 contre 0,078, un verdict de « aucun effet » qui était
> faux. Un flou étale le contraste sans le détruire, donc conserve la somme des
> écarts. Il faut le **RMS**, qui pénalise l'étalement. `hud_filtre_comparaison.png`
> dans `build/Release` montre les deux « HEALTH » l'un au-dessus de l'autre.

Défaut **0**. L'interface était floue depuis toujours dans EX ; elle est nette
maintenant, et c'est le point « GL_NEAREST sur le HUD » de la feuille de route
amont qui se coche.

### ✅ Les quatre correctifs de comportement KEX — 7 septembre 2026 (§8 point 12)

Deux des quatre s'appliquent à notre moteur, deux non. La différence n'est pas
une question de goût : elle tient à l'architecture du rendu, et se démontre.

#### `p_FixLineSkips` — implanté, défaut **1**

Le binaire situe lui-même le correctif. La chaîne `p_fixlineskips` est stockée
**immédiatement après** `PIT_CheckLine: spechit overflow!` — l'ordre du pool de
littéraux dit que les deux viennent du même fichier, leur `p_map.c`. Le
correctif est donc dans la collision, pas dans le mouvement du joueur.

Le mécanisme du saut, retrouvé dans notre code : `P_CheckPosition` remplit
`spechit` depuis les cases de blockmap autour de la **destination seule**
(`P_BlockMapBox(bbox, x, y, tmthing)`, `p_map.cc`), puis `P_TryMove` ne
déclenche que ce qui s'y trouve. Or `P_PlayerXYMovment` ([p_user.cc:437]) fait
**un seul** `P_PlayerMove` avec toute la quantité de mouvement — contrairement à
`P_XYMovement` ([p_mobj.cc:224]) qui, lui, découpe à `MAXMOVE` et porte le
commentaire « *[d64] fixed bug with fast projectiles going through walls* ». Le
joueur n'a jamais reçu ce découpage.

Chiffré, le seuil est net. `forwardmove[1] = 0x2c`, poussée `× 2048` =
1,375 unité/tic, friction `0xd200` = 0,8203 → vitesse terminale
`1,375 / 0,1797` ≈ **7,7 unités/tic**, environ **10,8 en course + strafe**. Le
rayon du joueur est 16. Tant qu'on marche, toute ligne traversée tombe dans la
boîte de destination et rien n'est sauté. Il faut une explosion de roquette, un
broyeur ou une poussée de téléporteur pour dépasser 16 — c'est exactement le
« *when player is moving fast* » de leur description.

**Le correctif ne touche pas au déplacement.** Collision, glissement le long des
murs, ramassage d'objets : inchangés. Seul grandit l'ensemble des lignes
examinées, de « dans la boîte de destination » à « n'importe où sur le trajet ».
Le test de traversée lui-même est le même — le centre passe d'un côté à l'autre
— donc le nouvel ensemble est un **sur-ensemble strict** de l'ancien : le
correctif ne peut que rajouter ce qui manquait. Sous le seuil, les deux
ensembles coïncident et rien ne change.

`P_CrossPathLines` ([p_map.cc]) fait un `P_PathTraverse(PT_ADDLINES)` de
l'ancienne à la nouvelle position. Les lignes sont **collectées puis déclenchées
après** : un traverseur ne doit pas appeler `P_UseSpecialLine` directement,
puisqu'une ligne de téléport déplace la chose et lance son propre
`P_PathTraverse`, dont l'état est dans des globales. La boucle s'arrête si la
position a changé — le reste du trajet ne décrit plus rien.

Défaut **1**, comme chez eux. `p_FixLineSkips 0` rend le comportement d'origine
au bit près : la branche `else` est le code inchangé.

#### `r_ForceInterpolatedAngleReset` — implanté, défaut **0**

L'angle caméra ne bouge qu'au tic. L'interpoler étale chaque rotation sur les
images suivantes : c'est fluide, et cela met jusqu'à **un tic entier — 28 ms** —
entre le mouvement de souris et la réponse de la vue. Le remettre à zéro à
chaque image échange l'un contre l'autre : l'angle se cale sur ce que le tic a
laissé, la visée répond tout de suite et avance par paliers de 35 Hz.

Une ligne dans `R_SetupFrame` ([r_main.cc]) : `anglelerp` remplace
`*i_interpolateframes` pour `viewangle` et `viewpitch` **seulement**. La
position reste interpolée dans les deux cas, donc le monde continue de glisser
sous une vue qui, elle, avance par crans. Défaut 0 : la fluidité vaut le tic de
latence à la manette, et qui joue à la souris a l'interrupteur.

#### `r_clipspritefragments` / `r_clipallspritetypes` — écartés, avec preuve

Ces deux-là réparent un problème que **notre architecture n'a pas**, et la
démonstration tient en trois faits vérifiés.

**Un.** L'original n'a pas de tampon de profondeur. Recherche exhaustive dans
DOOM64-RE : aucun `G_ZBUFFER`, aucune allocation de z-buffer, une seule
occurrence de `ZMODE_OPA` dans une macro de mode de rendu (`doomdef.h:1063`). Le
tri est **entièrement** l'ordre BSP.

**Deux.** L'original dessine les sprites *à l'intérieur* de chaque sous-secteur :
`sub->vissprite` est une liste chaînée par sous-secteur, parcourue au tour de ce
sous-secteur (`r_phase3.c:838`). Un sprite qui déborde sur un sous-secteur voisin
dessiné **plus tard** se fait donc repeindre par la géométrie de celui-ci. D'où
la découpe en fragments, un par sous-secteur traversé — et d'où l'option de
n'en découper qu'une partie (« *only corpses and shootable sprites* »), les
cadavres étant les plus visiblement fautifs puisqu'ils s'étalent au sol.

**Trois.** Nous ne dessinons pas comme ça. `R_RenderWorld` ([r_scene.cc:346])
passe **toute** la géométrie d'abord — `DL_ProcessDrawList(DLT_WALL)` puis
`(DLT_FLAT)` — et **tous** les sprites ensuite, `dglDepthMask(GL_FALSE)`,
triés d'arrière en avant par `SortSprites` ([r_drawlist.cc:92]), avec
`GL_DEPTH_TEST` actif et un tampon de profondeur bien présent sur la cible
hors-écran (`GL_DEPTH24_STENCIL8`, `postprocess.cc:1423`). L'occultation
sprite/mur est donc résolue **au pixel** par le z-buffer, et l'ordre
sprite/sprite par le tri global. Il n'existe aucun instant où un mur peut être
dessiné après un sprite qu'il devrait laisser voir.

Implanter la découpe par fragments chez nous coûterait du temps CPU pour un
résultat **pixel pour pixel identique**. C'est le bon réflexe de l'avoir
regardée ; c'est le bon réflexe de ne pas l'écrire.

> **⚠ CORRECTION du 7 septembre 2026 — cette conclusion était fausse.** Le code
> source de Kex 3 (PowerslaveEX, voir §8) donne le mécanisme réel, et il n'a rien
> à voir avec ce que la description de la cvar 3.8 laissait supposer. Ce n'est pas
> une découpe en fragments par sous-secteur : c'est une **passe de stencil par
> secteur**, et le sprite y est redessiné **le test de profondeur ÉTEINT**
> (`kexRenderScene::FixSpriteClipping`, `renderScene.cpp:1579`).
>
> Ce que cela répare est donc l'inverse de ce que j'avais compris : le z-buffer
> n'est pas la **solution** ici, il est la **cause**. Un sprite qui coupe un mur ou
> un sol se fait **trancher** par la géométrie qu'il traverse — le monstre collé au
> mur sectionné en diagonale. La passe marque la géométrie du secteur dans le
> stencil (faces avant à 128, faces arrière incrémentées), puis dessine le sprite
> **entier**, sans test de profondeur, borné au seul volume du secteur.
>
> **Notre moteur a donc l'artefact**, et précisément parce qu'il teste la
> profondeur sur les sprites. Le raisonnement « nous avons un z-buffer, donc le
> problème n'existe pas » était juste sur l'occultation et faux sur la découpe.
> Reste à mesurer l'ampleur chez nous avant d'écrire quoi que ce soit — et
> c'est maintenant faisable proprement, la source étant en GPL v2+.
>
> Leçon, plus utile que le correctif : *une description de cvar dit ce qu'un
> correctif fait, jamais comment. J'ai déduit le mécanisme de la formulation
> « clipped as fragments for every subsector crossed » et j'ai construit une
> démonstration entière sur cette déduction. La démonstration était rigoureuse ;
> sa prémisse était inventée.*

> À retenir de ces quatre-là : *une cvar de KEX décrit un correctif, pas
> toujours un correctif qui nous concerne. Ce qui se transpose, c'est le
> problème qu'elle nomme — pas la solution, qui dépend de leur pipeline.*

---








### ⛔ Démos N64 et générateur aléatoire — tenté puis ANNULÉ le 8 septembre 2026

**Tout le code décrit ici a été retiré à la demande de Dylan.** Le moteur est
revenu à son état d'avant : générateur de Boom intact avec ses `pr_class`, aucun
lecteur de démo N64, `rom/device.cc` ignorant de nouveau les lumps `DEMO`.
Vérifié par recherche — plus une occurrence de `demotrace`, `n64demo`,
`p_demovbls`, `p_boomrandom` ni `G_BeginRawDemo` dans l'arbre.

Ce qui suit est **le savoir**, pas le code. Il a coûté une soirée et évitera de
tout redécouvrir si la question revient.

#### Le format de démo N64, entièrement décodé

```
13 × uint32 big-endian   configuration : quel bit de manette fait quoi
 1 × uint32 big-endian   sensibilité du stick analogique
 N × uint32 big-endian   un mot d'état de manette par tic
```

**Big-endian**, aucune signature. Les treize mots sont la structure `buttons_t`
(DOOM64-RE `doomdef.h:1255`) : RIGHT, LEFT, FORWARD, BACK, ATTACK, USE, MAP,
SPEED, STRAFE, STRAFELEFT, STRAFERIGHT, WEAPONBACKWARD, WEAPONFORWARD. Les
**seize bits bas** du mot de manette portent le stick analogique, deux octets
signés, Y en 0-7 et X en 8-15.

Ni difficulté ni carte dans le fichier — l'appelant les fournit
(`d_screens.c:7`). Table de l'original (`d_main.c:46`) : DEMO1→map 3,
DEMO2→map 9, DEMO3→map 17, DEMO4→**map 32 (Hectic)**, toutes en `sk_medium`.
Fin de démo : `PAD_START` ou 4 000 entiers consommés.

Conversion en `ticcmd_t` : transcription de `P_BuildMove` (`p_user.c:270`). Les
échelles concordent — `TICRATE` vaut 30 chez nous comme chez eux,
`forwardmove {0x1c,0x2c}` est leur `{0xE000,0x16000}` sur 2048, les vingt
`angleturn` correspondent valeur pour valeur, et leur `table << 17` fois deux
champs vidéo égale notre `table << 2` puis `INT2F`.

#### Trois obstacles trouvés, et leur cause

1. **`rom/device.cc:157` jette les lumps `DEMO`**, derrière un TODO d'amont.
   C'est pourquoi rien ne joue après la carte de titre.
2. **Le mauvais décompresseur.** `lzss` ne vaut que pour les sprites et les GFX
   — notre propre `lzss.cc` le dit en tête de fichier. Les démos passent par
   `deflate` (le `dec_d64` de l'original). Envoyées à `lzss` elles sortaient à
   **75 octets de bruit au lieu de 16 000**.
3. **`demoend` est déclaré et jamais assigné.** Invisible tant que le seul
   lecteur s'arrête sur un octet `DEMOMARKER`.

#### La boucle d'attraction n'atteint jamais les démos

Mesuré en instrumentant `Title_Ticker` : le compte à rebours des trente secondes
démarre (0, 60, 120… 240 sur 900) **puis `mainmenuactive` bascule à 0**,
`pagetic` est remis à `gametic` à chaque tic, et le compteur ne repart jamais.
Défaut d'EX indépendant des démos. L'ordre d'EX diffère d'ailleurs de
l'original : titre → crédits → démos chez nous, TitleMap → démos → crédits chez
eux.

#### Sur la synchronisation, ce qui a été établi

- **Le moteur est déterministe** : deux exécutions d'une même démo donnent des
  traces rigoureusement identiques.
- **Les démos du `DOOM64.WAD` de KEX et celles de la ROM sont le même
  enregistrement** — traces identiques une fois comparées à armes égales.
- **L'original n'utilise pas le générateur de Boom** mais
  `rndtable[++index & 0xff]`, deux index indépendants (`d_main.c:116`). Une
  démo ne peut se rejouer que sur le générateur contre lequel elle a été faite ;
  ce n'est pas un problème de déterminisme mais **d'accord**.
- **`P_RandomShift` rend `(premier - second)` là où l'original calcule
  `(second - premier)`** (`p_pspr.c:695` et `:856`, écrit longhand exprès).
  Distribution identique, donc invisible en jeu — pas pour une démo. **Mesuré :
  inverser l'ordre ne change rien sur les premiers tics.**
- **L'original ne tourne pas d'un angle fixe par tic** :
  `angle += vblsinframe[0] * angleturn`, où `vblsinframe` est le nombre de
  champs vidéo depuis la dernière image, donc **variable selon la vitesse
  d'affichage de la console** (`d_main.c:164`, champ commenté « range from 4 to
  8 »). Piste sérieuse pour une dérive angulaire ; essayer 3 et 4 a empiré les
  choses, donc 2 n'est pas le problème.

#### Deux erreurs de méthode à ne pas refaire

**Du code mort pris pour un correctif.** `gameflags` et `compatflags` avaient été
posés dans le chemin de démo — mais `G_InitNew` appelle `G_SetGameFlags()` juste
après, qui les reconstruit depuis les cvars et jette tout. Annoncé comme une
correction, ce n'en était pas une. *Avant d'affirmer qu'une ligne corrige
quelque chose, vérifier que rien en aval ne l'écrase.*

**Compter les appels par fichier ne prouve rien.** EX a réparti le code
autrement : RE a deux `P_Random` dans `p_user.c`, nous un seul — et le second
existe bel et bien, dans `p_spec.cc`. Il faudrait comparer fonction par
fonction.

#### Ce qu'il aurait fallu avant de toucher au code

Un **témoin**. `-demotrace` produisait une trace de notre moteur ; il manquait
celle de l'original. DOOM64-RE est compilable : le construire, y ajouter les
mêmes lignes de trace, jouer `DEMO1LMP` et diffuser les deux journaux donne le
tic exact où les deux moteurs cessent d'être d'accord.

Sans ce témoin, chaque correctif est un pari — et quatre paris successifs, même
justes pris un à un, ont fini par rendre le résultat pire qu'au départ sans
qu'on puisse dire lequel. **Si la question est reprise, c'est par là qu'il faut
commencer.**


### ✅ Luminosité additive et gravité d'origine — 7 septembre 2026

Deux corrections tirées de l'article de Kaiser, toutes deux vérifiables, toutes
deux vérifiées.

#### La luminosité additionnelle additionne

`gbuffer.shader` faisait `result.rgb * uEnvBrightness`. Kaiser : « *It applies
an **additive layer** over the screen* ». C'est maintenant
`result.rgb + (uEnvBrightness - 1.0)`, et la borne de la cvar passe de
`[0,05 ; 4]` à `[0 ; 2]` — leur plage réelle, 1 au neutre.

La différence n'est pas cosmétique : **une multiplication laisse le noir noir,
une addition le relève.** C'est ce que les comparaisons côte à côte de MAP17
montraient dans les coins sombres sans qu'on sache le nommer.

**Mesuré** — deux captures, `r_Brightness 1.0` contre `1.5`, hausse moyenne par
bande de luminosité de départ :

| valeur de départ | hausse |
|---|---|
| 0–15 (les noirs) | **+127,0** — 51 813 pixels |
| 16–31 | **+127,0** |
| 32–47 | **+127,0** |

Une constante, identique jusque dans les noirs. C'est la signature arithmétique
de l'addition : une multiplication aurait donné +0 sur le noir et une hausse
proportionnelle ailleurs. `0,5 × 255 = 127,5`, et le compte est bon.

#### La gravité, aux expressions exactes de l'original

DOOM64-RE, où `GRAVITY` vaut `FRACUNIT*4` — notre `GRAVITY` étant `FRACUNIT`,
soit leur `GRAVITY/4` :

| | poussée initiale | accélération |
|---|---|---|
| joueur, original (`p_user.c:195`) | `-(GRAVITY/2)` = −2 | `GRAVITY/4` = 1 |
| mobj, original (`p_base.c:309`) | `-(GRAVITY/2)` = −2 | `(GRAVITY/FRACBITS)*3` = **0,75** |
| **avant**, chez nous | −1 | 1 pour les deux |
| **après** | `-(GRAVITY*2)` = −2 | joueur 1, mobj `(GRAVITY*3)/4` = 0,75 |

L'original commente lui-même la ligne : `// [d64]: non-players fall slightly
slower`. 0,75 sur 1, ce sont les 25 % de l'article, au chiffre près.

Trois écarts corrigés ; l'accélération du joueur était la seule déjà juste. Le
seuil d'accroupissement (`-GRAVITY*8`) correspondait déjà exactement à leur
`-(GRAVITY*2)`, et c'est ce qui a rendu l'échelle lisible.

**Conséquence** : tout cadavre, toute arme lâchée, tout projectile soumis à la
gravité tombait un tiers trop vite dans ce moteur, et démarrait à la moitié de
la vitesse qu'il devait.

> `-demotrace`, l'outil de trace décrit ici à l'origine, a été **retiré avec le
> reste du travail sur les démos le 8 septembre 2026**. Son principe reste
> consigné plus haut, dans la section « Démos N64 et générateur aléatoire ».


### L'article de Kaiser (24 mars 2020) — ce qu'il dit et ce qu'il nous coûte

`https://doom64ex.wordpress.com/2020/03/24/differences-between-doom-64-ex-2-5-versus-the-official-remaster/`

Kaiser y liste lui-même ce qui sépare EX 2.5 du remaster officiel. C'est la
source la plus autorisée qui existe sur la question, et elle **corrige deux
choses que nous avions faites de travers**.

#### ⚠️ Le tampon de profondeur — troisième version de cette analyse, et la bonne

> « *Depth buffer is no longer used and instead now follows how the original game
> rendered the scene. This means that sprites will no longer be cut off by floors
> or walls depending on the order of subsector draws. Additionally, sprites are
> now fragmented based on the subsectors they overlap* »

**KEX a purement et simplement abandonné le tampon de profondeur.** Ils rendent
la scène comme l'original : dans l'ordre du BSP, sans z-buffer.

Il faut être clair sur le parcours de cette question, parce qu'il est instructif :

1. **Le 7 septembre au matin**, j'ai déduit le mécanisme de la description de la
   cvar (« *clipped as fragments for every subsector crossed* ») et **écarté** les
   deux cvars en démontrant en trois points que notre z-buffer réglait déjà le
   problème.
2. **L'après-midi**, la source de Kex 3.0 a montré une passe de stencil, et j'ai
   corrigé : le z-buffer n'est pas la solution mais la cause. C'était mieux, mais
   ça décrivait l'**ancêtre de 2014**, pas Doom 64 KEX.
3. **Maintenant**, l'auteur lui-même : pas de stencil, pas de z-buffer du tout.

Ce qui est confirmé au passage : **notre moteur a bien l'artefact.** Nous
utilisons un tampon de profondeur, donc nos sprites sont coupés par les sols et
les murs qu'ils traversent, et c'est précisément ce que Kaiser dit avoir
supprimé.

> Leçon, et elle vaut plus que le correctif : *deux fois j'ai reconstruit un
> mécanisme au lieu de chercher si l'auteur l'avait écrit quelque part.* La
> description d'une cvar dit ce qu'un correctif fait ; un billet de blog de
> l'auteur dit comment. Chercher la seconde source avant de déduire.

Reproduire cela chez nous n'est pas une petite affaire — c'est renoncer au
z-buffer pour le monde et trier par sous-secteur. À peser, pas à faire dans la
foulée.

> ✅ **Fait le 8 septembre 2026** — `r_DepthBuffer`, défaut 0. Voir la section
> « Le tampon de profondeur abandonné » plus bas. La géométrie sort identique au
> pixel près sur quatre cartes, pour −3,8 % de framerate. La fragmentation des
> sprites, elle, n'est pas faite : ce n'est pas l'original, c'est un ajout de KEX
> par-dessus la réaffectation que RE fait déjà.

#### ⚠️ La luminosité additionnelle est **additive**, pas multiplicative

> « *Additional brightness setting in addition to the game's original brightness
> setting. It applies an **additive layer** over the screen* »

Notre `gbuffer.shader` fait `result.rgb = saturate(result.rgb * uEnvBrightness)`
— **une multiplication**. Kaiser dit une **couche additive**. C'est le
`r_Brightness` implanté le 6 septembre, et il est mathématiquement faux par
rapport au leur : une multiplication laisse le noir noir, une addition le
relève. C'est exactement la différence qu'on voyait dans les zones sombres sans
arriver à la nommer.

Correction à faire, une ligne.

#### La gravité, chiffrée exactement

> « *Non-player objects have slower gravity (they fall down %25 slower than the
> player)* »

Vérifié dans DOOM64-RE, qui donne les expressions exactes — `GRAVITY` y vaut
`FRACUNIT*4` :

| | initial | accélération |
|---|---|---|
| joueur, original (`p_user.c:195`) | `-(GRAVITY/2)` = −2 | `GRAVITY/4` = **1** |
| mobj, original (`p_base.c:309`) | `-(GRAVITY/2)` = −2 | `(GRAVITY/FRACBITS)*3` = **0,75** |
| joueur, chez nous (`p_user.cc:511`) | `-GRAVITY` = **−1** | `GRAVITY` = 1 |
| mobj, chez nous (`p_mobj.cc:357`) | `-GRAVITY` = **−1** | `GRAVITY` = **1** |

0,75 / 1 = les 25 % annoncés, au chiffre près. Le commentaire de l'original le
dit même en toutes lettres : `// [d64]: non-players fall slightly slower`.

**Trois écarts** chez nous : l'accélération des mobjs (1 au lieu de 0,75), et la
poussée initiale des deux (−1 au lieu de −2). L'accélération du joueur, elle,
est juste.

#### Le reste de la liste, et où nous en sommes

| ce que dit Kaiser | chez nous |
|---|---|
| Renderer à pipeline de shaders | ✅ fait |
| Émulation du filtre 3 points N64 | ✅ `r_N64Filter` |
| Meilleure gestion du 16/9 | ✅ `widescreen` dans `gl_main.cc` |
| Regard haut/bas et saut **retirés** | ❌ nous avons `v_MLook` et `p_AllowJump` — choix à faire |
| Rendu des nuages corrigé | ❓ à vérifier dans `r_sky.cc` |
| Les démos fonctionnent (dont la démo *hectic* cachée) | ❓ à tester |
| Timings de scripts décalés d'une image — porte bleue de *Breakdown*, *Main Engineering* | ❌ probablement hérité d'EX |
| Collision d'origine intégralement conservée, **bugs compris** | ⚠️ tension avec notre `p_FixLineSkips` à 1 |
| Projectiles explosent si le joueur est à ~5 unités d'un mur | ❌ à vérifier |
| Monstres déclenchent les linedefs « death trigger » (fléchettes d'*Even Simpler*) | ❌ à vérifier |
| Logique des lumières clignotantes et du glow corrigée | ❌ à vérifier |
| `RaiseToNearest` / `RaiseAndChange` à **demi-vitesse** | ❌ à vérifier |
| Randomisation des plateformes perpétuelles | ❌ à vérifier |
| Automap : maintenir *use* pour se déplacer | ❓ à vérifier |
| WAV au lieu de MIDI pour les bruitages | ❌ nous sommes en MIDI |

Kaiser précise que plusieurs de ces corrections étaient **nécessaires pour que
les démos se synchronisent** — c'est donc un test de non-régression tout prêt :
*si la démo cachée « hectic » se déroule jusqu'au bout sans dériver, la physique
et la logique des secteurs sont justes.* Aucun screenshot ne vaut ça.

### ✅ Le tampon de profondeur abandonné — 8 septembre 2026

Le premier point de la liste de Kaiser, et le plus structurel :

> « *Depth buffer is no longer used and instead now follows how the original game
> rendered the scene. This means that sprites will no longer be cut off by floors
> or walls depending on the order of subsector draws.* »

**`r_DepthBuffer` — défaut 0.** À 0 le monde est composé dans l'ordre du BSP,
sans aucun test de profondeur ; à 1 on retrouve exactement le chemin de
Doom64EX. Le nom dit ce qu'il commute, pas ce qu'on en pense.

#### Comment l'original s'y prend, lu dans DOOM64-RE

`R_BSP` (`r_phase1.c:33`) descend l'arbre d'avant en arrière avec son tampon de
colonnes et **empile** les sous-secteurs visibles dans `solidsubsectors`.
`R_RenderAll` (`r_phase3.c:21`) parcourt ce tableau **à l'envers** :

```c
while (endsubsector--, (endsubsector >= solidsubsectors))
{
    sub = *endsubsector;
    R_RenderWorld(sub);
    sub->drawindex = 0x7fff;
}
```

Le plus lointain est donc peint d'abord. Et `R_RenderWorld` traite **un
sous-secteur en entier** : ses murs, son plafond, son sol, **puis ses sprites**
(`R_RenderThings`). Un sprite est ainsi recouvert par la géométrie de tout ce
qui est devant lui, et par rien d'autre.

**L'original a déjà sa parade au débordement**, et je ne l'avais pas vue :
`R_AddSprite` (`r_phase1.c:465`) calcule les deux bords d'un sprite large,
cherche leur sous-secteur, et **réaffecte le sprite au plus proche des trois** —
donc au dernier dessiné. Limité aux `MF_CORPSE|MF_SHOOTABLE` et à moins de
`MAXZ` (256 unités).

C'est l'ancêtre direct de `r_clipallspritetypes` : « *If disabled, only corpses
and shootable sprites will be clipped* » n'est pas une option de KEX, **c'est le
comportement de la N64**.

#### Ce qui a été fait

| | |
|---|---|
| `subsector_t.drawindex` | rang dans le parcours de l'image, 1 pour le plus proche. Le nom et le sentinelle `DRAWINDEX_DONE` viennent de RE |
| `vtxlist_t.drawindex` | tamponné par `DL_AddVertexList`, ce qui suffit puisque les listes sont remplies dans l'ordre du parcours |
| `DL_ProcessWorldPainter` | parcourt les sous-secteurs du plus lointain au plus proche, murs → plafond/sol → sprites |
| `R_SpriteDrawIndex` | la réaffectation de RE, aux deux points de sonde et au seuil `MAXZ` près |
| `R_AddLeaf` | plafond **avant** sol, et couche d'eau opaque avant la couche à alpha 160 |

**Le tri disparaît, et c'est là qu'était le piège.** `SortDrawList` triait par
`texid` décroissant, et ce tri **portait accidentellement deux ordres** dont le
rendu dépendait : les deux couches d'un sol liquide (RE dessine `floorpic+1`
opaque puis `floorpic` à alpha 160, et `floorpic+1 > floorpic` remettait par
arithmétique les couches dans le bon sens), et le plafond avant le sol. Les deux
sont maintenant explicites dans l'ordre d'ajout.

**La fusion des lots survit.** C'était la crainte — la fusion des batchs est ce
qui avait refermé l'écart de 132 FPS. Elle tient, parce que les primitives d'un
même `glDrawElements` sont rastérisées **dans l'ordre de soumission** : peindre
et fusionner ne sont pas contradictoires tant qu'on ne trie pas. La géométrie
s'accumule d'un sous-secteur à l'autre et n'est vidée que devant un sprite.

#### L'erreur commise, et ce qu'elle a prouvé

J'avais allumé le blending pour toute la passe, en me disant que les sols et les
sprites en avaient besoin. Le chemin d'origine le laisse **éteint sur les murs**
et ne l'allume qu'avant les sols. La différence s'est vue tout de suite sur la
grille au fond de MAP01 : ses texels d'alpha intermédiaire, opaques avant,
devenaient translucides.

Corrigé en commutant l'état **par classe** dans `DL_PainterGroup`, avec purge du
lot en cours devant chaque changement — un draw ne porte qu'un état de blending.

Et cette erreur a offert la meilleure vérification du lot : une fois réparée,
**la géométrie sort identique au pixel près**.

#### Mesuré

MAP01/03/05/08, tic 160-200, 1920×1080, sprites coupés pour isoler la géométrie :

| | pixels différents sur 2 073 600 |
|---|---|
| MAP01 | **0** |
| MAP03 | **0** |
| MAP05 | **0** |
| MAP08 | **0** |
| MAP20 | 36 (0,002 %) |

**Zéro.** L'ordre BSP et le test de profondeur donnent rigoureusement la même
image sur la géométrie opaque — ce qui devait être, puisque le BSP *est* un tri
par profondeur exact. C'est la démonstration que le chemin est correct, et non
seulement plausible.

Les 36 pixels de MAP20 sont **une seule colonne**, `x 844`, `y 404..439` : une
égalité de rastérisation sur l'arête entre deux murs, tranchée autrement quand
plus rien ne départage par la profondeur. Contrôle fait — deux exécutions au
même réglage donnent 0 sur MAP20, donc ces 36 pixels sont réels et non du bruit.

**Sprites allumés**, MAP01 tic 200 : 0,108 % des pixels changent, tous dans la
bande `y 813..879`, là où les sprites rencontrent le sol. C'est le correctif :
le sprite n'est plus tranché.

| | |
|---|---|
| coût | 605 → **582 FPS** médians, soit **−3,8 %** |
| erreurs GL | **0**, MAP01/05/12 en `-glcheck` |
| balayage des 32 cartes | aucun plantage, aucune erreur GL, aucune texture sans entrée d'atlas |
| `r_SAO 1` sur les deux chemins | **identique au pixel près** |

Le coût vient des appels de dessin : deux par sous-secteur au lieu de deux pour
toute la scène, puisque murs et sols alternent et ne partagent pas leur état de
blending. Quatre pour cent pour l'ordre de composition de l'original.

**`r_SAO` n'est pas affecté, et c'est important :** la cible de profondeur du
G-buffer est écrite par le shader comme une couleur, pas par le tampon de
profondeur. En ordre peintre le fragment le plus proche est écrit en dernier,
donc il gagne — la cible est juste sans qu'on ait rien à faire.

#### Piège de mesure rencontré, à ne pas refaire

**MAP12 donnait 100 % de pixels différents.** Ce n'était pas le rendu : le
joueur y démarre dans la lave, la santé valait 100 d'un côté et 67 de l'autre, et
le voile rouge de dégâts teinte tout l'écran. Deux exécutions **au même réglage**
donnent 99,999 % de différence — la carte n'est tout simplement pas un point de
mesure.

C'est exactement ce que la passation disait déjà de MAP31. **Avant de conclure
qu'un écart vient du changement, mesurer l'écart du changement contre
lui-même.** Le contrôle coûte deux minutes et vaut une soirée.

#### Ce qui reste — la fragmentation

Kaiser ajoute : « *Additionally, sprites are now fragmented based on the
subsectors they overlap* ». **Ça n'est pas fait, et ce n'est pas l'original :**
RE réaffecte le sprite au sous-secteur le plus proche, il ne le découpe pas. La
fragmentation est un ajout de KEX par-dessus.

La réaffectation couvre le cas courant. Le cas qu'elle ne couvre pas est un
sprite qui traverse **trois** sous-secteurs ou plus, ou dont le milieu tombe
dans une cellule dessinée entre les deux extrémités. Fragmenter demanderait de
découper le quad du sprite contre le polygone convexe de chaque sous-secteur
traversé, en coordonnées monde, et de le dessiner au tour de chacun. C'est un
chantier à part entière — à ouvrir seulement si l'artefact se voit en jeu.

Les deux cvars de KEX correspondent exactement à ces deux crans :
`r_clipspritefragments` (la fragmentation, non faite) et
`r_clipallspritetypes` (étendre la réaffectation au-delà des cadavres et des
choses tirables — un `if` à retirer dans `R_SpriteDrawIndex`).

> **Correction à la section « Les quatre correctifs de comportement KEX ».** Le
> passage qui dit « notre moteur a donc l'artefact » n'est plus vrai depuis ce
> travail. Il l'avait **par le z-buffer**, et le z-buffer est parti.

### Comment UZDoom résout les deux mêmes problèmes — ré-analyse du 7 septembre 2026

Question posée : comment UZDoom gère-t-il l'équivalent de `gbuffer.shader` et de
`generic.shader` ? Réponse : **il n'a pas de `generic.shader`, et son
`gbuffer.shader` s'appelle `pp/present.fp`.**

#### D'abord la licence, parce qu'elle décide de ce qu'on peut faire

| fichier | licence |
|---|---|
| `shaders/glsl/main.fp`, `main.vp` | **GPL-3.0-or-later seule** |
| `shaders/pp/*.fp`, `pp/screenquad.vp` | **GPL-3.0-or-later seule** |
| `src/common/engine/renderstyle.h` | GPL-3 **+ BSD-3-Clause** (code antérieur à 2026) |
| `src/common/2d/v_2ddrawer.cpp` | GPL-3 **+ BSD-3-Clause** |
| `src/common/rendering/hwrenderer/data/hw_shaderpatcher.cpp` | GPL-3 **+ BSD-3-Clause** |

La coupure est nette et commode : **leur architecture C++ est à double licence,
donc reprenable ; leurs shaders ne le sont pas.** Ce qui tombe bien — nous
n'avons pas besoin des leurs, nous avons ceux de KEX, en GPL v2+.

#### Pas de shader générique : un seul über-shader

`main.vp` + `main.fp`, et tout le reste y est **concaténé à la compilation** :
une fonction d'éclairage (`func_normal`, `func_warp1/2/3`, `func_paletted`,
`func_pbr`, six variantes `fuzz_*`) et une fonction de matériau
(`material_normal`, `material_nolight`, `material_specular`, `material_pbr`),
plus des `#define`. La table des combinaisons est dans
`hw_shaderpatcher.cpp:283` — « Default », « Warp 1 », « Specular », « PBR »,
« Paletted », « No Texture »…

**Le 2D passe par le même shader que le monde.** Le HUD, les menus et la console
n'ont pas de programme à eux : `v_2ddrawer.cpp` pose un `mDrawMode` par quad et
c'est tout.

C'est le même principe que KEX (`doomSceneMain` compilé de cinq façons) et que
nous. Notre découpe en deux fichiers est simplement plus explicite.

#### Leur `uTextureMode` n'est pas notre `uTexMode`

Il faut le noter, car les noms trompent. `ETexMode` (`renderstyle.h:36`) compte
huit valeurs : `TM_NORMAL`, `TM_STENCIL` (1,1,1,a), `TM_OPAQUE` (r,g,b,1),
`TM_INVERSE`, `TM_ALPHATEXTURE` (1,1,1,gris), `TM_CLAMPY`, `TM_INVERTOPAQUE`,
`TM_FOGLAYER` — plus des bits de drapeau OR'és par-dessus (brightmap,
detailmap, glowmap).

Ce sont des **transformations du texel lui-même**, au service des render styles
de ZDoom. Notre `uTexMode` répond à une tout autre question : **comment la
texture se combine à la couleur de sommet** — `GL_MODULATE`, `GL_ADD`,
`GL_REPLACE`, texturage coupé. C'est l'environnement de texture du pipeline fixe
que Doom64EX employait.

Les deux axes sont orthogonaux, et **Doom 64 n'a pas besoin du leur** : il n'a
pas de `STYLE_Shaded` ni de brightmaps. Rien à reprendre ici, et c'est une
conclusion, pas un aveu.

#### `pp/present.fp` : leur `gbuffer.shader`, et il porte le gamma

Leur passe de présentation fait davantage que la nôtre :

- le gamma, avec **trois formules de luminance** au choix (`rec709`, « old »,
  moyenne) via un uniforme `GrayFormula`
- saturation, contraste, point noir et point blanc
- un **tramage** (`DitherTexture`)
- un écrêtage HDR à 2.0

**Le gamma vit donc chez eux dans la passe de présentation**, exactement là où
nous l'avons déplacé le 6 septembre en le sortant de la palette. Confirmation
indépendante d'une décision qui avait été prise à la mesure.

#### La seule idée à retenir : le tramage

Rien de leur code ne nous manque, mais une de leurs idées vaut d'être pesée.
Notre palette vient du **RGBA5551** de la N64 : 32 niveaux par canal étirés sur
256. C'est précisément le cas où un dégradé montre des bandes, et c'est ce que
leur `Dither` sur l'image finale sert à masquer.

À mesurer avant d'y toucher — l'original bande aussi, et une fidélité N64 bien
comprise peut vouloir garder ces bandes. Mais c'est la question intéressante que
cette lecture rapporte.

### Ce qu'il reste pour ressembler à KEX — état au 7 septembre 2026

Établi par diff : leurs cvars extraites du binaire contre nos 99 enregistrées,
débarrassées du bruit (variables membres, clés de localisation, backends D3D11 /
Vulkan hors périmètre). Classé par **ce qu'un joueur remarque**, pas par
difficulté.

#### 1. L'interface KEX — le plus visible de loin

Leur `Doom64.kpf` contient `gfx/` (87 fichiers), `fonts/` (4) et
`localization/` (6). C'est un habillage complet et moderne des menus et du HUD,
et c'est **la** différence qu'on voit en une seconde en lançant les deux
moteurs côte à côte. Rien d'autre sur cette liste n'a autant d'effet visuel.

Passe par la prise en charge des `.kpf`, déjà au programme. Le pk3 est déjà
découpé correctement pour ça (voir la section sur `d64ex/`).

#### 2. Découpler la logique du rendu — le plus gros écart de *sensation*

`cl_engineHZ` (« Frames per second to run the game logic at »), `cl_engineFPS`
(« Frames per second **the renderer** runs at », défaut 60), `cl_interpolation`
(« for high FPS and 144hz users »), `cl_maxlatetics`, `cl_engineNoSleep`.

Notre moteur est **cloué à 35 Hz** de logique avec interpolation par-dessus.
Le leur laisse monter la logique elle-même. C'est ce qui fait qu'une souris
répond différemment chez eux — et `r_ForceInterpolatedAngleReset`, fait le
7 septembre, n'en traite qu'un symptôme. Gros morceau, et le plus payant après
l'interface.

#### 3. Le traitement du son

`snd_lowpassfilter` + `snd_lowpassCutoffFreq` + `snd_lowpassGain`,
`snd_hardwarereverb`. Chez eux ce sont des DSP FMOD ; chez nous il faudrait les
écrire. Une part réelle de « on dirait le remaster » passe par là — un couloir
qui étouffe les sons ne s'imite pas au visuel.

#### 4. Fenêtrage et affichage — bon marché, et dans leur menu vidéo

`v_borderless` (**absent chez nous**, vérifié), `v_displaymonitor`,
`v_displayrestart`, `v_refresh`. Quelques heures, et ce sont des lignes que le
joueur voit dans le menu Vidéo.

`con_showfps` aussi : nous avons bien un compteur, mais seulement sous
`devparm` — pas de cvar.

#### 5. Parité fine du rendu

- `r_forcebufferclear` — « Clear framebuffer before drawing scene »
- `r_filtering` — le nôtre s'appelle `r_Filter` et sa polarité est inversée
- `r_motionblurshutterspeed` — le nôtre s'appelle `r_MotionBlurScale`
- `r_ambientocclusion` — le nôtre s'appelle `r_SAO`
- `r_clipspritefragments` / `r_clipallspritetypes` — **à rouvrir**, maintenant
  que le mécanisme réel est connu (passe de stencil, cf. la correction en §8)
- `gl_clipdistance`, `gl_useBindlessTextures`

#### Déjà couvert sous un autre nom

`r_greenblood` → notre `m_RegionBlood`. `st_showhud` → `st_DrawHud`.
`st_showmessages` → `m_Messages`. `r_resolutionscale_fixedscale` →
`r_ResolutionScale`. Ce ne sont pas des manques, seulement des noms.

#### Hors périmètre, et pourquoi

Les backends D3D11 et Vulkan (`d3d11_*`, `vk_*`, `r_rhiRenderFamily`), le
multi-threading du rendu (`r_gameThreadRenderCommands`, `jobs_*`), les gyros
(`cl_useGyros`) et tout `kexSocial*` / Bethesda.net. Utile seulement si l'on
vise autre chose qu'OpenGL sur PC.

### Ce que `DOOM64_x64.exe` dit des shaders — analyse du 7 septembre 2026

#### `BNet.kpf` : aucun shader

21 fichiers, 1,9 Mo. Trois polices (Montserrat, NotoSans ×2), dix icônes de
succès (`ACH_*`), six graphismes d'interface (`bnet_border`, `bnet_images`,
`bnet_rightstick`, `bnet_switch_on/off`, `spinner`). **Rien qui touche au
rendu.** Ce sont les ressources de la surcouche Bethesda.net — ce qui explique
au passage `bnetDisplayGrayscale.shader` dans `Doom64.kpf` (il grise la scène
derrière la surcouche) et les 27 classes `kexSocial*`.

#### 24 programmes chargés par leur nom

Le binaire nomme 24 chemins `progs/`, plus `dxShaders.bin` et `vkShaders.bin`.

**Deux fichiers du kpf ne sont jamais chargés** : `colorPicker.shader` et
**`downSampleDepth.shader`**. Du poids mort dans leur propre archive.

Le second nous concerne directement : **nous chargeons `downSampleDepth`, eux
non.** Ils ne prennent que `copyDepthMip`, et notre propre code explique
pourquoi c'est suffisant — `uMipIndex` à −1 vaut « copie directe », toute autre
valeur prend la branche qui réduit un niveau dans le suivant. Une simplification
possible, à mesurer avant d'y toucher.

Les corps partagés — `doomSceneMain`, `motionBlurMain`, `fxaa_main`,
`bilateralBlurMain` — n'apparaissent nulle part : ils sont inclus, jamais
chargés. Le motif « un `#define` plus un `#include` » est confirmé par l'absence.

#### `EMULATE_UNIFORM_BUFFERS` n'est pas dans le binaire

Zéro occurrence. **KEX ne définit jamais ce chemin : ils utilisent toujours de
vrais tampons d'uniformes.** L'émulation est un repli qu'ils ne prennent pas.

Ce qui replace `gl_UseUniformBuffers` (§8) sous son vrai jour : ce que notre cvar
*active* est leur comportement **par défaut**, et ce que nous avons par défaut
est un mode qu'ils n'empruntent jamais.

#### Le dialecte, et ce qu'il sait faire de plus

Quatre étages, pas deux : `SHADER_VERTEX`, `SHADER_PIXEL`, **`SHADER_GEOMETRY`**,
**`SHADER_COMPUTE`**. Le kpf n'en emploie que les deux premiers.

`__binding__` est bien présent (chemin Vulkan), ce qui confirme la lecture faite
de `common_glsl.inc`.

#### Le chemin OpenGL, dans le détail

`rhi\opengl\rhiShadersGL.cpp` livre presque toute sa mécanique :

- **GLSL 150 minimum** — *« GL shader version %i is below minimum requirements
  (150 or above is required) »*, soit OpenGL 3.2. Nous visons 3.3.
- Ils **injectent ` GLSL_VERSION=%i` dans chaque shader**, formaté par `%i%02i`
  — « 3.30 » devient 330.
- Les attributs de sommet sont liés par **nom généré** : `attrib%i`
  (`glBindAttribLocation`).
- Les sorties de fragment sont `outFragment0` à **`outFragment3`** — **quatre**
  cibles MRT. Nous en utilisons trois.
- Les plans de coupe passent par `uClipPlane%i` et `gl_ClipDistance`.
- **`uTex_` n'apparaît jamais** : ils ne résolvent pas les samplers par nom.
- `progs/default.shader` est **obligatoire** — *« CompileShader: Default shader
  not found »* et *« Default shader contains errors »* sont fatals.
- Leur préprocesseur borne l'imbrication des `#include`.

#### Trois backends, et un héritage

`kexRHIShaderManagerGL`, `...D3D11`, `...VK`, plus `kexRHIShaderGL/D3D11/VK`.
Sources sous `F:\devstuff\doom64ex\kex3_osiris\kexEngine\source\rhi\`.

Et au milieu : **`kexShaderObj::Compile` et `kexShaderObj::Link`** — exactement
la classe de PowerslaveEX / Kex 3.0. Dix ans plus tard, le même objet.

La compilation passe par le **système de jobs** (`pJobs`, cf.
`jobs_concurrentThreads`) et se chronomètre : *« Compile time: %fms »*. Le
D3D11 construit ses cibles en `vs_4_0` / `ps_4_0`, point d'entrée `main`.

#### Pourquoi les textures SMAA ne sont pas dans le kpf

`_areaTex` et `_searchTex` sont des noms d'objets internes, et `SearchTex`
apparaît en clair dans le binaire : **les deux textures de référence SMAA sont
embarquées dans l'exécutable**. D'où leur absence du kpf, et d'où la nécessité
où nous étions de fournir `smaa_area.raw` et `smaa_search.raw` nous-mêmes.

### Pourquoi `gbuffer.shader` et `generic.shader` ne sont pas dans `Doom64.kpf` — 7 septembre 2026

Parce qu'**ils sont à nous.** Le diff nom par nom entre notre `progs/` et le
vrai `Doom64.kpf` de Steam est net :

| chez nous, pas chez eux | chez eux, pas chez nous |
|---|---|
| `d64ex/gbuffer.shader` | `dxShaders.bin` |
| `d64ex/generic.shader` | `vkShaders.bin` |
| `d64ex/overrides.inc` | |
| `d64ex/smaa_area.raw`, `d64ex/smaa_search.raw` | |
| `d64ex/SMAA-LICENSE.txt` | |

**Tout le reste correspond exactement.** Les 36 fichiers de shader restants sont
les leurs, conservés tels quels — c'est la consigne de Dylan et elle est tenue.
Les six fichiers de `d64ex/` portent son copyright 2026. Leur kpf n'a jamais eu
de raison de les contenir.

#### Ce qui joue leur rôle chez KEX

**`generic.shader` ↔ leur `default.shader`.** Le leur fait 2 324 octets et tient
en une ligne utile : échantillonner `tBase`, multiplier par la couleur de
sommet. Le nôtre est bien plus gros parce qu'il doit **émuler le pipeline fixe**
— l'environnement de texture (`GL_ADD` pour le fondu d'écran et le ciel,
`GL_REPLACE` quand les lumières sont coupées), le brouillard propre au moteur,
le test alpha. Tout ce sur quoi le HUD, la console, les menus, l'automap, le
ciel et le melt de Doom64EX s'appuient encore, et que le profil core a supprimé.

**KEX n'a jamais eu cette dette.** Kex 3 est né shader-natif en 2014 : leur
chemin 2D n'a jamais eu d'environnement de texture à imiter, donc leur shader
générique peut se permettre d'être trivial.

**`gbuffer.shader` n'a pas d'équivalent chez eux** parce que ses deux tâches
sont ailleurs. Ramener le tampon hors-écran vers la fenêtre est une opération de
backend chez eux — leur RHI recopie, sans shader —, et leurs vues de débogage
sont des shaders séparés (`doomDisplayTexArray` pour la VRAM virtuelle, etc.).

**`dxShaders.bin` et `vkShaders.bin`** sont leurs archives précompilées D3D11 et
Vulkan (332 et 394 Ko). Sans objet pour un moteur GL seul, et elles expliquent
au passage les commandes `buildDX11ShaderArchive` et `vk_compileShaders` de leur
liste.

#### Ce que cela implique pour « rendre le kpf obligatoire »

Un fait mesuré qui corrige une hypothèse du projet : **les 36 fichiers de shader
du kpf portent tous l'en-tête GPL v2-or-later.** 39 occurrences de « *either
version 2* », zéro exception. Ce sont des logiciels libres sous **notre propre
licence**. Les redistribuer, ce que le pk3 fait déjà, est parfaitement légitime,
et **exiger l'achat Steam n'est pas nécessaire pour les shaders**.

Si le kpf devient obligatoire, ce sera donc pour ce qu'il contient d'autre —
156 fichiers, 29 Mo : `gfx/` (87), `progs/` (40), `tactile/` (16),
`localization/` (6), `fonts/` (4). Le `DOOM64.WAD` n'y est pas, il est à côté.

Et **même en l'exigeant, `progs/d64ex/` reste indispensable** : ces six
fichiers-là ne sont pas dans leur kpf, y compris les deux textures de référence
SMAA que Nightdive n'y met pas non plus. La séparation actuelle du pk3 — leurs
fichiers à la racine de `progs/`, les nôtres dans `progs/d64ex/` — est donc déjà
exactement la bonne, et c'est ce qui rendra la bascule simple le jour venu.

### 🐛 Ouvert : projectiles plasma / laser unmaker / BFG parfois invisibles

Signalé par Dylan le 7 septembre 2026. Intermittent, **persiste toute la
session**, disparaît au redémarrage du jeu.

**Ce n'est pas le code des shaders.** C'est probablement le *système* de shaders,
et précisément l'atlas (`atlas.cc`), qui a remplacé les textures GL
individuelles par un `sampler2DArray`.

Le mécanisme colle exactement à la forme du symptôme. `atlas_build()` s'exécute
**une seule fois au démarrage**. Une image qui échoue à se placer ou à se lire
laisse derrière elle une `AtlasEntry` **remise à zéro** :

```cpp
} catch (...) {
    e = AtlasEntry {};
    ++skipped;
}
```

Une entrée à zéro échantillonne un rectangle de 0 × 0 : la chose n'est
simplement pas dessinée. Et comme l'atlas n'est construit qu'au démarrage, l'état
tient toute la session et **un redémarrage le reconstruit**. C'est mot pour mot
la description.

Deux causes possibles d'échec intermittent : une lecture qui rate à travers le
chemin ZIP/WAD (le lecteur ZIP a déjà eu un défaut réparé, commit `d4e0d00`), ou
un échec de placement.

**Hypothèse concurrente, plus faible** : `MAX_SPRITES 1024` dans `r_things.cc`.
Le débordement est déjà signalé (`R_AddSprites: Sprite overflow`), mais il est
*par image* et ne survivrait pas jusqu'au redémarrage — il irait et viendrait
selon ce qui est visible. Ça ne colle pas.

**Non reproduit.** Sur cette machine l'atlas annonce `1856 images in 9 layers,
0 skipped` à chaque essai. C'est une hypothèse dont la *forme* correspond, pas
une cause démontrée, et il faut le dire.

#### Ce qui a été fait : rendre l'échec bavard

Un bug intermittent qu'on ne sait pas reproduire ne se corrige pas à l'aveugle ;
on le fait s'annoncer. Trois changements :

1. Chaque image sautée nomme désormais **son lump et sa palette**
   (`Texture array: sprite PLSSA0 palette 0 could not be read`).
2. Le résumé passe en **avertissement** quand `skipped` n'est pas nul, avec la
   conséquence écrite en toutes lettres — sinon la ligne se perd parmi deux
   cents autres au niveau info.
3. `texatlasverify` devient une **commande de console**. Elle n'était joignable
   que par `-texatlasverify` en ligne de commande, ce qui ne sert à rien quand
   ce qu'on veut examiner est un sprite disparu dix minutes après le lancement.
   Le moment intéressant est pendant que c'est faux.

#### La prochaine fois que ça arrive

1. Ouvrir la console et taper `texatlasverify` → *checked / mismatched /
   unreadable*. Un `unreadable` non nul confirme l'hypothèse sur-le-champ.
2. Regarder le début du journal : la ligne `Texture array:` dit combien d'images
   ont été sautées, et lesquelles.

Si les deux sont propres, l'atlas est hors de cause et il faudra chercher du
côté du chaînage `thinglist` des secteurs — `R_AddSprites` saute tout ce qui
porte `MF_NOSECTOR`, et une chose délivrée de son secteur reste vivante mais
invisible.

#### Complément du 7 septembre 2026, après précision de Dylan

Deux faits qui changent le diagnostic :

- **Le bug est antérieur à la séance du 7 septembre.** Il existait déjà pendant
  la migration vers OpenGL 3.3 — c'est-à-dire depuis l'arrivée de l'atlas. Aucune
  modification de cette séance n'est en cause.
- **Il touche aussi les monstres.** Leur sprite disparaît, la chose reste là.
  Ce n'est donc pas propre aux projectiles : c'est **le chemin des sprites** en
  général.

Ce qui écarte au passage l'idée d'un ordre d'empaquetage non déterministe :
`wad::list_section` renvoie une vue sur un `std::vector`, donc l'ordre est stable
d'une exécution à l'autre.

#### Le défaut trouvé, et il produit exactement ce symptôme

`DL_ProcessDrawList` ([r_drawlist.cc]) résolvait l'entrée d'atlas puis :

```cpp
if(e.valid()) {
    shader::stamp_batch(...);
}
// et rien d'autre
```

**Il n'y avait pas de `else`.** Une entrée invalide — largeur ou hauteur nulle,
ce que laisse derrière elle toute image sautée à la construction — signifie que
les sommets **n'ont jamais reçu leurs coordonnées d'atlas**. Et ils étaient
dessinés quand même, en portant encore ce que le lot précédent avait tamponné
sur eux. Ils échantillonnent alors la page d'une autre texture, ou du vide, que
le test alpha jette : **la chose est dessinée, et invisible.** Un monstre qu'on
entend et qu'on ne voit pas.

Le lot est maintenant abandonné (`drawcount = first; continue;`) plutôt que
dessiné avec des coordonnées volées — il manque de toute façon, et ainsi il ne
corrompt pas ce qui partage son draw. Un avertissement le nomme une fois.

**Ce n'est pas prouvé être *la* cause**, faute d'avoir pu reproduire. Mais c'est
un vrai défaut, sur le bon chemin, dont la signature est celle décrite ; et s'il
reste quelque chose après, le moteur le dira maintenant au lieu de le taire.

#### Récidive du 8 septembre 2026 — et l'atlas est innocenté

Dylan a fait la mesure prévue. **L'atlas est hors de cause, par ses propres
chiffres :**

```
Texture array: 1856 images in 9 layers (36 MB), 0 skipped
Texture array verify: 1856 checked, 0 mismatched, 0 unreadable
```

Aucune image sautée à la construction, aucune illisible à la relecture GPU.
L'hypothèse principale de cette section — une `AtlasEntry` remise à zéro — est
donc **morte**. Le correctif du `else` manquant dans `DL_ProcessDrawList` reste
juste, mais ce n'était pas ça.

Aucun avertissement `Sprite overflow` non plus dans sa console. Les sprites
arrivent donc bien dans la drawlist, avec une entrée d'atlas valide, et ne sont
pas dessinés.

#### Le défaut trouvé : la géométrie est bâtie avant que ses dimensions existent

`R_GenerateSpritePlane` construit le quad à partir de quatre tableaux :

```c
dx1 = -spriteoffset[spritenum];
dx2 = dx1 + (float)spritewidth[spritenum];
z   = vissprite->z + spritetopoffset[spritenum];
z2  = z - (float)spriteheight[spritenum];
```

**Le seul endroit du moteur qui les écrivait était `GL_BindSpriteTexture`**
(`gl_texture.cc:619`), et uniquement sur la branche qui monte réellement la
texture — les deux sorties anticipées (`cursprite` inchangé, texture déjà en
VRAM) passent devant sans rien poser.

Deux conséquences, et la seconde est la mauvaise :

1. **L'ordre est inversé dans la drawlist.** `procfunc` — donc
   `R_GenerateSpritePlane` — s'exécute **avant** `GL_BindSpriteTexture`. La
   première image où un sprite apparaît est donc toujours bâtie avec les valeurs
   d'avant.
2. **Ces tableaux sont alloués par `Z_Malloc`, qui ne met pas à zéro**
   (`InitSpriteTextures`, `gl_texture.cc:527`) — à la différence de `spriteptr`
   et `spritecount`, qui sont en `Z_Calloc` juste à côté. Ce qu'ils contiennent
   avant la première écriture est le contenu précédent du tas.

Et **une largeur ou une hauteur nulle donne un quad d'aire nulle** : la chose est
là, vivante, audible, et rien n'est dessiné. C'est mot pour mot le symptôme.

C'est aussi **exactement la fragilité que cette passation signalait déjà pour
`texturewidth[]`** (« un cache rempli par effet de bord d'un upload… c'est là que
les dimensions devraient être posées, une fois pour toutes »). Le même défaut
existait pour les sprites et personne ne l'avait relié.

#### Le correctif

Les dimensions sont **amorcées à la construction du tableau de textures**
(`atlas.cc`), depuis l'image elle-même, pour les 951 sprites — l'atlas lit déjà
chaque image, la donnée est là et ne coûte rien.

```cpp
if (p == 0 && spritewidth && spriteheight && spriteoffset && spritetopoffset) {
    spritewidth[i]     = image.width();
    spriteheight[i]    = image.height();
    spriteoffset[i]    = image.sprite_offset().x;
    spritetopoffset[i] = image.sprite_offset().y;
}
```

**C'est un plancher, pas un remplacement**, et c'est ce qui rend le correctif
sûr : `GL_BindSpriteTexture` continue d'écrire ces valeurs à l'upload, donc
l'état stable est inchangé. Vérifié **par le code et non par une capture** :
`SetTextureImage` ne modifie `*origwidth` / `*origheight` que si
`r_TexNonPowResize > 0`, ce qui n'arrive jamais en 3.3 core où
`GLAD_GL_ARB_texture_non_power_of_two` est un `constexpr` vrai. La valeur
amorcée **est** la valeur montée.

Au passage l'amorce est la *meilleure* des deux : elle donne la taille de
l'image, là où l'upload rapporte la taille **après** complétion en puissance de
deux — c'est-à-dire le bug 13, celui qui avait rendu tous les sprites du jeu trop
grands.

#### Et le défaut se dénonce désormais

`R_GenerateSpritePlane` refuse un sprite sans taille et le nomme une fois :

```
R_GenerateSpritePlane: sprite 412 has no size (0x0); it cannot be drawn.
```

Si le bug revient, la console dit **quel** sprite et **que** c'est bien ça. S'il
revient *sans* cet avertissement, la cause est ailleurs et cette piste est morte
à son tour — ce qui vaut tout autant.

#### Ce qui n'est pas prouvé, et il faut le dire

**Le bug n'a jamais été reproduit sur cette machine.** Six cartes lancées après
le correctif : aucun avertissement, aucune erreur GL, `0 skipped`. On ne peut
donc pas montrer que le correctif répare ce que Dylan voit — seulement qu'il
ferme un chemin réel, dont la signature est exactement celle décrite, et qui
était atteignable.

#### ⚠️ Épilogue du même jour : cette récidive n'en était pas une

**C'était moi.** J'avais laissé `seta r_RenderSprites 0` dans le `config.cfg` de
Dylan en travaillant sur `r_DepthBuffer` ; ce réglage rend invisibles **tous** les
sprites du monde, monstres et projectiles compris, et survit aux redémarrages
puisqu'il est sauvegardé. C'est lui qui a trouvé la cause. Il n'y avait aucun bug.

Voir la section « Mesurer par capture » plus bas : `-config` est maintenant
réparé et tout essai automatisé écrit ailleurs que dans sa configuration.

**Ce qui reste vrai malgré tout, et qui vaut d'être gardé :**

- **Le signalement du 7 septembre, lui, est antérieur** à mon réglage et reste
  non expliqué. La section ci-dessus garde sa valeur pour ce cas-là.
- **L'atlas est innocenté** par les chiffres de Dylan, et ça ne dépend pas de la
  cause : `0 skipped`, `0 mismatched`, `0 unreadable`.
- **Le défaut d'ordre dans les dimensions de sprites est réel**, indépendamment
  de ce symptôme : la géométrie était bâtie à partir de tableaux `Z_Malloc`
  jamais mis à zéro, écrits seulement par effet de bord d'un upload postérieur.
  Le correctif tient, et l'avertissement aussi.

*Leçon, et elle est à moi : un réglage de test oublié dans la configuration de
quelqu'un ne se manifeste pas comme un réglage, il se manifeste comme un bug du
moteur — et c'est lui qui en paie le prix.*


---
### ✅ Le `doom64.wad` du remaster comme IWAD — 8 septembre 2026

Le moteur démarre désormais avec **l'un ou l'autre** fichier. Qui n'a que la ROM
joue avec la ROM ; qui n'a que le WAD du remaster joue avec le WAD.

#### Ce que contient le fichier, vérifié octet par octet

`IWAD`, **1 668 lumps, 15 103 212 octets**. Sections réelles :

| lumps | section | format |
|---|---|---|
| 0–952 | `S_START`/`S_END` — **951 sprites** | PNG indexé 4 et 8 bits, **chunk `grAb`** portant l'offset |
| 953–971 | 19 palettes, **hors section** | 768 octets RGB brut |
| 972–1476 | `T_START`/`T_END` — **503 textures** | PNG indexé, pas de `grAb` |
| 1477–1497 | **21 graphismes, dans aucune section** | PNG, palette et RGBA mêlés |
| 1498–1592 | `DS_START`/`DS_END` — **93 sons** | **RIFF/WAVE**, PCM 16 bits mono |
| 1593–1618 | `DM_START`/`DM_END` — **24 musiques** | **MIDI standard** (`MThd`) |
| 1619–1658 | **MAP01…MAP40** | WAD imbriqué, en-tête `IWAD`, **14 lumps** non compressés |
| 1659–1662 | DEMO1LMP…DEMO4LMP | 16 000 o chacun, **petit-boutiste** (la ROM est gros-boutiste) |
| 1663–1665 | MAPINFO, ANIMDEFS, SKYDEFS | texte brut, CRLF |
| 1666–1667 | CHECKSUM (16 o, MD5), ENDOFWAD | |

Les 14 lumps d'une carte : `MAP01`, THINGS, LINEDEFS, SIDEDEFS, VERTEXES, SEGS,
SSECTORS, NODES, SECTORS, REJECT, BLOCKMAP, **LEAFS, LIGHTS, MACROS**.

#### Le blocage, et il était bien là où la passation le disait

Les sidedefs ne stockent pas un index de texture mais un **hachage 16 bits du
nom**, et `P_InitTextureHashTable` avait la ligne qui remplit la table
**commentée** depuis toujours.

**Vérifié sur le fichier** : les **4 902 références** de MAP01 résolvent toutes
avec `wad::LumpHash`, et les 503 textures donnent **502 hachages distincts** — la
seule collision étant les deux textures nommées `?`.

> *Piège de méthode rencontré en le vérifiant.* Mon premier script disait « 0
> référence résolue », ce qui aurait envoyé chercher une autre fonction de
> hachage. C'était une `Hashtable` PowerShell interrogée avec `[uint32]` quand
> ses clés étaient des `[int]` : deux types, jamais d'égalité. **Un outil de
> mesure se vérifie avant la chose mesurée.**

Les deux tables sont gardées **séparées** : un hachage est un nombre sur 16 bits
et un index un petit nombre, donc les mélanger laisserait un hachage tomber sur
un index et rendre silencieusement la mauvaise texture. Un seul IWAD étant
chargé, il n'y a rien à gagner à les mêler.

#### Les quatre changements

| | |
|---|---|
| `wad.cc` | essaie `doom64.rom` puis `doom64.wad` ; `wad::iwad_kind()` dit lequel |
| `doom_wad.cc` | `DM_START`/`DM_END` → section sons (le lecteur ROM y range déjà ses MUSAMB) ; la plage entre `T_END` et `DS_START` est revendiquée pour les **graphismes**, faute de `G_START` dans le fichier |
| `map.cc` | la table de hachage est remplie, et selon l'IWAD |
| `i_audio.cc` | le message audio dit ce qui se passe réellement |

**Le type est déduit du contenu, pas du nom** : le dialogue de sélection copie ce
que l'utilisateur choisit vers `doom64.rom`, donc un WAD choisi là arrive sous le
nom de la cartouche. `probe_iwad_` lit les quatre premiers octets.

#### Mesuré

| | |
|---|---|
| MAP01 depuis le WAD contre depuis la ROM | **moyenne 0,43 par canal, maximum 5 sur 255** |
| cartes chargées (1, 5, 12, 20, 28, 32) | toutes, aucune erreur GL, aucun avertissement |
| tableau de textures | 1 813 images, 9 couches, **0 sautée** (1 856 pour la ROM) |
| écran légal, écran-titre, logo, menu, curseur | corrects — ils viennent de la plage hors section |

L'écart de 5 sur 255 est **l'expansion de palette déjà documentée** : la ROM
réplique les bits (`(x<<3)+(x>>2)`, 31 → 255), le WAD décale seulement
(`x<<3`, 31 → 248). Environ 3 %, dans le sens attendu. Le WAD rend donc
essentiellement la même image que la cartouche.

#### Ce qui ne marche pas encore, et il faut le dire

**Les bruitages.** Les 93 sons du WAD sont du **WAV**, là où la cartouche stocke
des séquences N64 jouées par le synthétiseur. Les jouer demande un mélangeur PCM
que le moteur n'a pas — c'est un sous-système à part, pas une ligne à ajouter.
Le moteur dégrade proprement et le dit maintenant en toutes lettres.

**En revanche les 24 musiques MIDI du WAD se chargent bien** : il ne leur manque
qu'une banque d'instruments (`s_SoundFont`, ou `doomsnd.sf2` à côté du jeu),
puisque sans ROM il n'y a pas de soundfont. C'était caché derrière le message
« Failed to load 93 MIDI tracks », qui comptait les bruitages.

**Les sept niveaux perdus (MAP34–MAP40) ne sont pas atteignables.** Ils sont bien
dans le fichier, mais notre `mapinfo.txt` du pk3 — 33 cartes — **masque le
`MAPINFO` du WAD**, qui en déclare 40 : les deux se résolvent au même nom de lump
et le pk3 est chargé en dernier. `-warp 35` retombe donc sur MAP01.

Y accéder demande de trancher : laisser le `MAPINFO` de l'IWAD gagner quand c'est
le WAD du remaster (40 cartes, mais les noms y sont des clés de localisation
`$map_name_01` que nous ne savons pas résoudre), ou étendre notre `mapinfo.txt`
des sept entrées avec leurs vrais noms — Plant Ops, Evil Sacrifice, Cold Grounds,
Wretched Vats, Thy Glory, Final Judgement, Panic — et le cluster 7.

**Les démos du WAD** sont petit-boutistes là où celles de la ROM sont
gros-boutistes. Sans objet tant que le lecteur de démos n'existe pas.

### ✅ Le clavier non américain — 8 septembre 2026

Dylan, clavier AZERTY : **impossible de taper `_` dans la console**, donc
impossible de saisir la moindre cvar, puisqu'elles en contiennent toutes une.

#### La cause

`CON_ParseKey` traduisait les majuscules avec **`shiftxform`, une table QWERTY
américaine codée en dur** (`m_shift.cc`, héritée de 1999). Une table de ce genre
ne peut pas être juste ailleurs qu'aux États-Unis : sur un AZERTY, `Maj+8` donne
`8`, et la table le transforme en `*`.

Quatre choses décident du caractère qu'une touche produit — la disposition,
l'état de Maj, AltGr et les touches mortes — et **seule la plateforme les connaît
toutes les quatre**. Le moteur essayait d'en deviner deux.

#### Le correctif

`SDL_TEXTINPUT`, que SDL émet juste après le `SDL_KEYDOWN` correspondant, avec le
caractère réellement produit.

| | |
|---|---|
| `d_event.h` | nouveau type `ev_text`, en fin d'énumération — tous les responders testent un type précis ou ont des `case` explicites, donc un type de plus est ignoré partout ailleurs |
| `video.cc` | `SDL_StartTextInput()` à la création de la fenêtre, et `SDL_TEXTINPUT` posté en `ev_text`, un événement par octet ASCII imprimable |
| `con_console.cc` | `ev_text` alimente seul la ligne de saisie ; la branche clavier ne garde que le retour arrière |

**Un caractère et une touche ne sont pas la même chose**, et c'est le fond de
l'affaire : les liaisons de touches et le jeu continuent de lire `ev_keydown`,
qui leur donne une **position** — c'est ce qu'ils veulent, et c'est pourquoi
`bind W "+forward"` doit rester la touche `W` du QWERTY quelle que soit la
disposition. Seuls les champs de texte veulent un caractère.

**Le piège évité :** ne surtout pas appliquer `shiftxform` au caractère venu de
`ev_text`. Il est déjà juste, et la table le casserait précisément sur les
claviers qu'on cherche à servir. La transformation est retirée de `CON_ParseKey`,
et `shiftdown` avec elle, devenu mort.

#### Les trois champs de texte, et la table supprimée

Le même traitement a été appliqué aux deux autres, à la demande de Dylan :

| champ | ce qu'il faisait | maintenant |
|---|---|---|
| console | `shiftxform`, table QWERTY | `ev_text` |
| nom de sauvegarde / joueur (`m_menu.cc`) | `toupper(ch)` quand Maj | `ev_text`, via `M_InputChar` |
| ligne de chat (`st_stuff.cc`) | `shiftxform` | `ev_text` |

Le menu ne se servait pas de la table mais de `toupper`, ce qui est juste pour
les lettres et faux pour tout le reste : sur un AZERTY les **chiffres** sont les
caractères shiftés, donc une sauvegarde ne pouvait pas s'appeler « MAP 01 ».

**`m_shift.cc` et `m_shift.h` sont supprimés**, avec l'appel à
`M_InitShiftXForm()` et les trois `#include`. Plus personne ne lisait la table ;
la laisser aurait été pire que l'avoir, parce qu'on l'aurait crue vivante.

**Un piège propre au chat :** la touche qui *ouvre* la ligne (`t`) produit aussi
un caractère, qui serait devenu la première lettre du message. `st_eattext`
avale ce caractère-là et lui seul. La console n'a pas ce problème — elle s'ouvre
avec la touche `²`/backquote, traitée avant.

#### Vérification

**Validé en jeu par Dylan** pour la console : `>&"(-_)=_` saisi correctement, et
les deux commandes `r_depthbuffer 0` / `r_depthbuffer 1` exécutées. Le menu et le
chat compilent, démarrent et ne produisent aucune erreur GL, mais **je ne peux
pas taper dans la fenêtre du jeu** — leur validation lui revient aussi.

*Repli, s'il en fallait un : `bind F6 "r_DepthBuffer 1"` écrit dans `config.cfg`
au bloc-notes ne demande aucune saisie en jeu.*

### ⚠️ Mesurer par capture : deux conditions, apprises à la dure le 8 septembre 2026

#### 1. `-config` — ne jamais écrire dans la configuration de Dylan

Le 8 septembre, en travaillant sur `r_DepthBuffer`, j'ai laissé
**`seta r_RenderSprites 0`** dans son `config.cfg`. Chaque `+cvar valeur` passé en
ligne de commande est écrit à la sortie ; la passation le notait déjà comme piège
de *mesure*, et je le lui ai fait subir en *jeu*. Il a passé une soirée sur un
« bug » de sprites invisibles qui était mon réglage oublié.

**L'option `-config <fichier>` existe et ne marchait pas.** `G_LoadSettings`
(`g_settings.cc:135`) lisait bien l'argument et l'affectait à `ConfigFileName`,
mais `G_GetConfigFileName()` **ignorait cette variable** et codait `"config.cfg"`
en dur — la variable était écrite et jamais lue, au chargement comme à la
sauvegarde. Corrigé.

**Désormais, tout essai automatisé passe par `-config test.cfg`.** Vérifié : le
`config.cfg` de Dylan ressort identique au md5 après une série de lancements.

> Une option qui existe et ne fait rien est pire que pas d'option : on lui fait
> confiance.

#### 2. Le jeu reçoit les entrées du bureau

Deux exécutions rigoureusement identiques de MAP01 au tic 200 ont donné **84 % de
pixels différents**, puis 99 %. Ni les monstres (`-nomonsters` n'y change rien),
ni la souris (`v_MSensitivityX/Y 0` non plus).

La cause se lit sur les captures : **munitions 46 d'un côté, 50 de l'autre**, et
le joueur dans une autre pièce. La fenêtre prend le focus au lancement et
**reçoit les frappes clavier et les clics de qui utilise la machine**. Le joueur
marchait et tirait.

**Une capture n'est comparable que si personne ne touche à l'ordinateur pendant
la série.** Les mesures du matin — géométrie identique au pixel sur quatre cartes
— avaient été prises dans le calme, et leur contrôle passait à ce moment-là ;
elles restent valides. Celles de l'après-midi ne valaient rien.

**Vérifier le contrôle à chaque série, pas une fois pour toutes :** deux
exécutions au même réglage, avant de comparer quoi que ce soit. Le déterminisme
n'est pas une propriété du moteur, c'est une propriété des conditions.

#### 3. Ce qu'il faut préférer quand le doute existe

Pour un A/B visuel, **une bascule de cvar en jeu bat toute capture** :
`r_DepthBuffer` se change dans la console sans redémarrer, depuis un point de vue
strictement immobile. Aucune reproduction de position, aucune interpolation,
aucun tic à faire coïncider. C'est la mesure la plus propre disponible, et c'est
Dylan qui peut la faire, pas moi.

Et `-nomonsters` (`d_main.cc:867`) existe : il vide la carte de ses monstres,
utile sur MAP12 ou MAP31 où le joueur est attaqué au démarrage. Il ne règle pas
le problème d'entrées ci-dessus.

---

### ✅ La chaîne audio — 8 et 9 septembre 2026

Point de départ : FluidSynth 1.1.6 (le sous-module `fluidsynth-lite`), SDL
propriétaire du périphérique, aucun moyen de jouer un son enregistré. Le moteur
ne savait faire qu'une chose — synthétiser des séquences N64 — et c'est tout ce
que la cartouche demande.

Le `doom64.wad` du remaster demande autre chose : ses 24 musiques sont du MIDI
standard, mais ses **93 effets sonores sont des WAV**, et un synthétiseur n'a
aucune notion de « joue ce fichier ». D'où cette série.

État à la fin : **les deux IWAD sont intégralement audibles.**

| | cartouche | remaster |
|---|---|---|
| Instruments | ROM, 147 presets | `DOOMSND.DLS`, 54 instruments |
| Musique | séquences N64 | MIDI, menu compris |
| Effets | séquences N64 | 92 enregistrements WAV |
| Sortie | OpenAL Soft 1.25.1 | idem |

Neuf commits, du plus ancien au plus récent : `b489d9a`, `95f745f`, `d30d171`,
`f059c9c`, `cb2a5cb`, `8bcca95`, `a0eaaea`, `e3fd7fd`, `6862053`.

---

#### 1. FluidSynth 1.1.6 → 2.5.7

`ENABLE_SYSTEM_FLUIDSYNTH` passe à **ON par défaut**. Le `CMakeLists.txt` racine
essaie d'abord le paquet de configuration livré par FluidSynth 2.x
(`find_package(FluidSynth CONFIG)`) et garde l'ancien `FindFluidSynth.cmake` en
repli. Une 1.x est refusée là, avec un message :

```cmake
if (FluidSynth_VERSION VERSION_LESS 2.0)
  message(FATAL_ERROR "FluidSynth ... is too old: the cartridge soundfont
    loader is written against the 2.x API, and DLS support needs 2.1 or
    later (2.5 for it without libinstpatch).")
```

Sans ce garde-fou, une 1.x produit un écran d'erreurs sur des types opaques et un
`fluid_ramsfont` manquant, qui ne disent pas ce qui ne va pas.

Le sous-module `fluidsynth/` est **conservé et non supprimé**, pour que l'histoire
de ce qu'il faisait reste à un checkout de distance.

**`src/engine/sound/rom_source.cc` a été porté sur l'API 2.x.** `fluid_sample_t`
est devenu opaque en 2.0 : une structure `RomSample` locale porte désormais les
champs (start, end, boucle, pitch) et le handle FluidSynth à côté. Les cinq
callbacks d'un preset (`preset_get_name`, `preset_get_banknum`, `preset_get_num`,
`preset_noteon`) sont des fonctions libres, et les presets sont construits une
fois par `new_fluid_preset`.

> Un bug latent trouvé au passage : la version 1.x lisait le `Preset` **dans le
> pointeur `fluid_preset_t` lui-même** au lieu de son champ `data` — soit une
> `std::string` lue depuis le mauvais objet. Personne ne s'en était aperçu parce
> que seule une ligne de journal la demandait.

#### 2. Le témoin de la cartouche — `-romsfdump`

Changer de version de synthétiseur sans témoin, c'est espérer. `rom_sfont_dump()`
écrit un fichier texte qui décrit **ce que la cartouche a réellement remis au
synthétiseur** : chaque sample (fréquence, bornes, boucle, pitch, et une empreinte
FNV de son PCM décodé), chaque preset avec ses instruments, et une sonde de
synthèse qui joue chaque preset et relève son pic.

Le témoin pris sous 1.1.6 et celui pris sous 2.5.7 sont **identiques**, et
**147 presets sur 147 produisent du son**. C'est ce qui a permis d'avancer
ensuite sans se demander à chaque étape si la cartouche avait bougé — le témoin
a été rejoué après *chaque* commit de cette série, et il n'a jamais bougé.

`-romsfdump <fichier>` s'utilise en ligne de commande et n'a besoin que de la ROM.

#### 3. `DOOMSND.DLS`

Cherché **après la cartouche et avant `doomsnd.sf2`** dans `I_InitSequencer` :
qui possède la ROM joue l'audio de la N64 et ne veut rien d'autre ; qui n'a que
`doom64.wad` a 24 pistes MIDI et aucun instrument pour les jouer.

C'est une collection RIFF DLS, pas un SoundFont, donc le chargeur SF2 la refuse
en premier et le dit. **La ligne `fluidsynth: error: Not a SoundFont file` au
démarrage est normale** — FluidSynth passe ensuite le fichier à son lecteur DLS,
qui l'accepte. Le DLS natif demande FluidSynth 2.1 au minimum, et 2.5 pour s'en
passer de libinstpatch.

#### 4. Deux bugs trouvés en chemin

**Le slot des chansons (`b489d9a`).** `Seq_RegisterSongs` remplissait
`seq->songs[]` avec un compteur des lumps *trouvés*, pas avec la position du nom
dans la table. Avec la cartouche les 117 noms existent et rien ne se voyait. Avec
le WAD du remaster, dont les effets s'appellent `SFX_033`..`SFX_0124`, **les 93
recherches par nom échouaient** et les 24 musiques atterrissaient dans les slots
0 à 23. Le jeu demandait une musique à l'indice 93 et obtenait le silence, un
effet à l'indice 0 et obtenait de la musique.

Corrigé par `size_t slot = i++;` et un repli positionnel
(`wad::open(wad::Section::sounds, slot)`).

**Le plantage sur les WAV (`95f745f`) — celui-là était de moi.** Le repli
positionnel a rendu les lumps WAV trouvables, et :

```c
dmemcpy(song, song->data, 0x0e);          // AVANT le test MThd
if (dstrncmp(song->header, "MThd", 4)) { ... }
```

écrase les champs de la structure avec les quatorze premiers octets du fichier.
Pour un `RIFF....WAVE`, `ntracks` récupérait `'V','E'` — soit **17750** — et
`tracks` restait nul parce que l'allocation était sautée avec le reste. Le
premier son joué parcourait 17750 entrées d'un tableau nul.

Corrigé par un `invalidate()` explicite sur tout fichier refusé, plus des gardes
`song->tracks &&` dans `I_StartSound` et `I_StartMusic`.

> Une structure à moitié écrasée est pire qu'une structure vide : elle a l'air
> valide.

#### 5. OpenAL Soft 1.25.1 (`d30d171`)

LGPL-2.0-or-later, donc lié dynamiquement — ce qu'expédier une DLL veut dire de
toute façon — et compatible avec le GPL v2+ du moteur.

`src/engine/sound/oal.{hh,cc}` : ouverture du périphérique par défaut, contexte
courant, et sonde des extensions. Le nom du périphérique est lu via
`ALC_ALL_DEVICES_SPECIFIER` (le vieux `ALC_DEVICE_SPECIFIER` ne nomme que le
pilote). **EFX est présent** sur la machine de Dylan — c'est sur lui que se
construiront la réverbération par secteur et le filtre passe-bas de KEX.

Un échec n'est pas fatal et n'est pas censé l'être : une machine sans périphérique
audio doit encore lancer le jeu.

#### 6. La sortie du synthétiseur passe à OpenAL (`f059c9c`)

Deux bibliothèques ne peuvent pas se partager un périphérique de sortie. Le
synthétiseur déménage donc en premier, et les échantillons le suivront.

Rien de la synthèse ne change : FluidSynth rend toujours du stéréo 16 bits
entrelacé à 44100 dans un tampon qu'on lui tend. Ce qui change, c'est qui porte
ce tampon jusqu'aux enceintes. Un petit thread fait tourner **huit tampons de
256 trames** — 46 ms en réserve, un bloc rendu toutes les 6 ms — en désenfilant
ce qui a été joué, le remplissant, le réenfilant. Il relance aussi la source
quand elle s'est vidée, ce qu'OpenAL ne fait pas de lui-même.

`synth.sample-rate` est maintenant **écrit explicitement** à 44100 au lieu d'être
laissé au défaut : il doit s'accorder avec le taux que déclare le flux, et un
désaccord silencieux jouerait tout le jeu au mauvais pitch.

`Seq_Shutdown` arrête le flux là où il fermait le périphérique SDL, et pour la
même raison : le thread appelle `fluid_synth_write_s16` sur le synth que
`delete_fluid_synth` s'apprête à libérer.

Tout le code audio SDL mort est parti (`Audio_Play`, `Audio_Play_float`, les
opérateurs de `SDL_AudioSpec`, le bloc `SDL_OpenAudio` et son repli en float).
**Plus une seule référence audio à SDL dans le moteur.**

#### 7. Les 92 effets WAV du remaster (`cb2a5cb`)

`src/engine/sound/sfx.{hh,cc}`. Trente-deux voix OpenAL, un lecteur RIFF/WAVE qui
parcourt les chunks au lieu de supposer le préambule habituel de 44 octets, et
une règle simple : `I_StartSound` demande d'abord un enregistrement et retombe
sur le séquenceur s'il n'y en a pas.

**Propriété de sûreté qui rend l'affaire sans risque pour la cartouche :** aucun
lump de la ROM n'est un RIFF. Le chemin échantillonné reste donc inerte avec
`doom64.rom` sans un seul test sur l'IWAD — vérifié, 0 enregistrement inscrit.

**Ce qui n'est délibérément pas fait, c'est le positionnement.** OpenAL sait
placer un son dans le monde et l'atténuer par la distance, et ça ne ressemblerait
pas à DOOM 64. `S_AdjustSoundParams` calcule déjà un volume et une séparation
comme le jeu l'a toujours fait ; la source est relative à l'auditeur avec son
rolloff à zéro, si bien que sa position ne fait que déplacer le son entre les
enceintes. La largeur stéréo reproduit exactement les trois quarts que le pan
MIDI atteignait — `(pan - 128) / 128`, jamais l'extrême gauche ou droite.

**L'espace de canaux s'élargit au lieu de se dédoubler :** `I_GetMaxChannels`
compte les 64 du séquenceur **plus** les 32 voix enregistrées, et les trois
accesseurs que `s_sound.cc` parcourt distinguent les deux bancs par l'indice.
`s_sound.cc` lui-même n'a pas changé d'une ligne.

**Deux sons bouclent** — le bourdonnement du fusil plasma et le tremblement de
terre — et les fichiers ne peuvent pas le dire : **aucun des 93 ne porte de chunk
`smpl`**, donc aucun point de boucle à lire. Le moteur le dit à leur place, à
partir du fait que ce sont les deux que le jeu démarre une fois et arrête à la
main.

92 et pas 93 : `NOSOUND` a un chunk `data` de longueur nulle et est refusé. C'est
le silence, c'est correct.

#### 8. 🐛 L'ordre des sons — mon erreur, et comment la table a été dérivée (`8bcca95`)

**Ce que j'avais supposé sans le vérifier :** que le k-ième enregistrement du WAD
était le k-ième son de `sounds.h`. Les noms rendaient l'hypothèse séduisante —
`SFX_033` à `SFX_0124`, exactement 92, exactement 32 de plus que l'énum 1–92.
**C'est faux.** Ces numéros ne sont qu'un compteur.

Dylan a entendu le résultat immédiatement : une porte avec le râle d'agonie d'un
monstre, le pistolet sur un téléporteur.

Ce qui a été vérifié avant de conclure, dans l'ordre :

1. **Le pk3 ne contient aucun son** → la section `sounds` vient du seul WAD,
   indices 0 à 116, aucun décalage.
2. **`section_index()` mesuré dans le moteur** (`-sfxdump`) → slot 1 = `SFX_033`,
   contigu jusqu'à 92. L'hypothèse d'indexation était bonne.
3. **Donc c'est l'ordre lui-même.**

La table a alors été tirée **de la cartouche**, qui contient les mêmes 92 sons et
sait ce qu'ils sont :

- Chaque séquence d'effet porte un *program change* désignant un patch, **et ce
  numéro n'est pas le sien**. La séquence 1 (le coup de poing) demande le patch
  56 ; la séquence 5 (le pistolet) demande le patch 1. C'est là que tout se
  jouait. `rom_sfont_dump` imprime désormais ce lien (`seq N prog M`) pour que la
  dérivation puisse être **refaite** plutôt que crue sur parole.
- Chaque patch désigne un sample.
- Chaque enregistrement du remaster s'est révélé être **un de ces samples avec
  chaque valeur 16 bits dupliquée** : du 22050 Hz qui transporte 11025 Hz de
  contenu, quatre octets là où la cartouche a un échantillon.

Vérification finale : décimer chaque enregistrement et lui appliquer **la même
empreinte FNV que le témoin** applique au PCM de la cartouche. **92 sur 92
identiques, octet pour octet.** Pas par longueur — la correspondance par longueur
ne donnait que 13 appariements nets sur 92, avec des collisions — mais par
contenu. Le résultat est une permutation propre de 1 à 92, aucun slot inutilisé,
aucun en double.

Une seule paire restait indécidable et ne peut pas compter : la cartouche joue
`sfx_oof` et `sfx_noway` depuis le même sample, donc le remaster porte ce son
deux fois et les deux copies sont identiques.

**Le code place désormais un enregistrement par son nom, jamais par sa position.**
Un nom qui n'est pas de ceux du remaster passe par `Seq_SoundLookup`, donc un
PWAD qui remplace `SNDPUNCH` par un WAV atterrit au bon endroit. `-sfxdump`
imprime ce que chacun a résolu, plus son pic.

Contrôle de lisibilité du résultat : `SFX_033 → 5` pistolet, `034 → 6` fusil,
`035 → 7` plasma, `036 → 8` BFG. Le WAD commence par les armes dans l'ordre des
armes.

> Une hypothèse qui explique élégamment les chiffres n'est pas une mesure.
> « 33 = 1 + 32 » et « 124 = 92 + 32 » sont vrais et ne prouvaient rien.

#### 9. La musique du menu — un méta-événement non consommé (`a0eaaea`)

**Symptôme :** avec `doom64.wad` + `DOOMSND.DLS`, aucune musique au menu — et
lancer le menu **coupait la musique du titlemap qui jouait déjà**.

Le chemin de mesure, parce qu'il est réutilisable :

| Question | Moyen | Réponse |
|---|---|---|
| Le lump est-il trouvé et accepté ? | `-musdump` | oui, `MUSTITLE` 6 pistes |
| Le jeu le demande-t-il ? | `-musdump` | oui, 6 canaux démarrés |
| Le synthétiseur produit-il du son ? | mètre de crête temporaire | **pic 1, 0 voix** |
| Le DLS est-il en cause ? | même mesure sur MAP01 | non, 20000–32294, 1–4 voix |
| La cartouche est-elle en cause ? | même mesure sur la ROM | non, 1345–2669, 1 voix |

Donc : `MUSTITLE` du WAD, en particulier. Sa **piste 0 ne contient aucune note.**
Elle contient ce qu'un logiciel de MAO laisse traîner :

```
ff 7f 0f   05 0f 1c "2019.03.01" ...          longueur 15
ff 7f 2c   05 0f 2d "Microsoft Sans Serif,8.25,False,False,1,0"   longueur 44
ff 7f 34   05 0f 12 ... "Speakers (Realtek High Definition Audio)"  longueur 52
ff 58 04   04 02 18 08                         signature rythmique
ff 51 03   04 c4 b4                            tempo
ff 2f 00                                       fin
```

**Les deux gestionnaires concernés lisaient par le contenu, pas par la longueur.**
`MIDI_SEQUENCER` lisait la longueur, lisait l'octet fabricant, constatait que ce
n'était pas le `0x00` de la cartouche et s'arrêtait — treize octets restaient dans
le flux. Le `default:` ne lisait **rien du tout**, pas même la longueur, ce qui
condamnait aussi le `0x58`.

Ce n'est pas une panne discrète limitée à une piste : des octets aléatoires lus
comme changements de contrôleur arrivent au synthétiseur comme les autres, et
parmi les contrôleurs que le hasard finit par nommer il y a ceux qui coupent un
canal. **D'où la musique du titlemap coupée.**

Les deux calculent maintenant la fin de l'événement depuis sa longueur déclarée
et s'y placent, quoi qu'ils aient compris du contenu. Les marqueurs de boucle de
la cartouche déclarent exactement les longueurs que le code consomme — comportement
inchangé, vérifié.

Corrigé au passage : `MIDI_MESSAGE` écrivait un octet au-delà de son tampon de
256 quand l'octet de longueur valait 255. Sans conséquence jusqu'ici, mais le
moteur lit maintenant des MIDI qui ne viennent plus de la cartouche.

**Mesure avant / après :**

| | avant | après |
|---|---|---|
| `MUSTITLE`, WAD + DLS | pic **1**, **0 voix** | 7000–19000, voix actives |
| `MUSTITLE`, cartouche | 1345–2669, 1 voix | 1345–2669, 1 voix |

> C'est la troisième fois dans ce projet qu'un défaut vient de données lues avec
> confiance : le champ de taille du lecteur ZIP, l'en-tête écrasé avant son test,
> et ici une longueur déclarée mais jamais respectée.

#### 10. Le volume (`e3fd7fd` puis `6862053`)

Premier essai : les enregistrements passaient par la courbe au carré que le
General MIDI définit pour le contrôleur 7 — celui qu'envoie le séquenceur — en
raisonnant qu'un enregistrement recevant la valeur linéaire brute se retrouverait
au-dessus de tout ce que joue le synthétiseur.

**La mesure a tranché dans l'autre sens.** Les 92 fichiers sont masterisés sans
aucune marge :

| | |
|---|---|
| Pic médian | 30968 sur 32767 |
| À pleine échelle | 33 des 92 |
| Le plus discret | 10393 |

La courbe ne les retenait pas au niveau de la musique, elle les maintenait 5 dB
en dessous — et la musique atteint aussi la pleine échelle.

Deux verdicts de Dylan ont borné la réponse : **0,53 nettement trop bas, 1,00 un
peu trop fort.** La constante `SFX_HEADROOM` vaut **0,8** (environ −2 dB), posée
entre ces deux bornes. Elle est nommée et seule sur sa ligne, pour que le
prochain qui n'est pas d'accord avec mes oreilles n'ait qu'un chiffre à changer.

Le gain est sinon exactement ce que le jeu demande : le volume calculé par
`S_AdjustSoundParams` selon la distance, multiplié par le curseur (`s_SfxVol`,
0 à 100). Aucune courbe empruntée au MIDI.

Validé par Dylan le 9 septembre 2026 : « correct pour l'instant ».

#### 11. Ce que contiennent `DOOMSND.SF2` et `DOOMSND.DLS`

Analyse RIFF des deux fichiers, 9 septembre 2026.

**`DOOMSND.SF2`** (5 429 302 octets, celui extrait de la ROM par doom64ex 2.5) —
c'est la cartouche, chiffre pour chiffre :

| | SF2 | notre lecteur de ROM |
|---|---|---|
| Samples | 124, tous 22050 Hz | 124 |
| Presets | 147 (bank 0 : **54**, bank 1 : **93**) | 147, même répartition |
| PCM | 2 693 984 échantillons | 2 693 984 |

**Confirmation indépendante que `rom_source.cc` lit la cartouche correctement.**
Ses samples s'appellent `SFX_000`..`SFX_123`, ce qui explique au passage d'où
vient la forme des noms du WAD du remaster.

**`DOOMSND.DLS`** (3 230 114 octets) — beaucoup plus petit, et on voit pourquoi :

- **54 instruments, bank 0, programmes 0 à 53** — exactement la bank 0 du SF2
- **aucun kit de percussion**, et **pas de bank 1**
- 33 ondes, mono 22050 Hz 16 bits
- produit avec **Awave Studio v10.6**

Autrement dit Nightdive a converti **uniquement les instruments de musique** et
laissé tomber la bank 1, les 93 effets, puisqu'ils les livrent en WAV.
**Le DLS et les WAV sont les deux moitiés du SF2.**

C'est aussi ce qui explique l'avertissement `No preset found on channel 9
[bank=128 prog=0]` au démarrage : FluidSynth place le canal 9 en percussion par
défaut, et le DLS n'a pas de kit. Aucune musique de Doom 64 n'utilise ce canal,
donc c'est sans effet — mais il ne faut pas le prendre pour la cause d'un silence.

#### 12. Comment UZDoom gère FluidSynth et OpenAL — analyse du 9 septembre 2026

Utile parce qu'il résout les mêmes problèmes, et parce que deux de ses fichiers
sont réutilisables sous notre licence.

**Les couches.** FluidSynth n'apparaît **jamais** dans l'arbre principal
d'UZDoom : il est enfermé dans la bibliothèque **ZMusic**
(`libraries/ZMusic/source/mididevices/`) comme l'un des ~14 périphériques MIDI
interchangeables. Le jeu ne sait pas lequel joue. OpenAL, lui, est le moteur de
rendu du son (`src/common/audio/sound/oalsound.cpp`, 2210 lignes).

**La différence qui compte.** `FluidSynthMIDIDevice` ne fait qu'une chose de
synthèse (`ComputeOutput` → `fluid_synth_write_float`). Le tempo, les ticks et la
distribution des événements sont dans la classe de base `SoftSynthMIDIDevice`, et
son `ServiceStream` **fait avancer le séquenceur à l'intérieur du callback
audio** : rendre jusqu'au prochain tick, jouer le tick, recommencer.
**L'horloge MIDI *est* l'horloge d'échantillonnage** — aucune dérive possible.

Notre moteur fait l'inverse : un thread SDL séparé (`Thread_PlayerHandler`)
interroge `SDL_GetTicks()` pendant que le thread audio tire les échantillons de
son côté. **Deux horloges.** Le commentaire de villsa dans `I_InitSequencer`
l'admet lui-même (« off-sync when uncapped framerates are enabled »).

**Les tailles de tampon, et pourquoi les nôtres sont différentes :**

| | UZDoom | notre moteur |
|---|---|---|
| Tampons | 4 | 8 |
| Par tampon | 11025 trames (250 ms) | 256 trames (5,8 ms) |
| **Latence** | **1 seconde** | **46 ms** |
| Réveil du thread | 100 ms (variable de condition) | 1 ms |
| Format | float32 | 16 bits |

Ils peuvent se permettre une seconde parce que **chez eux aucun effet ne passe
jamais par le synthétiseur**. Chez nous, avec la cartouche, tous les effets sont
des séquences : la latence du flux est celle entre appuyer sur la détente et
entendre le coup. C'est la justification de nos 46 ms.

**Côté OpenAL.** Chargé dynamiquement (`FModule OpenALModule{"OpenAL"}`), pas lié
à la compilation — le jeu démarre même sans la DLL. Il pré-alloue **128 sources**
(`snd_channels`) dans un pool `FreeSfx` ; quand il n'y en a plus,
`FindLowestChannel()` vole la moins prioritaire puis la plus lointaine. **Le flux
de musique prend une source dans ce même pool.**

Son 2D : `AL_SOURCE_RELATIVE TRUE`, position nulle, `AL_ROLLOFF_FACTOR 0` —
identique à ce que nous faisons.

Son 3D : ils calculent **leur propre** courbe d'atténuation DOOM (`GetRolloff`)
puis règlent `AL_REFERENCE_DISTANCE = gain × dist` pour qu'OpenAL, avec son modèle
inverse-distance, **reproduise** cette courbe. C'est la piste à suivre si on veut
un jour le placement 3D sans perdre l'atténuation d'origine.

EFX : un seul `AuxiliaryEffectSlot` en `AL_EFFECT_EAXREVERB` (repli
`AL_EFFECT_REVERB`), deux filtres `AL_FILTER_LOWPASS`, appliqués par source via
`AL_DIRECT_FILTER` et `AL_AUXILIARY_SEND_FILTER`. Sous l'eau :
`AL_LOWPASS_GAINHF` à 0,125 plus un décalage de hauteur.

**Aucune prise en charge du DLS nulle part** dans UZDoom ni dans ZMusic. Sur ce
point précis, notre moteur fait quelque chose qu'ils ne font pas.

**Licences — à relire avant toute copie**, c'est plus nuancé que noté jusqu'ici :

- `oalsound.cpp`, `s_environment.cpp` : **double licence**. GPL-3.0-or-later,
  **et** BSD-3-Clause **pour le code écrit avant 2026 seulement**. Ce qui a été
  ajouté en 2026 et après est GPL-3 uniquement, donc inutilisable chez nous.
- ZMusic `mididevices/` et `streamsources/` : **BSD-3-Clause pur** (Marisa Heit),
  19 fichiers sur 20 — dont `music_fluidsynth_mididevice.cpp` et
  `music_softsynth_mididevice.cpp`. Seul `driver_adlib.cpp` est GPL.

Les deux fichiers qui portent l'idée la plus intéressante — le séquenceur cadencé
à l'échantillon — sont donc en BSD-3 pur et intégralement utilisables.

#### 13. Les outils de diagnostic conservés

| Option | Ce qu'elle sépare |
|---|---|
| `-romsfdump <f>` | ce que la cartouche a remis au synthétiseur : samples, presets, empreintes PCM, sonde de synthèse, et le `seq N prog M` de chaque séquence |
| `-sfxdump` | quel enregistrement a résolu vers quel son, sa taille, sa fréquence et son pic |
| `-musdump` | ce que les 24 slots de musique contiennent, et ce que le jeu demande ensuite avec combien de canaux démarrés |

Les trois existent parce qu'une musique muette ou un son faux ne dit rien sur
*lequel* des trois endroits a échoué : le lump introuvable, le fichier refusé, ou
le synthétiseur silencieux. Les séparer est ce qui a permis de trouver le bug du
méta-événement en une session.

#### 14. Ce qui reste ouvert côté audio

- **La réverbération EFX** — `snd_hardwarereverb` et `snd_lowpassfilter` de KEX.
  L'extension est présente, et `I_StartSound` reçoit déjà `MS_REVERB` /
  `MS_REVERBHEAVY` (16 ou 32) sans rien en faire pour les enregistrements.
- **La dérive de l'horloge MIDI** — voir §12. Deux fichiers BSD-3 montrent la
  solution.
- **La courbe d'atténuation de la cartouche.** L'écart de niveau entre ses effets
  (pics 7 à 4973, environ 700×) est **inhérent aux données d'atténuation de la
  cartouche**, identique sous 1.1.6 et 2.5.7 — ce n'est pas un défaut du moteur.
  `s_attenuation_to_percent` étale l'octet 0–127 sur 0–90 dB. Les fichiers
  `wess*` de DOOM64-RE diraient ce que fait vraiment la N64.
- **Les sept niveaux perdus (MAP34–40)** : présents dans le `doom64.wad`, mais
  le `mapinfo.txt` du pk3, qui n'en déclare que 33, masque le `MAPINFO` du WAD
  qui en déclare 40. Ce n'est pas de l'audio, mais c'est le dernier gros morceau
  du support du remaster.

---

## 9. Manière de travailler avec Dylan

- Il préfère les **instructions manuelles pas à pas** plutôt que des
  remplacements de fichiers entiers, et l'**instanciation structurée** plutôt
  que des littéraux hexadécimaux bruts.
- Il se décrit comme ayant des connaissances limitées, mais il a débogué un
  lecteur ZIP en remontant d'un menu défaillant jusqu'à un champ de taille lu au
  mauvais offset. Le traiter comme quelqu'un qui sait ce qu'il fait, en
  expliquant le pourquoi de chaque étape.
- Un changement à la fois, avec vérification entre chaque. Cette méthode a
  fonctionné.
- Il tend à vouloir refonder les bases avant de construire. La base est arrêtée
  et validée : **ne pas rouvrir le débat, avancer sur le rendu.**

---

## 10. Autres projets

- **DOOM64 EX+ Enhanced** — son fork de la lignée EX+ (Gibbon), avec éclairage
  dynamique et manette SDL3. Mis de côté, pas abandonné.
- **Doom Builder 64 II** — son éditeur de maps Doom 64.
