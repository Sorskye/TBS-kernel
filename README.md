# UPL Kernel

## Overview
The UPL Kernel (User Programmable Linux Kernel) is a lightweight, modular operating system designed for embedded systems and educational purposes. It offers a flexible architecture that allows developers to customize and optimize kernel features for their specific use cases.

## Architecture
The UPL Kernel follows a microkernel architecture, minimizing the core functionalities to provide a smaller attack surface and increased stability. Key components include:

- **Microkernel**: Handles basic services such as interprocess communication (IPC) and memory management.
- **User Space Services**: Various services run in user space, communicating with the microkernel to extend functionality without compromising kernel integrity.
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

3. **Configure the Build**:
   Modify the configuration files to suit your target platform in the `config/` directory.

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