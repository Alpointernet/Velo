<p align="center">
  <img src="icon/app.png" width="128" alt="Velo Icon">
</p>

<h1 align="center">Velo</h1>

<p align="center">
  A fast, lightweight code editor for Windows built with C++ and Scintilla.
</p>

---

## Features

- **Syntax Highlighting** — C/C++, Python, HTML/XML with full Lexilla integration
- **Tabbed Editing** — Open multiple files with a clean tab bar
- **Dark Theme** — Polished dark UI with custom-drawn title bar, scrollbars, and controls
- **Session Persistence** — Remembers open tabs, active tab, and all settings between launches
- **Auto-Close Braces** — Automatically pairs `()`, `{}`, `[]`, `""`, `''` with smart step-over and backspace deletion
- **Selection Wrapping** — Select text and type a bracket/quote to wrap it
- **Indentation Guides** — Visual block-indent markers
- **Word Wrap & Line Numbers** — Togglable via keyboard shortcuts
- **Find Bar** — Quick inline search with `Ctrl+F`
- **Custom Scrollbars** — Styled dark scrollbars that match the theme
- **Portable Fonts** — Ships with JetBrains Mono and Inter, loaded privately at runtime

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New tab |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+W` | Close tab |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+F` | Find |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |

## Building

### Prerequisites

- **Visual Studio 2022** (Community or higher) with C++ desktop development workload
- **Inno Setup 6** (optional, for building the installer)

### Compile

```bat
scripts\build.bat
```

This will:
1. Build Scintilla from source
2. Compile the resource file (icon)
3. Compile all C++ source files and link into `Velo.exe`

### Create Installer

```bat
scripts\create_setup.bat
```

Builds the project, then compiles an installer using Inno Setup. The output (`VeloInstaller.exe`) is placed in the `output/` folder, which opens automatically when done.

## Project Structure

```
├── src/                  # Application source code
│   ├── main.cpp          # Entry point, window procedure, keybinds
│   ├── globals.h         # Shared declarations and constants
│   └── components/       # Modular UI components
│       ├── editor.cpp    # Scintilla styling and syntax highlighting
│       ├── tabmanager.cpp# Tab management and session persistence
│       ├── dialogs.cpp   # Settings dialog and message boxes
│       ├── ui_drawing.cpp# Custom title bar, tab bar, and scrollbar painting
│       └── fonts.cpp     # Private font loading
├── scintilla/            # Scintilla source (built during compile)
├── fonts/                # JetBrains Mono and Inter font files
├── icon/                 # Application icon (app.png)
├── scripts/              # Build and installer scripts
│   ├── build.bat
│   ├── create_setup.bat
│   └── installer.iss
└── resource.rc           # Windows resource file (icon embedding)
```

## License

This project uses the [Scintilla](https://www.scintilla.org/) editing component, which is released under a permissive license. See `scintilla/scintilla/License.txt` for details.
