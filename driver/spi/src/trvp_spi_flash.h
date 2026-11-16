// See LICENSE for license details.

#ifndef __TRVP_SPI_FLASH_H__
#define __TRVP_SPI_FLASH_H__

#define SPI_FLASH_INST_RDID		0x9f	// and 0x9e
#define SPI_FLASH_INST_READ		0x03
#define SPI_FLASH_INST_4READ		0x13
#define SPI_FLASH_INST_FAST_READ	0x0b
#define SPI_FLASH_INST_DOFR		0x3b
#define SPI_FLASH_INST_DIOFR		0xbb
#define SPI_FLASH_INST_QOFR		0x6b
#define SPI_FLASH_INST_4QOR		0x6c
#define SPI_FLASH_INST_QIOFR		0xeb
#define SPI_FLASH_INST_ROTP		0x4b
#define SPI_FLASH_INST_WREN		0x06
#define SPI_FLASH_INST_WRDI		0x04
#define SPI_FLASH_INST_PP		0x02
#define SPI_FLASH_INST_4PP		0x12
#define SPI_FLASH_INST_QIFP		0x32
#define SPI_FLASH_INST_4QPP		0x34
#define SPI_FLASH_INST_SE		0xd8
#define SPI_FLASH_INST_4SE		0xdc
#define SPI_FLASH_INST_BE		0xc7
#define SPI_FLASH_INST_RDSR		0x05
#define SPI_FLASH_INST_RDCR		0x35
#define SPI_FLASH_INST_WRR		0x01

/* Legacy SPI Status Register bits */
#define SPI_FLASH_STATUS_WIP		(1<<0)		/* Write in progress bit */
#define SPI_FLASH_CONFIG_QUAD		(1<<1)
#define SPI_FLASH_CONFIG_LC_MASK	(3<<6)
#define SPI_FLASH_CONFIG_LC_NO_DUMMY	(3<<6)

#define SPI_FLASH_MANUFACTURER_SPANSION	0x01

#define SPI_FLASH_PAGE_SIZE		0x100		// 256 bytes

void spi_flash_configure(void);
void spi_flash_read(unsigned int flash_addr, int num_bytes, unsigned char *rdata);
void spi_flash_program(unsigned int flash_addr, int num_bytes, unsigned char *wdata);
void spi_flash_erase_sector(unsigned int flash_addr);
void spi_flash_read_id(int num_bytes, unsigned char *id);
void spi_flash_enable_quad(void);
void spi_flash_print_id(void);

//void disable_flash();
//void spi_flash_read(void);
//void command_flash(unsigned char data1, unsigned char data2, unsigned char data3);

#endif
