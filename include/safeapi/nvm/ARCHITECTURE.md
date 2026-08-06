/**
 * @page nvm_architecture Non-Volatile Memory Architecture
 *
 * Address-based read/write to persistent storage. Caller manages layout.
 * Backend handles flash/EEPROM mechanics.
 *
 * @section nvm_architecture_api Read/Write Interface
 *
 * sapi_nvm_read(offset, data, size) - Read from NVM
 * sapi_nvm_write(offset, data, size) - Write to NVM
 *
 * @section nvm_architecture_wear Flash Wear
 *
 * Flash has ~100k erase cycles. Application responsible for wear leveling
 * if needed. Write checksums for integrity.
 *
 * @section nvm_architecture_backend Backend Implementation
 *
 * Platform-specific: STM32 flash, external SPI flash, etc.
 * Framework provides abstract interface.
 *
 */

