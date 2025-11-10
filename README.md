TUI File Manager (C, ncurses)

A simple terminal-based file manager written in C using ncurses. Features:
- Navigate directories
- View file/directory info (size, permissions, mtime)
- Create directory
- Delete file/directory (non-recursive)
- Rename/move
- Copy files
- Open file with $PAGER

Build

```bash
make
```

Run

```bash
make run
```

Controls
- Up/Down: navigate
- Enter: open directory / view file info
- Backspace: go up
- n: create directory
- d: delete selected (confirm)
- r: rename
- c: copy file
- o: open file with $PAGER
- q: quit

Notes
- Minimal, educational project. Use carefully when deleting files.
- Tested on Linux with ncurses.
