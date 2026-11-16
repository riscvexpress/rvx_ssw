#include "ervp_mmio_util.h"
#include "ervp_assert.h"
#include "trvp_spi_device.h"

#define SPI_DEVICE_REG_INTR_STATE       0x00
#define SPI_DEVICE_REG_INTR_ENABLE      0x04
#define SPI_DEVICE_REG_CONTROL          0x10
#define SPI_DEVICE_REG_CFG              0x14
#define SPI_DEVICE_REG_ADDR_MODE        0x20
#define SPI_DEVICE_REG_FLASH_STATUS     0x28
#define SPI_DEVICE_REG_JEDEC_CC         0x2C
#define SPI_DEVICE_REG_JEDEC_ID         0x30
#define SPI_DEVICE_REG_READ_THRESHOLD   0x34
#define SPI_DEVICE_REG_MAILBOX_ADDR     0x38
#define SPI_DEVICE_REG_UPLOAD_STATUS    0x3C
#define SPI_DEVICE_REG_UPLOAD_STATUS2   0x40
#define SPI_DEVICE_REG_UPLOAD_CMDFIFO   0x44
#define SPI_DEVICE_REG_UPLOAD_ADDRFIFO  0x48
#define SPI_DEVICE_REG_CMD_INFO(i)      (0x7C + 4*(i))

#define SPI_DEVICE_READBUF_OFFSET       0x1000
#define SPI_DEVICE_MAILBOX_OFFSET       0x1800
#define SPI_DEVICE_SFDP_OFFSET          0x1C00
#define SPI_DEVICE_PAYLOAD_OFFSET       0x1E00

#define SPI_DEVICE_CONTROL_MODE_SHIFT   4
#define SPI_DEVICE_CONTROL_MODE_MASK    (3u<<4)
#define SPI_DEVICE_CFG_MAILBOX_EN       (1u<<24)
#define SPI_DEVICE_ADDR_MODE_4B_EN      (1u<<0)
#define SPI_DEVICE_ADDR_MODE_PENDING    (1u<<31)
#define SPI_DEVICE_UPLOAD_CMDFIFO_NOTEMPTY  (1u<<7)
#define SPI_DEVICE_UPLOAD_ADDRFIFO_NOTEMPTY (1u<<15)

#define SPI_DEVICE_REG(base, offset) REG32((base) + (offset))

static void __spi_device_write_bytes(unsigned int addr, unsigned int offset, int num_bytes, const unsigned char *data)
{
	int i, n;
	unsigned int word;
	assert_must((offset&3u)==0);
	while(num_bytes > 0)
	{
		n = (num_bytes > 4)? 4 : num_bytes;
		word = 0;
		for(i=0; i<n; i++)
			word |= ((unsigned int)data[i]) << (8*i);
		REG32(addr + offset) = word;
		data += n;
		offset += 4;
		num_bytes -= n;
	}
}

static void __spi_device_read_bytes(unsigned int addr, unsigned int offset, int num_bytes, unsigned char *data)
{
	unsigned int word = 0;
	unsigned int shift;
	int is_first = 1;
	while(num_bytes > 0)
	{
		shift = 8 * (offset & 3u);
		if((shift==0) || is_first)
			word = REG32(addr + (offset & ~3u));
		is_first = 0;
		*data = word >> shift;
		data++;
		offset++;
		num_bytes--;
	}
}

void spi_device_set_mode(unsigned int base, int mode)
{
	unsigned int value = SPI_DEVICE_REG(base, SPI_DEVICE_REG_CONTROL);
	value = (value & ~SPI_DEVICE_CONTROL_MODE_MASK) | ((((unsigned int)mode)<<SPI_DEVICE_CONTROL_MODE_SHIFT) & SPI_DEVICE_CONTROL_MODE_MASK);
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_CONTROL) = value;
}

void spi_device_set_jedec_id(unsigned int base, uint8_t manufacturer, uint16_t device, uint8_t cc, uint8_t num_cc)
{
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_JEDEC_CC) = cc | (((unsigned int)num_cc)<<8);
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_JEDEC_ID) = (((unsigned int)manufacturer)<<16) | device;
}

void spi_device_set_cmd_info(unsigned int base, int slot, unsigned int info)
{
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_CMD_INFO(slot)) = info;
}

void spi_device_set_mailbox(unsigned int base, unsigned int spi_addr, int enable)
{
	unsigned int value;
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_MAILBOX_ADDR) = spi_addr;
	value = SPI_DEVICE_REG(base, SPI_DEVICE_REG_CFG);
	if(enable)
		value |= SPI_DEVICE_CFG_MAILBOX_EN;
	else
		value &= ~SPI_DEVICE_CFG_MAILBOX_EN;
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_CFG) = value;
}

void spi_device_set_addr_4b(unsigned int base, int enable)
{
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_ADDR_MODE) = enable? SPI_DEVICE_ADDR_MODE_4B_EN : 0;
	while(SPI_DEVICE_REG(base, SPI_DEVICE_REG_ADDR_MODE) & SPI_DEVICE_ADDR_MODE_PENDING);
}

void spi_device_set_read_threshold(unsigned int base, unsigned int threshold)
{
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_READ_THRESHOLD) = threshold;
}

void spi_device_write_readbuf(unsigned int base, unsigned int offset, int num_bytes, const unsigned char *data)
{
	__spi_device_write_bytes(base + SPI_DEVICE_READBUF_OFFSET, offset, num_bytes, data);
}

void spi_device_write_mailbox(unsigned int base, unsigned int offset, int num_bytes, const unsigned char *data)
{
	__spi_device_write_bytes(base + SPI_DEVICE_MAILBOX_OFFSET, offset, num_bytes, data);
}

void spi_device_write_sfdp(unsigned int base, unsigned int offset, int num_bytes, const unsigned char *data)
{
	__spi_device_write_bytes(base + SPI_DEVICE_SFDP_OFFSET, offset, num_bytes, data);
}

void spi_device_read_payload(unsigned int base, unsigned int offset, int num_bytes, unsigned char *data)
{
	__spi_device_read_bytes(base + SPI_DEVICE_PAYLOAD_OFFSET, offset, num_bytes, data);
}

unsigned int spi_device_get_flash_status(unsigned int base)
{
	return SPI_DEVICE_REG(base, SPI_DEVICE_REG_FLASH_STATUS);
}

void spi_device_set_flash_status(unsigned int base, unsigned int status)
{
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_FLASH_STATUS) = status;
}

int spi_device_upload_cmd_available(unsigned int base)
{
	return (SPI_DEVICE_REG(base, SPI_DEVICE_REG_UPLOAD_STATUS) & SPI_DEVICE_UPLOAD_CMDFIFO_NOTEMPTY) != 0;
}

unsigned int spi_device_pop_upload_cmd(unsigned int base)
{
	return SPI_DEVICE_REG(base, SPI_DEVICE_REG_UPLOAD_CMDFIFO);
}

int spi_device_upload_addr_available(unsigned int base)
{
	return (SPI_DEVICE_REG(base, SPI_DEVICE_REG_UPLOAD_STATUS) & SPI_DEVICE_UPLOAD_ADDRFIFO_NOTEMPTY) != 0;
}

unsigned int spi_device_pop_upload_addr(unsigned int base)
{
	return SPI_DEVICE_REG(base, SPI_DEVICE_REG_UPLOAD_ADDRFIFO);
}

int spi_device_get_payload_size(unsigned int base)
{
	return SPI_DEVICE_REG(base, SPI_DEVICE_REG_UPLOAD_STATUS2) & 0x1FF;
}

int spi_device_get_payload_start(unsigned int base)
{
	return (SPI_DEVICE_REG(base, SPI_DEVICE_REG_UPLOAD_STATUS2) >> 16) & 0xFF;
}

unsigned int spi_device_get_interrupt(unsigned int base)
{
	return SPI_DEVICE_REG(base, SPI_DEVICE_REG_INTR_STATE);
}

void spi_device_clear_interrupt(unsigned int base, unsigned int mask)
{
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_INTR_STATE) = mask;
}

void spi_device_enable_interrupt(unsigned int base, unsigned int mask)
{
	SPI_DEVICE_REG(base, SPI_DEVICE_REG_INTR_ENABLE) = mask;
}
