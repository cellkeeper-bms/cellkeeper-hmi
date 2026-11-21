/* Fails the link of a build that leaves too little internal RAM for the ESP32-P4's start-up.
 *
 * Added to the link by __init__.py for pre-v3 ESP32-P4 silicon (esp32: engineering_sample: true)
 * built with ESPHome 2026.10 or later through ESP-IDF directly. Until FreeRTOS starts, the only
 * heap is the internal RAM from _heap_start (the end of static data) to 0x4ff3afc0, which is
 * APP_USABLE_DIRAM_END in ESP-IDF's heap/port/esp32p4/memory_layout.c (SOC_ROM_STACK_START -
 * SOC_ROM_STACK_SIZE); tools/boot_heap.py reads the same end from the ELF's region table. The
 * Wi-Fi co-processor driver and the system tasks fill that heap; with too little of it the P4
 * asserts in vApplicationGetIdleTaskMemory at every boot and needs a USB flash to recover.
 * 110000 bytes: 108,720 asserted and 113,360 boots (ESPHome 2026.10.2). Keep it equal to
 * FAIL_BELOW in tools/boot_heap.py. Ninja does not relink when only this file changes: delete
 * the build's .elf files to try a new limit on an existing build.
 */
ASSERT(0x4ff3afc0 - _heap_start >= 110000,
       "CellKeeper: under 110000 bytes of internal RAM left for start-up (0x4ff3afc0 - _heap_start). The P4 would assert in vApplicationGetIdleTaskMemory at every boot: move static data to the heap (see components/hmi_bms/__init__.py, tools/boot_heap.py)")
