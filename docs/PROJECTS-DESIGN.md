# FILE split in two: SAMPLES and PROJECTS (design, 2026-11-05, not built)

The owner's ask: projects are reached today through the sample browser (B1 up past AUDIO/, into PROJECTS/, then LOAD,
the pool's button). FILE becomes two modes: **SAMPLES** (the browser and the pool, as now) and **PROJECTS**. The
mockups are drawn with the firmware's kit (`host/mock_ux2.c`, `tools/mockups/modes.js`; `sh tools/mockups/make_mockups.sh`):
`results/ux-v2-mockups/Projects_*`, `ProjectName_*`, `SamplesBrowser_*`.

## OCT- no, OCT+ yes (the browsers' design language)

In SAMPLES, PROJECTS and the name keybed, **OCT+ is yes / open / OK** and **OCT- is no / back**. When a question waits
(LOAD over edits, SAVE +1, a rename over a taken name, NEW) the header asks it (LOAD COSMIC TRAM?) with the **← →
box** that already means back / OK in pop-ups: ← is OCT-, → is OCT+. The line under the card says what yes does to
the project you leave (ACID LAKE SAVED FIRST). ProjectsConfirm_screen.png / _panel.png.

## SAMPLES (the browser)

- **Its root is AUDIO/**: nothing above it is reachable (no RemoteFM1 folder, no PROJECTS/). The path line reads
  `AUDIO/BREAKS/`.
- **An UP row on top** (an arrow, UP, and where it goes on the right) whenever the folder is not AUDIO/ itself; SEL
  onto it and LOAD (or INTO) goes up. B1 UP stays as the shortcut.
- **OCT+ opens** the row under the cursor (a folder: into it; UP: up; a file: LOAD into the pool), **OCT- goes up**.
  FREE (it was F2) moves to B9.
- Everything else as now: INTO, the EDITOR / SLICE / POOL pages, LOAD into the pool, PREVIEW, the pool meter, the cells.

## PROJECTS

The screen (Projects_screen.png):
- Header: FILE/PROJECTS, the cursor's place among them (2/14).
- **NOW**: the project you work on, and EDITED (accent) while it has unsaved edits, else SAVED.
- **The list**: every `.ecp` in data/elasticat/projects/, **newest first**, its age on the right (NOW, 2H, 1D ..),
  the loaded one marked with the accent dot. SEL scrolls.
- **The card** (the lower half, where the cells are elsewhere: no knob params on this page, so no cell boxes): the
  selected project's name big, **its 64 patterns as bubbles** (written ones cream, the one it opens on in the accent),
  its tempo and how many samples it uses. Read from the file as the cursor lands (a project is ~7 KB).

The keys (Projects_panel.png), the actions on the right where LOAD sits in SAMPLES:
| Key | Does |
|---|---|
| OCT+ or B10 **LOAD** | opens the selected project; with unsaved edits it asks first (LOAD COSMIC TRAM?, OCT+ yes: the edits saved, then it opens; OCT- no) |
| OCT- | back: leaves PROJECTS (a question waiting: no) |
| B9 **SAVE** | saves the current project over its file (as SHIFT + FILE does anywhere, which stays) |
| B8 **SAVE +1** | saves the current project as a new one, its name plus the next free number (SPICY WALRUS -> SPICY WALRUS 2); the copy becomes the project you work on, the old file stays as it was |
| B1 **NEW** | a new project (the current one saved first), named by namesizer |
| B2 **RENAME** | the name keybed for the selected project (the current one or any other; the file is renamed: written under the new name, the old one deleted) |
| B3 **RANDOM** | the selected project gets a random name at once (namesizer; SHIFT + F1 undoes) |

**File names are project names**: SPICY WALRUS.ecp, not PROJ-007.ecp (old PROJ-nnn files keep their names until
renamed). Names are the keybed's set without `/` and never start with `.`; a name taken already gets the next free
number. last.txt follows renames.

## The name keybed (FIZZ's typewriter, ProjectName_screen.png)

The same keyboard as FIZZ's (PLAYING.md "How do I type a name?"), for the project's name (and later the pattern
names, which today use the knob speller):
- **B1..B11 = A..K, W1..W16 = L..Z and SPACE**; a key types its letter at the cursor; the screen draws the keybed
  with a letter on every key, the key pressed lights there; all 27 keys lit on the panel.
- **The suggested name starts selected**: OK at once keeps it, the first key typed replaces it.
- **SHIFT held**: the black keys 0..9 and -, the white keys the signs (FIZZ's set, less `/`).
- **PRESETS (or K4) clockwise rolls a new name every detent, for ever** (namesizer pairs mostly, now and then a
  phonic or cosmic word: FIZZ's names.c, a port of the owner's namesizer.lua written for Elasticat); **anticlockwise
  walks back** through the last ten names seen (the dots under the name show where), and clockwise again walks
  forward through them before it rolls new ones (owner: as in FIZZ). A name typed or edited is kept among them; a
  die beside the name when rolled.
- **ALGO** moves the cursor; **SEL / K1** turns the character under it.
- **OCT+ = OK** (FILE too); **OCT- deletes** the character before the cursor (a selected name: all of it), and on an
  empty name it is no: back with the old name kept; **SHIFT + OCT-** clears; **HOME** cancels at any time. (FIZZ's
  F1 delete is Elasticat's OCT-: the same button.)
- 16 characters (Elasticat's names), FIZZ's are 12: the roller keeps to 16.

## Open questions for the owner

1. **How FILE splits**: (a) the FILE button's cycle becomes SAMPLES > PROJECTS > EDITOR > SLICE > SAMPLER; or
   (b) FILE = SAMPLES (its cycle as now) and **SHIFT + FILE held** = PROJECTS (SHIFT + FILE tapped still saves); or
   (c) PROJECTS on the mode picker only. The mockup labels the button PROJECTS for the picture.
2. **Delete**: not asked for, not drawn. If wanted: hold B11 + a held confirm, like FIZZ's B11 + W.
3. **NEW** on B1: wanted, or leave it out?
