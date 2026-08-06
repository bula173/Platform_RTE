/**
 * @page nvm_user_guide Non-Volatile Memory User Guide
 *
 * @section nvm_user_guide_overview What is NVM?
 *
 * NVM provides persistent storage for configuration, state, diagnostics.
 * Caller manages address space and layout. Read/write with checksums.
 *
 * @section nvm_user_guide_quick_start Quick Start
 *
 * @code
 * // Define NVM layout
 * #define CONFIG_ADDR 0
 * #define CONFIG_SIZE 256
 *
 * // Read configuration
 * config_t cfg;
 * sapi_nvm_read(CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
 *
 * // Verify checksum
 * uint32_t crc = compute_crc((uint8_t *)&cfg, sizeof(cfg) - 4);
 * if (crc != cfg.checksum) {
 *     log_error("Config corrupted");
 *     use_default_config(&cfg);
 * }
 *
 * // Save configuration
 * cfg.checksum = compute_crc((uint8_t *)&cfg, sizeof(cfg) - 4);
 * sapi_nvm_write(CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
 * @endcode
 *
 * @section nvm_user_guide_guidelines Best Practices
 *
 * 1. Always use checksums
 * 2. Manage wear (limit writes)
 * 3. Plan address layout
 * 4. Version data for evolution
 *
 * @section nvm_user_guide_see_also See Also
 *
 * - @ref nvm_architecture for design
 * - @ref checksum_user_guide for CRC
 *
 */

