# rcopy

rcopy is a lightweight Wayland clipboard history tool written in C + GTK.

It captures text and images, keeps a searchable picker UI, and supports fast toggle show/hide with a resident picker process.

## Features

- Text and image clipboard capture (`text/plain`, `image/png`, `image/jpeg`, `image/webp`)
- Searchable GTK picker UI with image previews
- Keyboard-first interaction:
	- `Up/Down` navigate
	- `Enter` restore + paste + hide
	- `Esc` hide
- Resident hidden picker for fast open
- Session-scoped storage (cleared on reboot/logout)
- Startup integration with systemd user services

## Commands

- `rcopy daemon`: capture clipboard history in the background
- `rcopy picker`: run hidden resident picker process
- `rcopy toggle`: show/hide picker (used by keybinding)

## Requirements

- Linux Wayland session
- `gtk+-3.0` development package
- `wl-clipboard` (`wl-paste`, `wl-copy`)
- `wtype`
- `systemd --user`

## Build and Install

```bash
make
make install
```

`make install` does all of the following:

- installs binary to `~/.local/bin/rcopy`
- installs services:
	- `~/.config/systemd/user/rcopy.service`
	- `~/.config/systemd/user/rcopy-picker.service`
- runs `systemctl --user daemon-reload`
- enables and starts both services

Useful install targets:

- `make install-bin`: install binary only
- `make install-startup`: install/enable services only
- `make disable-startup`: disable and remove services

## Data Storage (Important)

Clipboard history is stored in session runtime storage, not persistent home storage:

- `XDG_RUNTIME_DIR/rcopy`
- usually `/run/user/<uid>/rcopy`

This means history is cleared automatically after reboot/logout.

Within a session, history is bounded by `max_items` (default `500`) and older entries are pruned.

## GNOME Keybinding (Alt+V)

Set toggle to `Alt+V`:

```bash
gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/rcopy-toggle/']"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/rcopy-toggle/ name 'rcopy toggle'
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/rcopy-toggle/ command '$HOME/.local/bin/rcopy toggle'
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/rcopy-toggle/ binding '<Alt>v'
```

## Service Checks

```bash
systemctl --user status rcopy.service
systemctl --user status rcopy-picker.service
```

## Troubleshooting

- Picker does not open:
	- verify keybinding command points to `~/.local/bin/rcopy toggle`
	- restart picker service:
		`systemctl --user restart rcopy-picker.service`
- No new items in picker:
	- verify daemon is running:
		`systemctl --user status rcopy.service`
	- verify runtime files exist under `/run/user/<uid>/rcopy`
- First open slow:
	- ensure `rcopy-picker.service` is active (prewarmed hidden picker)
