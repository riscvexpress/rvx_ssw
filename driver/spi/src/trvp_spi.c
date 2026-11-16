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
// 13.Apr.2017 
// Sukho Lee (shle99@etri.re.kr)
// ****************************************************************************
// ****************************************************************************
#include "ervp_misc_util.h"
#include "ervp_mmio_util.h"
#include "ervp_uart.h"
#include "trvp_spi.h"
#include "ervp_printf.h"
#include "ervp_multicore_synch.h"
#include "ervp_external_peri_group_memorymap.h"

#define SPI_BASEADDR            (EXTERNAL_PERI_GROUP_BASEADDR + SUBMODULE_ADDR_ERVP_EXTERNAL_PERI_GROUP_SPI)

#define SPI_REG_CONTROL         (0x10 + SPI_BASEADDR)
#define SPI_REG_STATUS          (0x14 + SPI_BASEADDR)
#define SPI_REG_CONFIGOPTS(i)   (0x18 + 4*(i) + SPI_BASEADDR)
#define SPI_REG_CSID            (0x38 + SPI_BASEADDR)
#define SPI_REG_COMMAND         (0x3C + SPI_BASEADDR)
#define SPI_REG_RXDATA          (0x40 + SPI_BASEADDR)
#define SPI_REG_TXDATA          (0x44 + SPI_BASEADDR)

#define SPI_CONTROL_SPIEN       (1u<<31)
#define SPI_CONTROL_SW_RST      (1u<<30)
#define SPI_CONTROL_OUTPUT_EN   (1u<<29)

#define SPI_STATUS_READY        (1u<<31)
#define SPI_STATUS_ACTIVE       (1u<<30)
#define SPI_STATUS_TXFULL       (1u<<29)
#define SPI_STATUS_TXEMPTY      (1u<<28)
#define SPI_STATUS_RXEMPTY      (1u<<24)

#define SPI_CONFIGOPTS_CPOL     (1u<<31)
#define SPI_CONFIGOPTS_CPHA     (1u<<30)
#define SPI_CONFIGOPTS_CLKDIV   (0xFFFFu)

#define SPI_COMMAND_CSAAT       (1u<<9)
#define SPI_COMMAND_QUAD        (2u<<10)
#define SPI_COMMAND_RX          (1u<<12)
#define SPI_COMMAND_TX          (2u<<12)
#define SPI_COMMAND_BIDIR       (3u<<12)

#define SPI_MAX_SEGMENT_BYTES   512

void print_spi_host_config(const SpiHostConfig *config)
{
	char bitvector[33];
	printf("\ndivisor: %u", config->divisor);
	printf("\nsck_mode: %u", config->sck_mode);
	convert_value_to_bitvector(config->select, bitvector);
	printf("\nselect: %s", bitvector);
	printf("\ncs_polarity: %u", config->cs_polarity);
	printf("\n");
}

void print_spi_host_status(void)
{
	char bitvector[33];
	SpiHostConfig config;
	spi_host_store_config(&config);
	printf("\ndivisor: %u", config.divisor);
	printf("\nsck_mode: %u", config.sck_mode);
	convert_value_to_bitvector(config.select, bitvector);
	printf("\nselect: %s", bitvector);
	convert_value_to_bitvector(REG32(MMAP_SPIO_SPI_CS_POLARITY), bitvector);
	printf("\ncs_polarity_list: %s", bitvector);
	printf("\n");
}

static int spi_is_initialized = 0;
static int spi_cs_hold = 0;

static inline void __spi_init_once()
{
	REG32(SPI_REG_CONTROL) = SPI_CONTROL_SPIEN | SPI_CONTROL_OUTPUT_EN;
	REG32(SPI_REG_CSID) = 0x0;
}

static inline void __spi_wait_status(unsigned int mask, unsigned int value)
{
	while((REG32(SPI_REG_STATUS)&mask)!=value)
		IDLE;
}

static inline unsigned int __spi_csaat()
{
	return spi_cs_hold? SPI_COMMAND_CSAAT : 0;
}

static inline void __spi_push_next(const unsigned char** data, int* num_left)
{
	int i;
	int num_bytes = (*num_left > 4)? 4 : *num_left;
	unsigned int word = 0;
	if(*data != 0)
	{
		for(i=0; i<num_bytes; i++)
			word |= ((unsigned int)(*data)[i]) << (8*i);
		*data += num_bytes;
	}
	__spi_wait_status(SPI_STATUS_TXFULL, 0);
	REG32(SPI_REG_TXDATA) = word;
	*num_left -= num_bytes;
}

static inline void __spi_pop_next(unsigned char** data, int* num_left)
{
	int i;
	int num_bytes = (*num_left > 4)? 4 : *num_left;
	unsigned int word;
	__spi_wait_status(SPI_STATUS_RXEMPTY, 0);
	word = REG32(SPI_REG_RXDATA);
	if(*data != 0)
	{
		for(i=0; i<num_bytes; i++)
			(*data)[i] = word >> (8*i);
		*data += num_bytes;
	}
	*num_left -= num_bytes;
}

static void __spi_transfer(unsigned int command, int num_bytes, const unsigned char* tx_data, unsigned char* rx_data)
{
	int has_tx = (command & SPI_COMMAND_TX) != 0;
	int has_rx = (command & SPI_COMMAND_RX) != 0;
	int num_segment_bytes, tx_left, rx_left;

	while(num_bytes > 0)
	{
		num_segment_bytes = (num_bytes > SPI_MAX_SEGMENT_BYTES)? SPI_MAX_SEGMENT_BYTES : num_bytes;
		num_bytes -= num_segment_bytes;
		__spi_wait_status(SPI_STATUS_READY, SPI_STATUS_READY);
		REG32(SPI_REG_COMMAND) = command | (num_segment_bytes-1) | ((num_bytes > 0)? SPI_COMMAND_CSAAT : __spi_csaat());

		tx_left = has_tx? num_segment_bytes : 0;
		rx_left = has_rx? num_segment_bytes : 0;
		if(tx_left > 0)
			__spi_push_next(&tx_data, &tx_left);
		while((tx_left > 0) || (rx_left > 0))
		{
			if(tx_left > 0)
				__spi_push_next(&tx_data, &tx_left);
			if(rx_left > 0)
				__spi_pop_next(&rx_data, &rx_left);
		}
	}
}

void spi_host_configure(const SpiHostConfig *config)
{
	unsigned int value, temp;
	if(spi_is_initialized==0)
	{
		__spi_init_once();
		spi_is_initialized = 1;
	}
	if(config->cs_polarity==SPI_CS_ACTIVE_HIGH)
	{
		temp = config->select;
		value = REG32(MMAP_SPIO_SPI_CS_POLARITY);
		value = value | temp;
	}
	else
	{
		temp = ~config->select;
		value = REG32(MMAP_SPIO_SPI_CS_POLARITY);
		value = value & temp;
	}
	REG32(MMAP_SPIO_SPI_CS_POLARITY) = value;
	spi_host_restore_config(config);
	if(config->enable_quad)
		config->enable_quad();
}

void spi_host_store_config(SpiHostConfig *config)
{
	unsigned int configopts = REG32(SPI_REG_CONFIGOPTS(REG32(SPI_REG_CSID)));
	spi_host_config_init(config);
	config->divisor = configopts & SPI_CONFIGOPTS_CLKDIV;
	config->sck_mode = ((configopts&SPI_CONFIGOPTS_CPOL)? 2 : 0) | ((configopts&SPI_CONFIGOPTS_CPHA)? 1 : 0);
	config->select = 1 << REG32(SPI_REG_CSID);
	config->cs_polarity = (REG32(MMAP_SPIO_SPI_CS_POLARITY)&config->select)? SPI_CS_ACTIVE_HIGH : SPI_CS_ACTIVE_LOW;
}

void spi_host_restore_config(const SpiHostConfig *config)
{
	unsigned int configopts = config->divisor & SPI_CONFIGOPTS_CLKDIV;
	if(config->sck_mode&2)
		configopts |= SPI_CONFIGOPTS_CPOL;
	if(config->sck_mode&1)
		configopts |= SPI_CONFIGOPTS_CPHA;
	if(config->select)
		REG32(SPI_REG_CSID) = __builtin_ctz(config->select);
	REG32(SPI_REG_CONFIGOPTS(REG32(SPI_REG_CSID))) = configopts;
}

void spi_host_start()
{
	spi_cs_hold = 1;
}

void spi_host_end()
{
	spi_cs_hold = 0;
	__spi_wait_status(SPI_STATUS_ACTIVE, 0);
	REG32(SPI_REG_CONTROL) = SPI_CONTROL_SPIEN | SPI_CONTROL_OUTPUT_EN | SPI_CONTROL_SW_RST;
	__spi_wait_status(SPI_STATUS_ACTIVE|SPI_STATUS_TXEMPTY|SPI_STATUS_RXEMPTY, SPI_STATUS_TXEMPTY|SPI_STATUS_RXEMPTY);
	REG32(SPI_REG_CONTROL) = SPI_CONTROL_SPIEN | SPI_CONTROL_OUTPUT_EN;
}

void spi_host_receive_qspi(int num_bytes, unsigned char* data)
{
	__spi_transfer(SPI_COMMAND_RX | SPI_COMMAND_QUAD, num_bytes, 0, data);
}

void spi_host_receive(int num_bytes, unsigned char* data)
{
	__spi_transfer(SPI_COMMAND_BIDIR, num_bytes, 0, data);
}

void spi_host_transmit(int num_bytes, const unsigned char* data)
{
	__spi_transfer(SPI_COMMAND_TX, num_bytes, data, 0);
	__spi_wait_status(SPI_STATUS_READY|SPI_STATUS_ACTIVE|SPI_STATUS_TXEMPTY, SPI_STATUS_READY|SPI_STATUS_TXEMPTY);
}

void spi_host_transmit_qspi(int num_bytes, const unsigned char* data)
{
	__spi_transfer(SPI_COMMAND_TX | SPI_COMMAND_QUAD, num_bytes, data, 0);
	__spi_wait_status(SPI_STATUS_READY|SPI_STATUS_ACTIVE|SPI_STATUS_TXEMPTY, SPI_STATUS_READY|SPI_STATUS_TXEMPTY);
}

void spi_host_transceive(int num_bytes, const unsigned char* tx_data, unsigned char* rx_data)
{
	__spi_transfer(SPI_COMMAND_BIDIR, num_bytes, tx_data, rx_data);
}

void spi_host_read(int num_header_bytes, const unsigned char* header, int num_dummy_bytes, int num_bytes, unsigned char* data, int use_qspi)
{
	spi_host_start();
	spi_host_transmit(num_header_bytes, header);
	spi_host_transmit(num_dummy_bytes, 0);
	if(use_qspi)
		spi_host_receive_qspi(num_bytes, data);
	else
		spi_host_receive(num_bytes, data);
	spi_host_end();
}

void spi_host_write(int num_header_bytes, const unsigned char* header, int num_bytes, const unsigned char* data, int use_qspi)
{
	spi_host_start();
	spi_host_transmit(num_header_bytes, header);
	if(use_qspi)
		spi_host_transmit_qspi(num_bytes, data);
	else
		spi_host_transmit(num_bytes, data);
	spi_host_end();
}
