<p align="center">
  <img src="icon/app.png" width="128" alt="Velo">
</p>

<h1 align="center">Velo</h1>

<p align="center">
  A minimal, fast code editor that stays out of your way.
</p>

<p align="center">
  <strong>~130 KB</strong> binary · <strong>Native C++</strong> · <strong>Zero frameworks</strong> · <strong>Instant startup</strong>
</p>

---

## Why Velo

Most editors are either too bloated or too bare. Velo sits in the sweet spot — it gives you everything you need to write code comfortably, and nothing you don't.

- **Opens instantly.** No splash screens, no loading bars, no telemetry. Double-click and you're typing.
- **Truly lightweight.** The entire editor is a single ~130 KB executable. It uses less RAM than your browser's favicon cache.
- **Built for dark mode.** Custom-drawn title bar, scrollbars, tabs, and dialogs — all designed from scratch to feel native and cohesive. No bright white flashes, ever.
- **Comfortable to code in.** Syntax highlighting for C/C++, Python, and HTML. Auto-closing braces and quotes. Smart bracket wrapping. Indentation guides. All the small things that make typing code feel right.
- **Remembers your workspace.** Close it with five files open, reopen it tomorrow — same tabs, same settings, right where you left off.

## At a glance

| | |
|---|---|
| **Languages** | C, C++, Python, HTML/XML |
| **Fonts** | JetBrains Mono (editor), Inter (UI) — bundled, no install needed |
| **Session** | Persists open tabs, active tab, and all settings automatically |
| **Shortcuts** | `Ctrl+N` new · `Ctrl+O` open · `Ctrl+S` save · `Ctrl+W` close · `Ctrl+F` find · `Ctrl+Tab` switch tabs |
| **Settings** | Auto-close braces · Indentation guides · Whitespace rendering · Block cursor |

## Building

Requires **Visual Studio 2022** with the C++ desktop workload.

```bat
scripts\build.bat
```

To build the installer (requires [Inno Setup 6](https://jrsoftware.org/isdl.php)):

```bat
scripts\create_setup.bat
```

## Project structure

```
src/                → Application source
  main.cpp          → Entry point, window proc, keybinds
  globals.h         → Shared state and constants
  components/       → Editor, tabs, dialogs, UI drawing, fonts
scintilla/          → Scintilla editing component (built from source)
fonts/              → Bundled JetBrains Mono and Inter
icon/               → App icon
scripts/            → Build and installer scripts
```

## License

Uses the [Scintilla](https://www.scintilla.org/) editing component. See `scintilla/scintilla/License.txt`.
