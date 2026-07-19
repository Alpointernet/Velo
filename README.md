<p align="center">
  <img src="icon/app.png" width="128" alt="Velo">
</p>

<h1 align="center">Velo</h1>

<p align="center">
  A minimal, distraction-free text and code editor built for speed and utility.
</p>

<p align="center">
  <strong>~130 KB binary</strong> · <strong>Instant startup</strong> · <strong>Native C++</strong> · <strong>Zero bloat</strong>
</p>

---

## The Balance: Text Simplicity, Code Power

Most editors force a choice: a basic notepad that lacks essential coding amenities, or a heavy IDE/editor that takes seconds to load, hogs memory, and demands configuration.

Velo is built to be the perfect middle ground. It is as fast and clean as a simple text editor, but loaded with the modern details you expect when writing code.

- **Instant Startup:** Launches instantly. No splash screens, no update checks, no background processes. Double-click and you are typing.
- **Zero Config, Private Fonts:** Ships as a single self-contained setup. It loads professional typography (JetBrains Mono for editor, Inter for UI) privately at runtime without installing them on your system.
- **Tailored for Dark Mode:** Every single window component—including the title bar, scrollbars, dialogs, and tabs—is custom-drawn to match a premium dark theme. No blinding white flashes on launch or dialog popup.
- **Usability Focus:** Equipped with smart brace/quote auto-closing, bracket selection wrapping, brace pair deletion, and indent guides. All designed to make editing fluid and natural.
- **Workspace Memory:** Saves your open tabs, the active tab, and your settings automatically. Reopen Velo and pick up exactly where you left off.

## Key Features

- **Languages:** Built-in highlighting for C, C++, Python, and HTML/XML.
- **Dynamic Scrollbars:** Custom-rendered minimal scrollbars matching the theme color palette.
- **Inline Find:** Fast, non-obtrusive find bar (`Ctrl+F`).
- **Real-time Settings:** Toggle features like block cursor, whitespace indicators, auto-braces, and indent guides instantly from settings.

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
