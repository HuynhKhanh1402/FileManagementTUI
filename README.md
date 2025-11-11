# 📁 T- 📂 Navigate directories with intuitive controls
- 📊 View detailed file information (size, permissions, owner, group, modification time)
- ➕ Create new directories
- 📄 Create new empty files (like `touch` command)
- ℹ️ Display comprehensive info about files/directories (includes access time, inode, etc.)
- 🗑️ Delete files and directories (with confirmation)
- ✏️ Rename files and directories
- 🚀 Move files/directories to another location (supports absolute and relative paths)
- 📋 Copy files
- 👁️ View file contents with system pager
- 🎨 Color-coded interface with ncurses (blue=dir, green=file, cyan=link)le Manager

A professional terminal-based file manager written in C using ncurses library. Features a clean interface with detailed file information display.

## ✨ Features

- 📂 Navigate directories with intuitive controls
- 📊 View detailed file information (size, permissions, owner, group, modification time)
- ➕ Create new directories
- � Create new empty files (like `touch` command)
- ℹ️ Display comprehensive info about files/directories (includes access time, inode, etc.)
- �🗑️ Delete files and directories (with confirmation)
- ✏️ Rename/move files and directories
- 📋 Copy files
- 👁️ View file contents with system pager
- 🎨 Color-coded interface with ncurses

## 🏗️ Project Structure

```
FileManagement2/
├── include/          # Header files
│   ├── fs.h         # File system operations
│   └── ui.h         # User interface
├── src/             # Source files
│   ├── fs.c         # File system implementation
│   ├── ui.c         # UI implementation
│   └── main.c       # Entry point
├── bin/             # Binary executable (generated)
├── obj/             # Object files (generated)
├── Makefile         # Build configuration
├── .gitignore       # Git ignore rules
└── README.md        # This file
```

## 🔧 Requirements

- GCC compiler (C11 or later)
- ncurses library
- Linux/Unix environment

### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libncurses5-dev libncursesw5-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc ncurses-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel ncurses
```

## 🚀 Quick Start

### Build

```bash
make
```

### Run

```bash
bin/filemgr [start_directory]
# or
make run
```

### Clean Build Artifacts

```bash
make clean
```

### Rebuild from Scratch

```bash
make rebuild
```

## ⌨️ Keyboard Controls

| Key | Action |
|-----|--------|
| `↑` / `↓` | Navigate through files and directories |
| `Enter` | Open directory or view file details |
| `Backspace` | Go to parent directory |
| `n` | Create new directory |
| `f` | Create new file (like touch) |
| `d` | Delete selected item (with confirmation) |
| `r` | Rename selected item in current directory |
| `m` | Move item to another directory (absolute or relative path) |
| `c` | Copy selected file |
| `i` | Show detailed information about selected item |
| `o` | Open file with `$PAGER` (default: `less`) |
| `q` | Quit application |

## 📝 Notes

- This is an educational project demonstrating C programming with ncurses
- Use caution when deleting files - there's no undo!
- Directory deletion only works for empty directories
- Tested on Linux with ncurses library

## 🛠️ Development

### Makefile Targets

- `make` or `make all` - Build the project
- `make build` - Same as `make all`
- `make run` - Build and run the program
- `make clean` - Remove build artifacts
- `make rebuild` - Clean and rebuild
- `make info` - Show project build information

## 📄 License

Educational project - feel free to use and modify.

## 🤝 Contributing

This is a learning project. Feel free to fork and experiment!
