// ****************************************************************************
// Copyright SoC Design Research Group, All rights reserved.    
// Electronics and Telecommunications Research Institute (ETRI)
//
// THESE DOCUMENTS CONTAIN CONFIDENTIAL INFORMATION AND KNOWLEDGE 
// WHICH IS THE PROPERTY OF ETRI. NO PART OF THIS PUBLICATION IS 
// TO BE USED FOR ANY OTHER PURPOSE, AND THESE ARE NOT TO BE 
// REPRODUCED, COPIED, DISCLOSED, TRANSMITTED, STORED IN A RETRIEVAL 
// SYSTEM OR TRANSLATED INTO ANY OTHER HUMAN OR COMPUTER LANGUAGE, 
// IN ANY FORM, BY ANY MEANS, IN WHOLE OR IN PART, WITHOUT THE 
// COMPLETE PRIOR WRITTEN PERMISSION OF ETRI.
// ****************************************************************************
// 8.May.2017 
// Sukho Lee (shle99@etri.re.kr)
// ****************************************************************************
// ****************************************************************************
#include "ervp_assert.h"
#include "ervp_misc_util.h"
#include "ervp_mmio_util.h"
#include "ervp_printf.h"
#include "ervp_delay.h"
#include "ervp_external_peri_group_memorymap_offset.h"
#include "ervp_external_peri_group_api.h"
#include "ervp_platform_controller_api.h"

#include "platform_info.h"
#include "flash_info.h"
#include "trvp_spi_flash.h"
#include "trvp_spi.h"

#ifdef FLASH_SPI_FREQ_HZ
#define SPI_FREQ_OF_FLASH		FLASH_SPI_FREQ_HZ
#else
#define SPI_FREQ_OF_FLASH		1000000
#endif
#define SPI_MODE_OF_FLASH			SPI_SCKMODE_3

#ifdef SPI_INDEX_FOR_SPI_FLASH
static SpiHostConfig spi_flash_spi_config = {(1<<SPI_INDEX_FOR_SPI_FLASH), SPI_DIVSOR(SPI_FREQ_OF_FLASH), SPI_MODE_OF_FLASH, 0};
#else
static SpiHostConfig spi_flash_spi_config = {0};
#endif

void spi_flash_configure(void)
{
	static int is_first = 1;
	if(is_first)
	{
		unsigned char id;
		spi_host_configure(&spi_flash_spi_config);
		// bug workaround: the first command after FPGA configuration reads invalid data
		spi_flash_read_id(1, &id);
		is_first = 0;
	}
#ifdef FLASH_IS_QSPI
	spi_flash_spi_config.enable_quad = spi_flash_enable_quad;
#else
	spi_flash_spi_config.enable_quad = 0;
#endif
	spi_host_configure(&spi_flash_spi_config);
}

#define FLASH_ADDR_CMD_SIZE 5

static int spi_flash_quad_dummy_bytes = 1;

static void make_flash_addr_cmd(unsigned char *buffer, unsigned char inst, unsigned int flash_addr)
{
	buffer[0] = inst;
	buffer[1] = (flash_addr>>24) & 0xFF;
	buffer[2] = (flash_addr>>16) & 0xFF;
	buffer[3] = (flash_addr>>8) & 0xFF;
	buffer[4] = flash_addr & 0xFF;
}

static unsigned char read_status_reg(void)
{
	unsigned char buffer[4];
	buffer[0] = SPI_FLASH_INST_RDSR;

	spi_host_start();
	spi_host_transmit(1, buffer);

	spi_host_receive(1, buffer);
	spi_host_end();

	return buffer[0];
}

void spi_flash_read(unsigned int flash_addr, int num_bytes, unsigned char *rdata) 
{
	unsigned char buffer[FLASH_ADDR_CMD_SIZE];
	int num_reads;

	while(num_bytes>0)
	{
		if(num_bytes > SPI_FLASH_PAGE_SIZE)
			num_reads = SPI_FLASH_PAGE_SIZE;
		else
			num_reads = num_bytes;

		if(spi_flash_spi_config.enable_quad)
		{
			make_flash_addr_cmd(buffer, SPI_FLASH_INST_4QOR, flash_addr);
			spi_host_read(FLASH_ADDR_CMD_SIZE, buffer, spi_flash_quad_dummy_bytes, num_reads, rdata, 1);
		}
		else
		{
			make_flash_addr_cmd(buffer, SPI_FLASH_INST_4READ, flash_addr);
			spi_host_read(FLASH_ADDR_CMD_SIZE, buffer, 0, num_reads, rdata, 0);
		}

		num_bytes -= num_reads;
		flash_addr += num_reads;
		rdata += num_reads;
	}
}

void spi_flash_program(unsigned int flash_addr, int num_bytes, unsigned char *wdata) 
{
	unsigned char buffer[FLASH_ADDR_CMD_SIZE];
	int num_writes;

	while(num_bytes>0)
	{
		num_writes = SPI_FLASH_PAGE_SIZE - (flash_addr % SPI_FLASH_PAGE_SIZE);
		if(num_writes > num_bytes)
			num_writes = num_bytes;
		buffer[0] = SPI_FLASH_INST_WREN;
		spi_host_start();
		spi_host_transmit(1, buffer);	// write enable
		spi_host_end();

		// page program	(max write size: 256 bytes(page size))
		if(spi_flash_spi_config.enable_quad)
		{
			make_flash_addr_cmd(buffer, SPI_FLASH_INST_4QPP, flash_addr);
			spi_host_write(FLASH_ADDR_CMD_SIZE, buffer, num_writes, wdata, 1);
		}
		else
		{
			make_flash_addr_cmd(buffer, SPI_FLASH_INST_4PP, flash_addr);
			spi_host_write(FLASH_ADDR_CMD_SIZE, buffer, num_writes, wdata, 0);
		}

		while((read_status_reg() & SPI_FLASH_STATUS_WIP) == SPI_FLASH_STATUS_WIP)
			delay_unit(100);
		num_bytes -= num_writes;
		flash_addr += num_writes;
		wdata += num_writes;
	}
}

void spi_flash_erase_sector(unsigned int flash_addr)
{
	// erase flash
	unsigned char buffer[FLASH_ADDR_CMD_SIZE];
	buffer[0] = SPI_FLASH_INST_WREN;
	spi_host_start();
	spi_host_transmit(1, buffer);	// Write enable
	spi_host_end();

	make_flash_addr_cmd(buffer, SPI_FLASH_INST_4SE, flash_addr);
	spi_host_start();
	spi_host_transmit(FLASH_ADDR_CMD_SIZE, buffer);
	spi_host_end();

	while((read_status_reg() & SPI_FLASH_STATUS_WIP) == SPI_FLASH_STATUS_WIP)
		delay_unit(1000);
}

void spi_flash_read_id(int num_bytes, unsigned char *id)
{
	const unsigned char inst = SPI_FLASH_INST_RDID;
	spi_host_read(1, &inst, 0, num_bytes, id, 0);
}

__attribute__((weak)) void spi_flash_enable_quad(void)
{
	const unsigned char rdcr = SPI_FLASH_INST_RDCR;
	const unsigned char wren = SPI_FLASH_INST_WREN;
	static int checked = 0;
	unsigned char buffer[3];
	unsigned char manufacturer;

	if(checked)
		return;
	checked = 1;

	spi_flash_read_id(1, &manufacturer);
	if(manufacturer == SPI_FLASH_MANUFACTURER_SPANSION)
	{
		spi_host_read(1, &rdcr, 0, 1, &buffer[2], 0);
		spi_flash_quad_dummy_bytes = ((buffer[2] & SPI_FLASH_CONFIG_LC_MASK)==SPI_FLASH_CONFIG_LC_NO_DUMMY)? 0 : 1;
		if((buffer[2] & SPI_FLASH_CONFIG_QUAD)==0)
		{
			buffer[0] = SPI_FLASH_INST_WRR;
			buffer[1] = read_status_reg();
			buffer[2] |= SPI_FLASH_CONFIG_QUAD;

			spi_host_write(1, &wren, 0, 0, 0);
			spi_host_write(3, buffer, 0, 0, 0);

			while((read_status_reg() & SPI_FLASH_STATUS_WIP) == SPI_FLASH_STATUS_WIP)
				delay_unit(1000);

			spi_host_read(1, &rdcr, 0, 1, &buffer[2], 0);
			assert_must_msg((buffer[2] & SPI_FLASH_CONFIG_QUAD)!=0, "flash quad mode is not enabled");
		}
	}
	else
	{
		assert_must_msg(0, "unsupported flash manufacturer");
	}
}

void spi_flash_print_id(void)
{
	int i;
	unsigned char buffer[4];
	spi_flash_read_id(3, buffer);

	for(i=0;i<3;i++)
		printf("flash id [%d]: 0x%x\n", i, buffer[i]);
}
