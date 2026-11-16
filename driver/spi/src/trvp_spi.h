// See LICENSE for license details.

#ifndef __TRVP_SPI_H__
#define __TRVP_SPI_H__

#include <stdint.h>
#include <string.h>

#define SPI_SCKMODE_0 0 /* Start: Low, Data placing: Falling edge, Data sampling: Rising edge */
#define SPI_SCKMODE_1 1 /* Start: Low, Data placing: Rising edge, Data sampling: Falling edge */
#define SPI_SCKMODE_2 2 /* Start: High, Data placing: Rising edge, Data sampling: Falling edge */
#define SPI_SCKMODE_3 3 /* Start: High, Data placing: Falling edge, Data sampling: Rising edge */

#define SPI_CS_ACTIVE_LOW 0
#define SPI_CS_ACTIVE_HIGH 1

#define SPI_DIVSOR(freq) ((SPI_CLK_HZ + 2 * (freq) - 1) / (2 * (freq)) - 1)

typedef struct
{
	uint8_t select; // onehot
	uint16_t divisor;
	uint8_t sck_mode;
	uint8_t cs_polarity;
	void (*enable_quad)(void);
} SpiHostConfig;

static inline void spi_host_config_init(SpiHostConfig *config)
{
	memset(config, 0, sizeof(SpiHostConfig));
}

void print_spi_host_config(const SpiHostConfig *config);
void print_spi_host_status(void);
void spi_host_configure(const SpiHostConfig *config);
void spi_host_store_config(SpiHostConfig *config);
void spi_host_restore_config(const SpiHostConfig *config);
void spi_host_start();
void spi_host_end();
void spi_host_receive(int num_bytes, unsigned char *data);
void spi_host_transmit(int num_bytes, const unsigned char *data);
void spi_host_transceive(int num_bytes, const unsigned char *tx_data, unsigned char *rx_data);
void spi_host_receive_qspi(int num_bytes, unsigned char *data);
void spi_host_transmit_qspi(int num_bytes, const unsigned char *data);
void spi_host_read(int num_header_bytes, const unsigned char *header, int num_dummy_bytes, int num_bytes, unsigned char *data, int use_qspi);
void spi_host_write(int num_header_bytes, const unsigned char *header, int num_bytes, const unsigned char *data, int use_qspi);

#endif
