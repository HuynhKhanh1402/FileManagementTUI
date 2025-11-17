# 📁 Terminal File Manager

A professional terminal-based file manager written in C with ncurses. Navigate and manage files efficiently from the command line with a clean, color-coded interface.

## Features

- **Directory Navigation**: Browse directories with keyboard controls
- **File Operations**: Create, delete, rename, move, and copy files and directories
- **Built-in File Viewer**: View text files with line numbers and scrolling controls
- **File Editing**: Edit files directly with nano or vim integration
- **Detailed File Information**: Display permissions, ownership, size, timestamps, and inode numbers
- **Color-Coded Interface**: Directories, files, and symbolic links use distinct colors for easy identification
- **Confirmation Dialogs**: Safety prompts before destructive operations

## Requirements

- GCC compiler (C11 or later)
- ncurses library
- Linux/Unix environment
- nano or vim (for file editing)

### Installing Dependencies

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

## Building and Running

### Build
```bash
make
```

### Run
```bash
bin/filemgr [start_directory]
```
If no directory is specified, the current directory is used.

### Alternative: Build and Run
```bash
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

## Keyboard Controls

| Key | Action |
|-----|--------|
| `↑` / `↓` | Navigate through files and directories |
| `Enter` | Open directory or view file details |
| `Backspace` | Go to parent directory |
| `n` | Create new directory |
| `f` | Create new file |
| `d` | Delete selected item (with confirmation) |
| `r` | Rename selected item |
| `m` | Move item to another directory |
| `c` | Copy selected file |
| `i` | Show detailed information |
| `o` | Open file in built-in viewer |
| `e` | Edit file with nano/vim |
| `q` | Quit application |

### File Viewer Controls

When viewing a file (press `o`):

| Key | Action |
|-----|--------|
| `↑` / `↓` | Scroll line by line |
| `PgUp` / `PgDn` | Scroll page by page |
| `Home` | Jump to start |
| `End` | Jump to end |
| `q` / `ESC` | Exit viewer |

## Project Structure

```
FileManagement2/
├── include/          # Header files
│   ├── fs.h         # File system operations API
│   └── ui.h         # User interface API
├── src/             # Source files
│   ├── fs.c         # File system operations implementation
│   ├── ui.c         # User interface implementation
│   └── main.c       # Application entry point
├── bin/             # Compiled binary (generated)
├── obj/             # Object files (generated)
├── Makefile         # Build configuration
└── README.md        # Documentation
```

## Architecture

The application is structured in three layers:

1. **File System Layer** (`fs.c`/`fs.h`): Handles all file operations including reading directories, creating/deleting files, copying, moving, and file I/O.

2. **User Interface Layer** (`ui.c`/`ui.h`): Manages the ncurses-based terminal UI, including window management, color schemes, and user input handling.

3. **Main Application** (`main.c`): Entry point that initializes the UI with the specified starting directory.

## Implementation Details

- **Language**: C (C11 standard)
- **UI Library**: ncurses for terminal interface
- **File Operations**: POSIX system calls (stat, open, read, write, etc.)
- **Sorting**: Directories listed first, then alphabetically by name
- **Error Handling**: Comprehensive error checking with user-friendly messages

## Limitations

- Directory deletion only works for empty directories
- File copying does not preserve all metadata (permissions are set to default)
- No undo functionality for delete operations
- Terminal must support colors
- Symbolic links are displayed but not followed when opened

## Development

### Makefile Targets

- `make all` / `make build` - Compile the project
- `make run` - Build and execute the program
- `make clean` - Remove compiled binaries and object files
- `make rebuild` - Clean and build from scratch
- `make info` - Display project configuration

### Code Style

The codebase follows these conventions:
- K&R-style indentation
- Function names prefixed with `fm_` for file manager operations
- Clear separation between UI and file system logic
- Comprehensive comments for complex operations

## License

MIT License - see [LICENSE](LICENSE) file for details.

Copyright (c) 2025 Huỳnh Quốc Khánh

## Contributing

This project is open for contributions. Feel free to submit issues or pull requests.
