# rcopy (C + GTK)

Minimal Wayland clipboard history tool.

## Commands

- `rcopy daemon`: save clipboard items (text and images)
- `rcopy toggle`: open/close selector UI

## Behavior

- Daemon stores clipboard payload with MIME type.
- Text is stored as plain text payload.
- Images (`image/png`, `image/jpeg`, `image/webp`) are stored as binary payloads.
- UI shows text entries and image previews.
- `Up/Down`: move selection
- `Enter`: restore selected payload to clipboard with correct MIME, paste with `Ctrl+V`, close
- `Esc`: close

## Build

```bash
make
make install
```

Requires:

- `gtk+-3.0` development files
- `wl-clipboard` (`wl-paste`, `wl-copy`)
- `wtype`

## Startup and keybinding

Use `rcopy daemon` as startup process.
Bind your hotkey to `rcopy toggle`.
