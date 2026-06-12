# EmbedKit: Lightweight Circular Buffer (FIFO) Module

**Author:** M Mohammed Riyaz  
**Language:** C (C99 Standard)  
**Target Architecture:** Microcontrollers (ARM Cortex-M, AVR, ESP32, etc.)

---

## 📖 Overview: What is a Ring Buffer?
A **Ring Buffer** (or Circular Queue) is a fixed-size memory array wrapped end-to-end to operate as a continuous loop. It uses a First-In-First-Out (FIFO) data structure. 

In embedded systems, ring buffers are critical for safely passing data between asynchronous domains—such as transferring incoming bytes from a fast Hardware Interrupt Service Routine (like a UART RX interrupt) to the slower background `main()` loop. 

**Key Advantages:**
* **Deterministic Memory:** Uses static allocation (`uint8_t storage[8]`), entirely avoiding `malloc()`, memory leaks, and heap fragmentation.
* **Non-Blocking Logic:** The read and write pointers chase each other in a circle, meaning data can be continuously written and read without shifting elements around in memory.

---

## ⚙️ Core Features & Module Breakdown
This module provides a strictly typed, defensive-programming API for handling 8-bit data streams. 

### 1. Initialization (`ringbuf_init`)
Sets the internal `head` (write index), `tail` (read index), and `count` (capacity tracker) to absolute zero. This ensures the queue is in a known, clean state upon MCU boot or reset before hardware interrupts are enabled.

### 2. State Management (`ringbuf_count`, `ringbuf_is_empty`, `ringbuf_is_full`)
These lightweight getter functions provide the current status of the buffer. By checking these states *before* reading or writing, the application prevents buffer overruns and prevents returning garbage data.

### 3. Data Ingestion (`ringbuf_write`)
Inserts a single byte at the `head` pointer.
* **Safety Protocol:** Refuses to overwrite unread data. If the buffer is full (`count == 8`), the function safely rejects the incoming byte and returns a negative error code.
* **Execution:** Advances the `head` pointer and increments the total `count`.

### 4. Data Extraction (`ringbuf_read`)
Retrieves the oldest unread byte from the `tail` pointer.
* **Safety Protocol:** If the buffer is empty, it returns a precise error code rather than outputting stale data left over in RAM.
* **Execution:** Advances the `tail` pointer and decrements the total `count`.

---

## 🚀 Performance: The Bitwise Wrap-Around Trick
Standard circular buffers use the modulo operator `%` to wrap pointers back to zero when they hit the array limit (e.g., `head = (head + 1) % 8`). 

However, on lightweight microcontrollers lacking a dedicated hardware divider, the `%` operator triggers a slow software division routine that consumes dozens of CPU clock cycles. Inside a high-frequency UART interrupt, this latency can cause dropped packets.

**The Optimization:**
Because this buffer's size is strictly locked to a power of two (`8`), this module replaces the modulo operator with a **Bitwise AND mask**:
```c
rb->head = (rb->head + 1) & (BUFFER_SIZE - 1);