# terrible-todo-tui

A TODO TUI app written in C. 

## Controls

Normal mode:
- `arrow up`: move cursor up
- `arrow down`: move cursor down
- `a`: adds new entry to current list (starts insert mode)
- `d`: deletes selected entry
- `enter`: move selected entry to the other list
- `q`: quits the program

Insert mode:
- `arrow left`: move cursor left
- `arrow right`: move cursor right
- `backspace`: delete character before the cursor
- `del`: delete character at cursor
- `enter`: finish writing the entry (back to normal mode)
- `esc`: abort writing the entry (back to normal mode)

## Quick start

On linux:
```
$ ./build.sh
$ ./build/todo-tui
```

On windows:
```
$ .\build.bat
$ .\build\todo-tui.exe
```

## The TODO App mascot

<img src="https://bigrat.monster/media/bigrat.jpg" width="50%">
