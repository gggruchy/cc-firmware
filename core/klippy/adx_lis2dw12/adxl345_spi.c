#include "adxl345_spi.h"
#include "sunxi_gpio.h"
#include "debug.h"
static void pabort(const char *s)
{
	perror(s);
	abort();
}
static const char *device0 = "/dev/spidev0.0";
static const char *device1 = "/dev/spidev1.0";
static uint32_t mode = 0;
static uint8_t bits = 8;
static char *input_file;
static char *output_file;
// static uint32_t speed = 500000;
// static uint16_t delay;
static int verbose;
static int transfer_size;
static int iterations;
static int interval = 5; /* interval in seconds for showing transfer rate */

static pthread_mutex_t lock;

// uint8_t default_tx[] = {
// 	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 	0x40, 0x00, 0x00, 0x00, 0x00, 0x95,
// 	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
// 	0xF0, 0x0D,
// };

// uint8_t default_rx[ARRAY_SIZE(default_tx)] = {0, };
// char *input_tx;

static void hex_dump(const void *src, size_t length, size_t line_size,
		     char *prefix)
{
	int i = 0;
	const unsigned char *address = (const unsigned char *)src;
	const unsigned char *line = address;
	unsigned char c;

	printf("%s | ", prefix);
	while (length-- > 0) {
		printf("%02X ", *address++);
		if (!(++i % line_size) || (length == 0 && i % line_size)) {
			if (length == 0) {
				while (i++ % line_size)
					printf("__ ");
			}
			printf(" |");
			while (line < address) {
				c = *line++;
				printf("%c", (c < 32 || c > 126) ? '.' : c);
			}
			printf("|\n");
			if (length > 0)
				printf("%s | ", prefix);
		}
	}
}

/*
 *  Unescape - process hexadecimal escape character
 *      converts shell input "\x23" -> 0x23
 */
static int unescape(char *_dst, char *_src, size_t len)
{
	int ret = 0;
	int match;
	char *src = _src;
	char *dst = _dst;
	unsigned int ch;

	while (*src) {
		if (*src == '\\' && *(src+1) == 'x') {
			match = sscanf(src + 2, "%2x", &ch);
			if (!match)
			{
			//	pabort("malformed input string");
			}
			


			src += 4;
			*dst++ = (unsigned char)ch;
		} else {
			*dst++ = *src++;
		}
		ret++;
	}
	return ret;
}

int spi_transfer(int fd, uint32_t speed,uint8_t const *tx, uint8_t const *rx, size_t len, const char* adxl_name)
{
	pthread_mutex_lock(&lock);
#if SPI_GPIO_CS

	if(strcmp(adxl_name, "adxl345") || strcmp(adxl_name, "adxl345_x"))
	{
		cs0_l();
	}
	else
	{
		cs1_l();
	}
#endif
	int ret;
	int out_fd;
	struct spi_ioc_transfer tr = {
		.tx_buf = (__u64)tx,
		.rx_buf = (__u64)rx,
		.len = len,
		.speed_hz = speed,
		.delay_usecs = 0,
	
		.bits_per_word = bits,
	};
	static uint8_t last_rx_len = 0;
	static uint8_t total_rx_num = 0;
	// uint8_t cur_tx[9] = {0,};
	// for (int i = 0; i < len; i++)
	// {
	// 	cur_tx[i] = tx[i];
	// }
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret != len)
	{
		last_rx_len = 0;
		printf("rx len: %x len:%d",len,ret);
		for (int i = 0; i < len; i++)
		{
			printf("  %x ", rx[i]);
		}
		printf("\n");
		pthread_mutex_unlock(&lock);
		return -1;
	}
	else 
	{
		// static uint8_t last_rx[9] = {0,};
		// if(len == 9) total_rx_num++;
		// else total_rx_num = 0;

		// if(last_rx_len )
		// {
		// 	if(rx[0]!= last_rx[0])
		// 	{
		// 		printf("total_rx_num:%d last_rx_len: %x last_rx:",total_rx_num,last_rx_len);
		// 		for (int i = last_rx_len; i ; i--)
		// 		{
		// 			printf("  %x ", last_rx[i-1]);
		// 		}
		// 		printf("            rx len: %x rx:",len);
		// 		for (int i = 0; i < len; i++)
		// 		{
		// 			printf("  %x ", rx[i]);
		// 		}
		// 		// printf("             tx:");
		// 		// for (int i = 0; i < len; i++)
		// 		// {
		// 		// 	printf("  %x ", cur_tx[i]);
		// 		// }
		// 		printf("\n");
		// 		pthread_mutex_unlock(&lock);
		// 		return -2;
		// 	}
		// }
		// last_rx_len = len;
		// if(last_rx_len > 9) last_rx_len = 9;
		// for (int i = 0; i < last_rx_len; i++)
		// {
		// 	last_rx[i] = rx[last_rx_len-1-i];
		// }
	}
#if SPI_GPIO_CS
	if(strcmp(adxl_name, "adxl345_y"))
	{
		cs0_h();
	}
	else
	{
		cs1_h();
	}
#endif
	pthread_mutex_unlock(&lock);
	return 0;
}


// spi_master_status_t hal_spi_transfer_cbd(hal_spi_master_port_t port, void *buf, uint32_t size, uint8_t receive_data)
// {
//     spi_master_status_t ret;
//     hal_spi_master_transfer_t tr;

//     if (size == 9)
//     {
//         tr.tx_buf = buf;
//         tr.tx_len = 1;
//         tr.rx_buf = buf;
//         tr.rx_len = size - 1;
//         tr.rx_buf++;
//     }
//     else
//     {
//         tr.tx_buf = buf;
//         tr.tx_len = size;
//         tr.rx_buf = buf;
//         tr.rx_len = size;
//     }
    
//     tr.dummy_byte = 0;
//     tr.tx_single_len = size;
//     tr.tx_nbits = SPI_NBITS_SINGLE;
//     tr.rx_nbits = SPI_NBITS_SINGLE;
    
//     SPI_INFO("spi[%d] read data,len is %ld \n", port, size);
//     ret = hal_spi_xfer(port, &tr);
    
//     return ret;
// }


int spi_init(int device_id,uint32_t speed)
{
	pthread_mutex_init(&lock, NULL);
    int ret = 0;
	int fd;

	mode = SPI_CPHA;
	mode |= SPI_CPOL;
	char *device = device0;
	if(device_id == 1)
	{
		device = device1;
	}
	fd = open(device, O_RDWR);
	if (fd < 0)
	{
		GAM_DEBUG_printf ("can't open device%d:%s\n", device_id,device);
		return fd;
	}
		
	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
	{
		GAM_DEBUG_printf("can't set spi mode");
		return fd;
	}

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
	{
		GAM_DEBUG_printf("can't get spi mode:%d",mode);
		return fd;
	}

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		GAM_DEBUG_printf("can't get bits per word:%d",bits);
		return fd;
	}
	//	

	/*
	 * max speed hz
	 */
	printf("set speed: %d Hz (%d KHz)\n", speed, speed/1000);
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		GAM_DEBUG_printf("can't set max speed hz");
		return fd;
	}
	//	

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		GAM_DEBUG_printf("can't get max speed hz");
		return fd;
	}

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	return fd;
}
