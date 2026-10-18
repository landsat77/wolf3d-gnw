# Wolfenstein 3D — installation

Requires Retro-Go SD **v2.0.0-rc1 or later** on the Game & Watch.

The game data is not included: you need the full version of Wolfenstein 3D
(v1.4, the 8 `*.WL6` files: `AUDIOHED`, `AUDIOT`, `GAMEMAPS`, `MAPHEAD`,
`VGADICT`, `VGAHEAD`, `VGAGRAPH`, `VSWAP`). Steam: in
`steamapps/common/Wolfenstein 3D/base`; GOG: in the game's folder.

1. Build the data pack (Python 3, nothing else to install). Keep the
   `wf_render` folder next to `wf_pack.py`: it renders the AdLib music.

       python3 wf_pack.py Wolf3D.wf /path/to/wolf3d

   On Windows:

       py wf_pack.py Wolf3D.wf "C:\Games\Wolfenstein 3D\base"

   The pack is about 13 MB. Smaller: `--no-music` (about 2.5 MB).

2. Copy to the SD card, side by side, with the same name:

       /homebrews/Wolf3D.bin
       /homebrews/Wolf3D.wf

3. Start "Wolfenstein 3D" from the Homebrew tab. The first start copies the
   pack to the external flash: it takes a minute or so.

Controls: D-pad = move / turn, A = fire (validate in the menus, "yes"),
B = open doors (back in the menus, "no"; held with left / right: strafe),
TIME = next weapon, GAME = game menu / back, PAUSE/SET = Retro-Go menu
(savestates; Options: music, sound, swap A/B, always run). A savestate saves
the level being played (in a level only).

---

# Wolfenstein 3D — installation (français)

Nécessite Retro-Go SD **v2.0.0-rc1 ou plus récent** sur la Game & Watch.

Les données du jeu ne sont pas fournies : il faut la version complète de
Wolfenstein 3D (v1.4, les 8 fichiers `*.WL6` : `AUDIOHED`, `AUDIOT`,
`GAMEMAPS`, `MAPHEAD`, `VGADICT`, `VGAHEAD`, `VGAGRAPH`, `VSWAP`). Steam :
dans `steamapps/common/Wolfenstein 3D/base` ; GOG : dans le dossier du jeu.

1. Créer le pack de données (Python 3, rien d'autre à installer). Garder le
   dossier `wf_render` à côté de `wf_pack.py` : c'est lui qui calcule la
   musique AdLib.

       python3 wf_pack.py Wolf3D.wf /chemin/vers/wolf3d

   Sous Windows :

       py wf_pack.py Wolf3D.wf "C:\Jeux\Wolfenstein 3D\base"

   Le pack fait environ 13 Mo. Plus petit : `--no-music` (environ 2,5 Mo).

2. Copier sur la carte SD, côte à côte, avec le même nom :

       /homebrews/Wolf3D.bin
       /homebrews/Wolf3D.wf

3. Lancer « Wolfenstein 3D » depuis l'onglet Homebrew. Au premier
   lancement, le pack est copié dans la flash externe : cela prend environ
   une minute.

Commandes : croix = avancer / tourner, A = tirer (valider dans les menus,
« oui »), B = ouvrir les portes (retour dans les menus, « non » ; maintenu
avec gauche / droite : pas de côté), TIME = arme suivante, GAME = menu du
jeu / retour, PAUSE/SET = menu Retro-Go (sauvegardes ; Options : musique,
son, inverser A/B, toujours courir). Une sauvegarde enregistre le niveau en
cours (seulement pendant un niveau).
