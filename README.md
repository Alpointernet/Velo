<p align="center">
  <img src="icon/app.png" width="128" alt="Velo">
</p>

<h1 align="center">Velo</h1>

<p align="center">
  A minimal, fast text and code editor built for Windows.
</p>

<img width="1400" height="800" alt="Velo editor with tabs and syntax highlighting" src="https://github.com/user-attachments/assets/aceddf78-cd40-452b-ab6d-46651708ccda" />

---

## Key Features

Most text editors are either too simple or too bloated, which makes it harder to get the best experience. Velo comes great out of the box, as simple is better.

- **Lightweight:** Launches the second you click it, and consumes very little memory.
- **Functional:** Has tabs, search and replace tools.  
- **Clean Aesthetic:** Every interface element is intentional, and you can make your own themes easily.
- **Typographic Clarity:** Uses different fonts for editing and the GUI.
- **Coding Usability:** Basic coding features you would expect are here.
- **State Persistence:** Remembers what you did the last time.
- **Quiet File Updates:** Reloads files changed on disk automatically when there are no local edits. Unsaved edits get a small status-bar notice with **Reload** and **Keep edits** actions; reloading is undoable.
- **Responsive Motion:** Short, refresh-rate-aware transitions for tabs, menus, and settings, plus Zen-mode hover reveals that keep the editor viewport stable. Animations can be disabled in Settings.

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
| `F11` | Toggle Zen mode |
| `Shift+F5` | Launch file |
| `Ctrl+Shift+F5` | Launch file as administrator |

## Building & Installation

### Requirements
- **Visual Studio 2022 or newer**, or Visual Studio Build Tools, with **Desktop development with C++** and a Windows SDK.
- **Windows PowerShell 5.1**, included with Windows, for generating the application icon.
- The x64 **Scintilla.dll** and **Lexilla.dll** runtime libraries in the project root. They are local dependencies, not tracked in Git; obtain matching builds from [Scintilla](https://www.scintilla.org/ScintillaDownload.html).
- **[Inno Setup 6](https://jrsoftware.org/isdl.php)** to create the installer.

### Compile
Build and installer helpers (`*.bat`, `*.cmd`, and `scripts/`) are maintained locally and excluded from Git. The commands below apply to a local checkout with those helpers installed.

Run or double-click the local root launcher:
```bat
build.bat
```
It finds the latest installed C++ toolchain, generates `icon\app.ico` from `icon\app.png`, and builds `Velo.exe`. Intermediate compiler and resource files go into `obj\`.

### Create Installer Package
```bat
create_setup.bat
```
This builds the app and creates **`output\VeloInstaller.exe`**, then opens the output folder. The installer includes the editor, runtime libraries, fonts, file-type icons, and project license.

The compiler is discovered from `PATH`, standard installation directories, or locally installed development tools. For a custom installation, set `ISCC` to its full path:
```bat
set "ISCC=C:\Tools\Inno Setup 6\ISCC.exe"
create_setup.bat
```

### Clean Generated Files
```bat
clean.bat
```
Removes `Velo.exe`, `obj\`, `output\`, the generated application icon, and legacy build intermediates. Settings, sessions, and backups are stored separately in `%APPDATA%\Velo`.

### Automation
Every launcher accepts `--no-pause` and returns a nonzero exit code on failure:
```bat
build.bat --no-pause
create_setup.bat --no-pause
clean.bat --no-pause
```
Setting `NOPAUSE=1` provides the same behavior. Installer builds in this mode also skip opening Explorer. The root launchers forward to `scripts\`; both entry points work regardless of the caller's working directory.

## Project Structure

```
build.bat           → Local application build launcher (ignored)
create_setup.bat     → Local installer build launcher (ignored)
clean.bat           → Local generated-file cleanup launcher (ignored)
src/
  main.cpp          → Application entry point and window message handling
  globals.h         → Shared state, settings, and command identifiers
  include/          → Third-party Scintilla API headers
  components/
    animations.*    → Shared frame timing and UI transitions
    shortcuts.*     → Application keyboard shortcuts
    tabmanager.*    → Tabs, file operations, and session persistence
    editor.*        → Scintilla styling and editing behavior
    dialogs.*       → Settings, popup menus, and dialogs
    ui_drawing.*    → App chrome, layout, and hit testing
    paint_buffer.h  → Reusable GDI paint buffer
    theme.*         → Theme loading and live updates
    fonts.*         → Private runtime fonts
fonts/              → Editor and interface font assets
icon/               → Application image and file-type icons
scripts/            → Local build, installer, and cleanup implementation (ignored)
obj/                → Generated intermediates (ignored)
output/             → Generated installer (ignored)
```

Formatting and line-ending conventions are defined in `.editorconfig` and `.gitattributes`.

## License

See [LICENSE.md](LICENSE.md) for Velo's license. Velo uses the [Scintilla](https://www.scintilla.org/) editing library and Lexilla for syntax highlighting.
