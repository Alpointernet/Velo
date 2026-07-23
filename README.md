<p align="center">
  <img src="icon/app.png" width="128" alt="Velo">
</p>

<h1 align="center">Velo</h1>

<p align="center">
  A minimal, fast text and code editor built for Windows.
</p>

<img width="1400" height="800" alt="program" src="https://github.com/user-attachments/assets/4532800a-be64-4d41-926e-7cc5cdb84546" />

---

## Key Features

Most text editors are either too simple, or too bloated, which makes it harder to get the best experience. Velo comes great out of the box, as simple is better.

- **Lightweight:** Opens the second you click it, and consumes very little memory.
- **Functional:** Has tabs, search and replace tools.  
- **Clean Dark Aesthetic:** Every interface element is intentional.
- **Typographic Clarity:** Uses different fonts for editing and the GUI.
- **Coding Usability:** Basic coding features you would expect are here.
- **State Persistence:** Remembers what you did the last time.

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New tab |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save current file |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+W` | Close active tab |
| `Ctrl+Shift+W` | Close all tabs |
| `Ctrl+F` | Open/Close inline find |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |

## Building & Installation

### Requirements
- **Visual Studio 2022** with the C++ desktop workload.

### Compile
Run the build script in the root:
```bat
scripts\build.bat
```
This will compile the source files and resources into `Velo.exe`.

### Create Installer Package
To package Velo into a single installer executable (requires [Inno Setup 6](https://jrsoftware.org/isdl.php)):
```bat
scripts\create_setup.bat
```
The script compiles the project, generates the installer, and outputs it to the `output/` directory, which opens automatically upon completion.

## Project Structure

```
src/                → Main application source code
  main.cpp          → Entry point, Win32 window loop, keybinds
  globals.h         → Global state and settings declarations
  include/          → Scintilla header files
  components/       → Tab management, editor styling, dialogs, drawing APIs
fonts/              → Private fonts loaded at runtime
icon/               → App graphics and compiled icon assets
scripts/            → Build and packaging scripts
```

## License

Uses the [Scintilla](https://www.scintilla.org/) editing library.
