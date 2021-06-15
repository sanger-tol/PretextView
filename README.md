![GitHub all releases](https://img.shields.io/github/downloads/wtsi-hpag/PretextView/total?style=plastic)
# PretextView
Paired REad TEXTure Viewer. OpenGL Powered Pretext Contact Map Viewer.<br/>

PretextView is a desktop application for viewing pretext contact maps.<br/>

# Usage
Use the middle mouse button or the 'u' key to bring up the GUI. Pan by holding down the right mouse button. Click and drag with the left mouse button to select an are to zoom to. Scroll to zoom. A three button mouse is recomended.<br/>
Version 0.04 or later: Edit mode and Waypoint mode<br/>
Activate edit mode with the GUI, or use the 'e' key. Pickup a region of a contig with the left mouse button, pickup a whole contig with the middle mouse button or spacebar. Place a region with the left mouse button. Invert a selected region with the middle mouse button or spacebar. Undo the last edit with the 'q' key. Exit edit mode with the 'e' key. Use the GUI to see a list of completed edits.<br/>
Activate waypoint mode with the GUI, or use the 'w' key. Place a waypoint with the left mouse button, delete a waypoint with the middle mouse button or spacebar. Exit waypoint mode with the 'w' key. Use the GUI to see a list of waypoints, click on a waypoint in the list to vist it.<br/>
Enter scaffolding mode with the 's' key.

# Saving
Map state is automatically saved (`$XDG_CONFIG_DIR` or `~/.config` on Unix, and the `%APPDATA%` folder on Windows) while the app runs, and is loaded on map load.<br/>
You can also manually save/load state via the UI.

# AGP Output
Map state can be output in [AGP](https://www.ncbi.nlm.nih.gov/assembly/agp/AGP_Specification/) format via the UI. Objects are first created according to the scaffolds defined in scaffolding mode, with remaining sequences output as singletons.<br/>
## AGP Correction
Note that object/part sizes will only be accurate up to the size of an individual map texel, and that any input sequences smaller than an individual texel will not be output.<br/>
AGP files can be corrected by the included python script `AGPCorrect`, which requires access to the input sequences in (gzipped) FASTA format.
```bash
AGPCorrect ref.fa(.gz) map.agp >corrected_scaffs.agp
```
The script requires
* Python >= 3.8
* [Biopython](https://biopython.org/)

# Requirments, running
OpenGL 3.3<br/>
2G of RAM<br/>

# Windows, Mac and Linux Builds
The prebuilt apps for Windows, Mac and Linux are available [here](https://github.com/wtsi-hpag/PretextView/releases).<br/>
The Mac app was built on MacOS 10.13.6<br/>
The Linux app was built on kernel 3.13<br/>
The Windows app was build on Windows 10, and is known to work on at least Windows 7.

# Third-Party acknowledgements
PretextView uses the following third-party libraries:
* [glfw](https://github.com/glfw/glfw)
* [libdeflate](https://github.com/ebiggers/libdeflate)
* [FontStash](https://github.com/memononen/fontstash)
* [Nuklear](https://github.com/vurtun/nuklear)
* [glad](https://github.com/Dav1dde/glad)
* [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)
* [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h)
* [stb_truetype](https://github.com/nothings/stb/blob/master/stb_truetype.h)
* [stb_sprintf](https://github.com/nothings/stb/blob/master/stb_sprintf.h)
* Fonts from the 'Droid Serif' font set
* [Icons from open-iconic](https://github.com/iconic/open-iconic)

# Installation
Requires:
* clang >= 11.0.0 [Unix]
* clang-cl >= 11.0.0 [Windows]
* meson >= 0.57.1
* cmake >= 3.0.0

```bash
git submodule update --init --recursive
./install [Unix]
install.bat [Windows]
```
Application will be installed to the 'app' folder in the source directory.
