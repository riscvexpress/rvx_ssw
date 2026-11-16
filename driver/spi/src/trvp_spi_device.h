#ifndef __TRVP_SPI_DEVICE_H__
#define __TRVP_SPI_DEVICE_H__

#include <stdint.h>

#define SPI_DEVICE_MODE_FW 0
#define SPI_DEVICE_MODE_FLASH 1
#define SPI_DEVICE_MODE_PASSTHROUGH 2

#define SPI_DEVICE_SLOT_READ_STATUS1 0
#define SPI_DEVICE_SLOT_READ_STATUS2 1
#define SPI_DEVICE_SLOT_READ_STATUS3 2
#define SPI_DEVICE_SLOT_READ_JEDEC 3
#define SPI_DEVICE_SLOT_READ_SFDP 4
#define SPI_DEVICE_SLOT_READ_START 5
#define SPI_DEVICE_SLOT_READ_END 10
#define SPI_DEVICE_SLOT_RESERVED_START 11
#define SPI_DEVICE_NUM_SLOT 24

#define SPI_DEVICE_CMD_OPCODE(x) ((x)&0xFFu)
#define SPI_DEVICE_CMD_ADDR_CFG (1u<<8)
#define SPI_DEVICE_CMD_ADDR_3B (2u<<8)
#define SPI_DEVICE_CMD_ADDR_4B (3u<<8)
#define SPI_DEVICE_CMD_MBYTE (1u<<11)
#define SPI_DEVICE_CMD_DUMMY_CYCLES(n) ((1u<<15) | ((((n)-1)&7u)<<12))
#define SPI_DEVICE_CMD_PAYLOAD_EN(x) (((x)&0xFu)<<16)
#define SPI_DEVICE_CMD_PAYLOAD_OUT (1u<<20)
#define SPI_DEVICE_CMD_UPLOAD (1u<<24)
#define SPI_DEVICE_CMD_BUSY (1u<<25)
#define SPI_DEVICE_CMD_VALID (1u<<31)

#define SPI_DEVICE_INTR_UPLOAD_CMDFIFO_NOT_EMPTY (1u<<0)
#define SPI_DEVICE_INTR_UPLOAD_PAYLOAD_NOT_EMPTY (1u<<1)
#define SPI_DEVICE_INTR_UPLOAD_PAYLOAD_OVERFLOW (1u<<2)
#define SPI_DEVICE_INTR_READBUF_WATERMARK (1u<<3)
#define SPI_DEVICE_INTR_READBUF_FLIP (1u<<4)

#define SPI_DEVICE_FLASH_STATUS_BUSY (1u<<0)
#define SPI_DEVICE_FLASH_STATUS_WEL (1u<<1)

#define SPI_DEVICE_UPLOAD_CMD_OPCODE(x) ((x)&0xFFu)
#define SPI_DEVICE_UPLOAD_CMD_BUSY (1u<<13)
#define SPI_DEVICE_UPLOAD_CMD_WEL (1u<<14)
#define SPI_DEVICE_UPLOAD_CMD_ADDR4B (1u<<15)

#define SPI_DEVICE_READBUF_SIZE 0x800
#define SPI_DEVICE_MAILBOX_SIZE 0x400
#define SPI_DEVICE_SFDP_SIZE 0x100
#define SPI_DEVICE_PAYLOAD_SIZE 0x100

void spi_device_set_mode(unsigned int base, int mode);
void spi_device_set_jedec_id(unsigned int base, uint8_t manufacturer, uint16_t device, uint8_t cc, uint8_t num_cc);
void spi_device_set_cmd_info(unsigned int base, int slot, unsigned int info);
void spi_device_set_mailbox(unsigned int base, unsigned int spi_addr, int enable);
void spi_device_set_addr_4b(unsigned int base, int enable);
void spi_device_set_read_threshold(unsigned int base, unsigned int threshold);

void spi_device_write_readbuf(unsigned int base, unsigned int offset, int num_bytes, const unsigned char *data);
void spi_device_write_mailbox(unsigned int base, unsigned int offset, int num_bytes, const unsigned char *data);
void spi_device_write_sfdp(unsigned int base, unsigned int offset, int num_bytes, const unsigned char *data);
void spi_device_read_payload(unsigned int base, unsigned int offset, int num_bytes, unsigned char *data);

unsigned int spi_device_get_flash_status(unsigned int base);
void spi_device_set_flash_status(unsigned int base, unsigned int status);

int spi_device_upload_cmd_available(unsigned int base);
unsigned int spi_device_pop_upload_cmd(unsigned int base);
int spi_device_upload_addr_available(unsigned int base);
unsigned int spi_device_pop_upload_addr(unsigned int base);
int spi_device_get_payload_size(unsigned int base);
int spi_device_get_payload_start(unsigned int base);

unsigned int spi_device_get_interrupt(unsigned int base);
void spi_device_clear_interrupt(unsigned int base, unsigned int mask);
void spi_device_enable_interrupt(unsigned int base, unsigned int mask);

#endif
