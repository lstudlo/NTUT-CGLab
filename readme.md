# OpenGL OBJ Viewer

## Overview

This is a simple 3D object viewer written in C++ using OpenGL and the FreeGLUT library. It allows users to load and view Wavefront OBJ (`.obj`) model files, interactively manipulate the object and camera, and change rendering styles.

## Features

* **OBJ Model Loading:** Loads 3D models from `.obj` files (supports vertex `v` and face `f` tags).
* **Multiple Rendering Modes:**
  * **Point Mode:** Renders only the vertices of the model.
  * **Line Mode (Wireframe):** Renders the edges of the model's faces.
  * **Face Mode (Solid):** Renders filled polygons for the model's faces (default).
* **Color Modes:**
  * **Single Color:** Renders the entire model in a default white color.
  * **Random Color:** Assigns a random color to each face (in Face mode) or vertex (in Point mode when no faces are loaded).
* **Interactive Transformations:**
  * **Object Translation:** Move the object along the X, Y, and Z axes.
  * **Object Rotation:** Rotate the object around the X, Y, and Z axes.
* **Camera Controls:** Move the camera forward/backward, left/right, and up/down.
* **Automatic Centering and Scaling:** Automatically calculates the model's bounding box and adjusts the initial camera view to fit the loaded model.
* **Coordinate Axes:** Displays reference X (Red), Y (Green), and Z (Blue) axes for orientation.
* **Smooth Movement:** Uses key state tracking for continuous transformations while keys are held down.
* **Right-Click Menu:** Provides easy access to:
  * Load predefined or custom OBJ files.
  * Switch between rendering modes.
  * Switch between color modes.
  * Reset the view (object position/rotation and camera).
  * Quit the application.
* **Command-Line Loading:** Optionally load an OBJ file specified as a command-line argument.
* **Default File Loading:** Attempts to load a default file (`Models/gourd.obj`) if no command-line argument is given.

## Dependencies

To compile and run this application, you need:

1.  **A C++ Compiler:** A modern C++ compiler (supporting C++11 or later features like `auto`, `<limits>`, etc.). Examples: GCC (g++), Clang, MSVC.
2.  **OpenGL Libraries:** Typically installed with your graphics drivers.
3.  **FreeGLUT:** The Free OpenGL Utility Toolkit library for windowing and input handling.
  * **Linux (Debian/Ubuntu):** `sudo apt-get update && sudo apt-get install build-essential freeglut3-dev`
  * **Linux (Fedora):** `sudo dnf install gcc-c++ freeglut-devel`
  * **macOS (using Homebrew):** `brew install freeglut`
  * **Windows:** Download pre-compiled binaries (check the FreeGLUT website or use a package manager like vcpkg). Ensure `freeglut.dll` is accessible at runtime, and link against `freeglut.lib`.

## Building

The specific compilation command depends on your system and compiler. A typical command using g++ on Linux or macOS might look like this:

```bash
g++ your_source_file.cpp -o obj_viewer -lGL -lglut -lm