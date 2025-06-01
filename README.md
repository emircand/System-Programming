# System Programming Assignments

This repository contains a series of system-level programming assignments developed in **C** as part of a *System Programming* course. Each project demonstrates core concepts of low-level development, including process management, file I/O, multithreading, inter-process communication, and system call usage.

---

## 📁 Contents

| Folder                   | Description                                                                 |
|--------------------------|-----------------------------------------------------------------------------|
| `hw1/`                   | Multi-process execution using `fork()`, `exec()`, etc.                      |
| `hw2/`                   | Inter-process communication with pipes and shared memory.                  |
| `hw3/`                   | Multithreading and synchronization using POSIX `pthread`.                  |
| `hw4/`                   | File system operations and manipulation.                                   |
| `hw5/`                   | Signal handling and process control.                                       |
| `midterm/`               | Client-server hospital queue system using TCP sockets and custom queues.   |
| `final/`                 | Multi-process pide shop simulation with named pipes, signals, and logging. |

---

## 🛠️ Technologies & Concepts

- **Language**: C  
- **Platform**: Linux (tested on Ubuntu)  
- **Compiler**: GCC  
- **Core Concepts**:
  - Process creation and control (`fork()`, `exec()`, `wait()`)
  - Inter-process communication (`pipe()`, `shmget()`, `mmap()`, `mkfifo()`)
  - Socket programming (`bind()`, `accept()`, `connect()`)
  - Multithreading and synchronization (`pthread`, `mutex`)
  - Signal handling (`signal()`, `kill()`, `sigaction()`)
  - File operations and logging (`open()`, `read()`, `write()`, file locking)
