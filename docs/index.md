# The Redux Kernel Documentation
Welcome to the official internal documentation for **Redux OS**, a 64 bit OS running using limine!

## Release Pipeline & Versioning
Redux follows a strict release workflow to ensure stability before going public.

* **Indev / Nightly (Current):** Private development builds. Volatile, fast breaking changes.
* **Alpha 1 Milestone 1:** Triggers when **Ring 3 (User Mode)** is fully functional.
* **Alpha 1 Milestone 2:** Dedicated bug squashing, stabilization, and code cleanup phase.
* **Version 0.1.0 (Alpha):** First public release binary (Closed Source / No source available).
* **Version 1.0.0 (Beta):** Full open-source release on [Codeberg](https://codeberg.org/).
* **Version 2.0.0 (Release):** First official, fully stable production release.
* **Post-2.0.0 Suffixes:** Every future development build appends `i` (indev), `a` (alpha), or `b` (beta) to the version tag, unless its a release.

---

## 🛠️ System Architecture

### 1. CPU Mode & Initialization
* **Architecture:** Intel/AMD x86_64 Long Mode.
* **Interrupt Handler:** Driven entirely by a custom 64-bit Interrupt Descriptor Table (IDT).
* **Panic Layer:** Features a dedicated, unhandled exception catch mechanism (`PANIC I'M ABOUT TO DIE PANIC PANIC PANIC!`).

### 2. Time Management & Counters
* **Primary Timer:** Local APIC configured in **One-Shot Mode**.
* **Configuration:** Timer Divide Configuration Register (TDCR) set to divide-by-16 clock rate.
* **Interrupt Mapping:** Vector mapped above exception ranges (e.g., `0x40`) with an explicit End-of-Interrupt (`0xB0`) handshake.
* **Anti-Pit Policy:** Replaces legacy 8254 PIT loops with native internal hardware countdowns to protect the developer from dying in a pit.

### 3. Storage & Filesystem
* **Driver Layer:** Native Integrated Drive Electronics / Advanced Technology Attachment (ATA) sector handling.
* **Filesystem:** Integrated **FatFs library** compiled with 64-bit LBA configuration extensions (`FF_FS_EXFAT = 1`).
* **Input Isolation:** Keyboards write cleanly into a shell buffer to prevent backspace deletion of static screen print headers and copyright tags.

---



---

## 📜 Compliance & Contribution Licensing

### The Holy Labyrinth Clause (Section 4.2)
Any downstream open-source contributor or developer cloning Redux from Codeberg who alters, shortens, renames, deletes, or optimizes the directory chain path extending from `\top secret\` to `\PANIC IM ABOUT TO DIE PANIC PANIC PANIC\` will face an automatic, unappealable, and permanent ban from the core dev organization. The humor stays. OR ELSE... i will execute you in Minecraft.


