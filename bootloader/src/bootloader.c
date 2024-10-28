#include "bootloader.h"

// clang-format off
#include "ch.h"
#include "hal.h"
// clang-format on

#include <id_eeprom.h>
#include <sha256.h>
#include <stdlib.h>
#include <stm32h7xx_hal.h>

#include "board_ex.h"
#include "chprintf.h"
#include "globals.h"
#include "lwip/sockets.h"

// Macro to send a constant string to a socket
#define SEND(sock, str) write(sock, str, strlen(str))

#define GREETING_MESSAGE "BOOTLOADER VERSION:xcore-boot v1.0"
#define BOARD_MSG "BOARD:"
#define VERSION_MSG "HARDWARE VERSION:"
#define CARRIER_BOARD_MSG "CARRIER BOARD INFO:"
#define BOOTLOADER_PORT 8007

// 256bits need to be written at a single time and the buffer needs to be
// aligned, so we first copy our bytes into this tmp buffer and then let the
// hardware write to flash.
static uint8_t buffer[1000] __attribute__((aligned(32))) = {};

// Buffer for sending responses
static char out_buffer[1000];

// Working Area for the bootloader thread
static THD_WORKING_AREA(wa_bootloader_thread, 128000);

enum BOOTLOADER_STATE { HASH, IMAGE_SIZE, IMAGE_DATA };

static SHA256_CTX sha256;

/**
 * Parses a SHA-256 hash in hexadecimal format from the given buffer.
 *
 * @param buf  Pointer to the input buffer containing the hash in hexadecimal
 * format.
 * @param buflen  Length of the input buffer. Must be 64 to be valid.
 * @param hash  Array of 8 uint32_t elements to store the parsed hash.
 * @return true if the hash was successfully parsed, false otherwise.
 */
static bool parseSHA256(const char *buf, size_t buflen, uint8_t hash[32]) {
  if (buflen != 64) {
    return false;
  }
  for (uint8_t i = 0; i < 32; i++) {
    hash[i] = 0;
    for (uint8_t j = 0; j < 2; j++) {
      // 4 bits per character
      hash[i] <<= 4;
      char c = buf[2 * i + j];
      if (c >= '0' && c <= '9') {
        hash[i] |= c - '0';
      } else if (c >= 'A' && c <= 'F') {
        hash[i] |= (c - 'A') + 10;
      } else if (c >= 'a' && c <= 'f') {
        hash[i] |= (c - 'a') + 10;
      } else {
        return false;
      }
    }
  }
  return true;
}

static THD_FUNCTION(bootloader_thread, arg) {
  (void)arg;
  chRegSetThreadName("Bootloader");

  // Create a socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    return;
  }

  // Bind the socket to a port
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(BOOTLOADER_PORT);

  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    lwip_close(sockfd);
    return;
  }

  // Listen for incoming connections
  if (listen(sockfd, 0) < 0) {
    lwip_close(sockfd);
    return;
  }

  bool locked = false;

  // Main loop
  while (1) {
    // Accept a new connection
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (connfd < 0) {
      continue;
    }

    // Prevent chip from starting while bootloading
    if (!locked) {
      chMtxLock(&reboot_mutex);
      locked = true;
    }

    int flag = 1;
    int result =
        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    if (result < 0) {
      close(connfd);
      continue;
    }
    // Write Hardware Info
    SEND(connfd, GREETING_MESSAGE);
    SEND(connfd, "\n");
    SEND(connfd, BOARD_MSG);
    write(connfd, BOARD_NAME, strlen(BOARD_NAME));
    SEND(connfd, "\n");
    SEND(connfd, VERSION_MSG);
    SEND(connfd, "0");
    SEND(connfd, "\n");
    SEND(connfd, CARRIER_BOARD_MSG);
    SEND(connfd, "N/A");
    SEND(connfd, "\n");

    SEND(connfd, "SEND HASH\n");

    enum BOOTLOADER_STATE state = HASH;

    FLASH_EraseInitTypeDef EraseInitStruct;

    // Handle the connection
    int bytes_received;
    size_t buffer_fill = 0;
    size_t receive_bytes = 1;
    uint8_t hash[32];
    bool ok = true;

    size_t image_size = 0;
    size_t received_bytes = 0;

    // State for flashing
    uint32_t flash_address = TARGET_FLASH_ADDRESS;

    while (ok && buffer_fill < (sizeof(buffer) - receive_bytes) &&
           (bytes_received =
                read(connfd, buffer + buffer_fill, receive_bytes)) > 0) {
      buffer_fill += bytes_received;

      switch (state) {
        case HASH:
          // Receive single bytes until we have 64
          receive_bytes = 1;
          if (buffer[buffer_fill - 1] != '\n') {
            continue;
          }
          if (buffer_fill < 64) {
            ok = false;
            continue;
          }

          bool hash_ok = parseSHA256(buffer, 64, hash);
          if (!hash_ok) {
            ok = false;
            continue;
          }

          SEND(connfd, "HASH OK\nSEND LENGTH\n");
          buffer_fill = 0;
          state = IMAGE_SIZE;
          break;
        case IMAGE_SIZE: {
          receive_bytes = 1;
          if (buffer[buffer_fill - 1] != '\n') {
            continue;
          }
          // Put 0 termination
          buffer[buffer_fill - 1] = 0;
          image_size = strtol(buffer, NULL, 10);
          chsnprintf(out_buffer, sizeof(out_buffer), ">Image size: %u\n",
                     image_size);
          write(connfd, out_buffer, strlen(out_buffer));

          if (image_size > PROGRAM_FLASH_SIZE_BYTES) {
            SEND(connfd, "ILLEGAL SIZE");
            ok = false;
            continue;
          }

#if BOARD_HAS_EEPROM
          struct bootloader_info info = {0};
          if (!ID_EEPROM_SaveBootloaderInfo(&info)) {
            SEND(connfd, ">Error clearing EEPROM");
            ok = false;
            continue;
          }
#endif

          EraseInitStruct.NbSectors =
              (image_size + FLASH_PAGE_SIZE_BYTES - 1) / FLASH_PAGE_SIZE_BYTES;
          chsnprintf(out_buffer, sizeof(out_buffer),
                     ">Erasing %u Flash Pages.\n", EraseInitStruct.NbSectors);
          write(connfd, out_buffer, strlen(out_buffer));

          SEND(connfd, "LENGTH OK\n");
          HAL_FLASH_Unlock();
          SEND(connfd, ">Erasing Chip...\n");
          EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
          EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
          EraseInitStruct.Banks = FLASH_BANK_1;
          EraseInitStruct.Sector = FLASH_SECTOR_1;

          uint32_t e = 0;
          ok &= HAL_FLASHEx_Erase(&EraseInitStruct, &e) == HAL_OK;
          if (!ok) {
            SEND(connfd, ">Error during Erase\n");
            continue;
          }

          SEND(connfd, ">Erasing Done\n");

          SEND(connfd, "SEND DATA\n");
          buffer_fill = 0;
          received_bytes = 0;
          sha256_init(&sha256);
          state = IMAGE_DATA;
        } break;
        case IMAGE_DATA:
          // Read 32 bytes at a time (except for when the buffer ends)
          if (buffer_fill < 32 &&
              (buffer_fill + received_bytes) != image_size) {
            continue;
          }
          received_bytes += buffer_fill;
          sha256_update(&sha256, buffer, buffer_fill);
          // zeropad, if we're in the last block of data
          while (buffer_fill < 32) {
            buffer[buffer_fill++] = 0;
          }

          if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, flash_address,
                                (uint32_t)(&buffer)) != HAL_OK) {
            SEND(connfd, ">Flash Program Error!\n");
            ok = false;
            continue;
          }
          flash_address += 32;
          buffer_fill = 0;
          if (received_bytes == image_size) {
            // done, break out
            break;
          }
          break;
        default:
          break;
      }
    }

    // Verify image
    if (ok) {
      HAL_FLASH_Lock();

      SEND(connfd, ">Verifying Hash");
      uint8_t validate_hash[32];
      memset(validate_hash, 0, sizeof(validate_hash));
      // This verifies the received bytes
      sha256_final(&sha256, validate_hash);
      if (memcmp(validate_hash, hash, sizeof(hash)) == 0) {
        SEND(connfd, ">Receive Verify OK\n");
      } else {
        SEND(connfd, ">Receive Verify Failed\n");
        ok = false;
      }
    }

    if (ok) {
      uint8_t validate_hash[32];
      // Now verify the actual written bytes
      sha256_init(&sha256);
      memset(validate_hash, 0, sizeof(validate_hash));
      sha256_update(&sha256, (uint8_t *)TARGET_FLASH_ADDRESS, image_size);
      sha256_final(&sha256, validate_hash);
      if (memcmp(validate_hash, hash, sizeof(hash)) == 0) {
        SEND(connfd, ">Flash Verify OK\n");
      } else {
        SEND(connfd, ">Flash Verify Failed\n");
        ok = false;
      }
    }

#if BOARD_HAS_EEPROM
    if (ok) {
      // Store bootloader info
      struct bootloader_info info = {0};
      info.image_present = 1;
      info.image_size = image_size;
      memcpy(info.image_sha256, hash, sizeof(hash));
      if (!ID_EEPROM_SaveBootloaderInfo(&info)) {
        SEND(connfd, ">Error storing EEPROM");
        ok = false;
        continue;
      }
    }
#endif

    if (ok) {
      // Allow reboot into user program
      locked = false;
      chMtxUnlock(&reboot_mutex);
    }

    // Sleep to allow TCP thread to send some data before closing the socket
    chThdSleep(TIME_MS2I(100));

    close(connfd);
  }
}

void InitBootloaderThread() {
  chThdCreateStatic(wa_bootloader_thread, sizeof(wa_bootloader_thread),
                    NORMALPRIO + 0, bootloader_thread, NULL);
}
