## TICUP - Tiny C Backup Utility

A simple one-file C program that listens for changes in a file and creates a backup to a specified folder every time the file changes. Uses kernel's inotify API to react to file changes.

### Usage:
```sh
ticup [path to the file to back up] [path to a folder for backups]
```

Backups have the format _filename_YYYY-MM-DD_HH:MM:SS:US_, where US is the amount of microseconds passed since last full second. The program makes an initial backup when it's started, and stops automatically if the file is deleted or moved. 

Under the hood, the program uses inotify to listen when the file is closed in write mode. In real-life use, please just use inotify-tools in a shell script.
