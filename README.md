# TBS Kernel

## **Overview**

TBS is a hobby operating system kernel developed as a learning project to explore low-level system design and operating system internals.

The project focuses on implementing fundamental OS components from scratch in order to better understand how modern systems work. While still experimental, the kernel has grown beyond a minimal prototype and now includes several core subsystems such as preemptive multitasking, terminal interfaces, a basic window compositor, and virtual memory management.

The system is intentionally kept simple and readable so that new features and ideas can be explored without excessive complexity.

---

## **Features**

Current functionality implemented in the kernel:

- **Preemptive Multitasking**  
  A timer-driven scheduler allowing multiple tasks to run concurrently.

- **TTY Subsystem**  
  Basic terminal interfaces used for system interaction and debugging.

- **Window Compositor**  
  A minimal graphical compositor capable of drawing and managing simple windows.

- **Memory Management**
  - Identity-mapped paging
  - Basic virtual memory setup

- **RAM Filesystem (Work in Progress)**  
  An in-memory filesystem used during early system development.

---

## **Architecture**

TBS currently follows a **monolithic kernel architecture**, with most core functionality executing in kernel space (ring 0).

Paging is enabled for memory management and hardware-enforced protection. The system currently uses identity-mapped memory during early initialization.

While the kernel is currently monolithic, the design leaves room for potential **future separation between kernel and userspace** as the system evolves.

---

## **Project Goals**

The primary goals of this project are:

- Learn practical **kernel development**
- Understand **memory management and scheduling**
- Build fundamental OS components from scratch
- Experiment with system design decisions

The project prioritizes **learning, experimentation, and simplicity** over completeness or production readiness.

---

## **Roadmap**

Planned next steps for the system:

- Improve and stabilize the **RAM filesystem**
- Implement a **disk-based filesystem**
- Add a minimal **C runtime environment using musl libc**
- Port **TinyCC (TCC)** to allow compiling programs directly inside the OS
- Expand userspace support

---

## **Building**

### **Requirements**

- GCC cross-compiler (e.g. `x86_64-elf-gcc`)
- NASM
- Make
- QEMU

### **Build**

```bash
make
```

## **Boot Process**

A simplified overview of the system startup sequence:
- Bootloader loads the kernel
- Kernel initializes basic hardware and memory
- Paging is enabled
- Scheduler and multitasking are initialized
- Core subsystems (TTY, compositor, filesystem) start
- System enters the main kernel loop

## **Project Structure**
wip

## **Screenshots**
below is a screenshot of the kernel running the simple compositor. a few commands are displayed such as directory manipulation and pci enumeration
<img width="931" height="515" alt="rev1" src="https://github.com/user-attachments/assets/9e814393-4f99-44c0-a994-41ecc6299b1d" />

