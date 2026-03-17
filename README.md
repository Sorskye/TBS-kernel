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
- ![Kernel Architecture](images/kernel_architecture.png)
   *Diagram of UPL Kernel architecture*
- ![Build Process](images/build_process.png)
   *Overview of the kernel build process*

---
