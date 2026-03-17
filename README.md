# UPL Kernel

## Overview
The TBS kernel is a lightweight, modular operating system with the main focus being to learn kernel design and principles for myself. The goal is to offer a flexible architecture that allows developers to customize and optimize kernel features for their specific use cases.

## Architecture
The UPL Kernel aims for a Monolithic architecture with hardware enforced memory protection while running in ring 0. ( The system might migrate to a kernel- userspace seperation later )
The main goals of the system are:
- **Device Drivers**: Modular drivers can be added and removed, allowing for easy hardware support.
- **File System**: A lightweight file system tailored for embedded applications, with support for various file operations.
  

## Build Instructions
To build the UPL Kernel, follow these steps:
1. **Prerequisites**: Ensure you have the following tools installed:
   - GCC (GNU Compiler Collection)
   - Make
   - Git
   - QEMU (for testing)

2. **Clone the Repository**:
   ```bash
   git clone https://github.com/Sorskye/UPL-kernel.git
   cd UPL-kernel
   ```

4. **Build the Kernel**:
   Run the following command to compile the kernel:
   ```bash
   make
   ```

5. **Install the Kernel**:
   After building, install the kernel to your target device or use QEMU for emulation.

## Images
TBS kernel running a simple window compositor using a tty for keyboard input and command output. Use of the simple ramfs can be seen with directory operations.
Also a placeholder command can be seen that enumerates the pci devices connected to the machine.
<img width="931" height="515" alt="rev1" src="https://github.com/user-attachments/assets/5fbeb035-13a5-412f-a22f-c7f49e38de13" />

---
