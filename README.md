# Keys

![Keys screenshot](screenshot.png)

Manage your Omarchy Hyprland keybindings with smart **bind** and **unbind** recommendations.

Keys scans your active Hyprland bindings alongside your recent application launch history (last 30 days) and open windows to highlight:

- **Recommended to bind:** Your most-used apps that don't have a shortcut yet, paired with a conflict-free suggested key combo.
- **Recommended to unbind:** Preinstalled or custom app shortcuts whose apps are not installed or haven't been launched in the last 30 days.
- **Safe edits:** All changes are written atomically inside a managed block in `~/.config/hypr/bindings.lua` with an automatic backup, `hyprctl configerrors` validation (with rollback on error), and full Undo/Redo.

## Keyboard Shortcuts

| Shortcut | Action |
| --- | --- |
| `1` / `2` / `3` | Switch between Recommend, All Bindings, and Most Used |
| `/` | Search all bindings |
| `N` | Add a new custom shortcut |
| `Space` | Refresh usage scan |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / Redo |
| `?` | Toggle shortcut help overlay |
| `Q` | Quit |

## CLI

Keys also supports headless JSON output for scripts and agents:

```sh
keys scan --json   # Usage stats, bind recommendations, and unbind recommendations
keys list --json   # Active bindings and unbound overrides
```

## Build & Install

```sh
./bin/build     # Compile to build/keys
./bin/test      # Run the Qt Test suite offscreen
./bin/install   # Package and install with makepkg -fsi
```
