<p align="center">
  <img src="icon/app.png" width="128" alt="Velo">
</p>

<h1 align="center">Velo</h1>

<p align="center">
  A minimal, fast text and code editor built for Windows.
</p>

<p align="center">
  <strong>~130 KB binary</strong> · <strong>Instant startup</strong> · <strong>Native C++</strong> · <strong>Zero bloat</strong>
</p>

---

## The Balance: Text Simplicity, Code Power

Most editors force a choice between a basic notepad that lacks helper features, or a heavy IDE that takes seconds to load and hogs memory. Velo sits exactly in the middle. It has the startup speed and lightweight footprint of a simple text editor, combined with the core comforts needed to write code comfortably.

- **Instant Startup:** No loading screens, splash windows, or telemetry. It opens instantly so you can start typing immediately.
- **Clean Dark Aesthetic:** Every interface element—including the title bar, scrollbars, tabs, and dialogs—is custom-drawn in a unified dark theme to prevent bright system flashes.
- **Typographic Clarity:** Loads high-quality fonts (JetBrains Mono for editing, Inter for UI) privately at runtime, ensuring clean readability without cluttering your system fonts.
- **Fluid Coding Usability:** Features smart brace/quote auto-closing, bracket selection wrapping, character deletion helpers, and indent guides to keep your typing flow uninterrupted.
- **State Persistence:** Remembers your open tabs, active document, and setting configurations automatically between launches, picking up exactly where you left off.

## Key Features

- **Languages:** Highlighting for C, C++, Python, and HTML/XML.
- **Custom Scrollbars:** Native custom-painted scrollbars designed to match the theme.
- **Quick Search:** Inline non-obtrusive find bar (`Ctrl+F`).
- **Real-time Settings:** Toggle block cursor, guide lines, whitespace rendering, and auto-braces instantly.

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New tab |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save current file |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+W` | Close active tab |
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
