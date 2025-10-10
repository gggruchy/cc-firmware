#include "common.h"
#include "ymodem.h"
#include "string.h"
#include "combus.h"
#include "uart_combus.h"
#include "hl_assert.h"
#include "utils.h"
#include "hl_common.h"
#include "uart_combus.h"
#define LOG_TAG "ymodem"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
#define IS_AF(c) ((c >= 'A') && (c <= 'F'))
#define IS_af(c) ((c >= 'a') && (c <= 'f'))
#define IS_09(c) ((c >= '0') && (c <= '9'))
#define ISVALIDHEX(c) IS_AF(c) || IS_af(c) || IS_09(c)
#define ISVALIDDEC(c) IS_09(c)
#define CONVERTDEC(c) (c - '0')

#define CONVERTHEX_alpha(c) (IS_AF(c) ? (c - 'A' + 10) : (c - 'a' + 10))
#define CONVERTHEX(c) (IS_09(c) ? (c - '0') : CONVERTHEX_alpha(c))
#define TIME_OUT 10000
typedef struct
{
	combus_t *bus;
	char filepath[1024];
	uint32_t file_size;
	FILE *fp;
	char dev_name[256];
} ymodem_ctx_t;
static uint16_t Cal_CRC16(const uint8_t *data, uint32_t size);
static uint32_t Str2Int(uint8_t *inputstr, int32_t *intnum);
static uint32_t Send_Byte(uint8_t c, ymodem_ctx_t *ctx);
static int32_t Receive_Byte(uint8_t *c, uint32_t timeout, ymodem_ctx_t *ctx);
static void Int2Str(uint8_t *str, int32_t intnum);
int ymodem_init(user_ymodem_ctx_t *ctx, const char *dev_name, const char *filepath, uint32_t baudrate)
{
	HL_ASSERT(ctx != NULL);
	HL_ASSERT(filepath != NULL);
	HL_ASSERT(dev_name != NULL);
	// uart_combus_params_t params = {0};
	// params.baudrate = baudrate;
	ymodem_ctx_t *c = (ymodem_ctx_t *)malloc(sizeof(ymodem_ctx_t));
	if (c == NULL)
		return -1;
	memset(c, 0, sizeof(ymodem_ctx_t));
	c->bus = (combus_t *)malloc(sizeof(combus_t));
	if (c->bus == NULL)
	{
		free(c);
		return -1;
	}
	memset(c->bus, 0, sizeof(combus_t));
	combus_attach_drvier(c->bus, &uart_combus_ops);
	if (combus_open(c->bus, dev_name, (void *)baudrate) != 0)
	{
		free(c->bus);
		free(c);
		return -2;
	}
	c->fp = fopen(filepath, "rb");
	if (c->fp == NULL)
	{
		Send_Byte(CA, c);
		Send_Byte(CA, c);
		combus_close(c->bus);
		free(c->bus);
		free(c);
		return -3;
	}
	strcpy(c->filepath, filepath);
	c->file_size = utils_get_size(c->filepath);
	strncpy(c->dev_name, dev_name, sizeof(c->dev_name) - 1);
	*ctx = c;
	return 0;
}
int ymodem_deint(user_ymodem_ctx_t *ctx)
{
	HL_ASSERT(ctx != NULL);
	ymodem_ctx_t *c = (ymodem_ctx_t *)*ctx;
	combus_close(c->bus);
	fclose(c->fp);
	free(c->bus);
	free(c);
	*ctx = NULL;
	return 0;
}
int path_to_file_name(const char *path, char *file_name)
{
	int i = 0;
	int len = strlen(path);
	for (i = len - 1; i >= 0; i--)
	{
		if (path[i] == '/')
		{
			break;
		}
	}
	if (i < 0)
	{
		return -1;
	}
	strcpy(file_name, path + i + 1);
	return 0;
}
void Int2Str(uint8_t *str, int32_t intnum)
{
	uint32_t i, Div = 1000000000, j = 0, Status = 0;

	for (i = 0; i < 10; i++)
	{
		str[j++] = (intnum / Div) + 48;

		intnum = intnum % Div;
		Div /= 10;
		if ((str[j - 1] == '0') & (Status == 0))
		{
			j = 0;
		}
		else
		{
			Status++;
		}
	}
}
uint32_t Str2Int(uint8_t *inputstr, int32_t *intnum)
{
	uint32_t i = 0, res = 0;
	uint32_t val = 0;

	if (inputstr[0] == '0' && (inputstr[1] == 'x' || inputstr[1] == 'X'))
	{
		if (inputstr[2] == '\0')
		{
			return 0;
		}

		for (i = 2; i < 11; i++)
		{
			if (inputstr[i] == '\0')
			{
				*intnum = val;
				/* return 1; */
				res = 1;
				break;
			}

			if (ISVALIDHEX(inputstr[i]))
			{
				val = (val << 4) + CONVERTHEX(inputstr[i]);
			}
			else
			{
				/* Return 0, Invalid input */
				res = 0;
				break;
			}
		}
		/* Over 8 digit hex --invalid */
		if (i >= 11)
		{
			res = 0;
		}
	}
	else /* max 10-digit decimal input */
	{
		for (i = 0; i < 11; i++)
		{
			if (inputstr[i] == '\0')
			{
				*intnum = val;
				/* return 1 */
				res = 1;
				break;
			}
			else if ((inputstr[i] == 'k' || inputstr[i] == 'K') && (i > 0))
			{
				val = val << 10;
				*intnum = val;
				res = 1;
				break;
			}
			else if ((inputstr[i] == 'm' || inputstr[i] == 'M') && (i > 0))
			{
				val = val << 20;
				*intnum = val;
				res = 1;
				break;
			}
			else if (ISVALIDDEC(inputstr[i]))
			{
				val = val * 10 + CONVERTDEC(inputstr[i]);
			}
			else
			{
				/* return 0, Invalid input */
				res = 0;
				break;
			}
		}
		/* Over 10 digit decimal --invalid */
		if (i >= 11)
		{
			res = 0;
		}
	}

	return res;
}
static int32_t Receive_Byte(uint8_t *c, uint32_t timeout, ymodem_ctx_t *ctx)
{
	volatile uint64_t start_tick_ms = hl_get_tick_ms();
	while (hl_get_tick_ms() - start_tick_ms < timeout)
	{
		if (read(ctx->bus->fd, c, 1) == 1)
		{
			// LOG_D("%s Receive_Byte %02x\n", ctx->dev_name, *c);
			return 0;
		}
	}
	return -1;
}
static uint32_t Send_Byte(uint8_t c, ymodem_ctx_t *ctx)
{
	if (write(ctx->bus->fd, &c, 1) == 1)
	{
		return 0;
	}
	return -1;
}

/*
*********************************************************************************************************
*	函 数 名: Ymodem_PrepareIntialPacket
*	功能说明: 准备第一包要发送的数据
*	形    参: data 数据
*             fileName 文件名
*             length   文件大小
*	返 回 值: 0
*********************************************************************************************************
*/
void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t *fileName, uint32_t *length)
{
	uint16_t i, j;
	uint8_t file_ptr[FILE_SIZE_LENGTH] = {0};

	/* 第一包数据的前三个字符  */
	data[0] = SOH; /* soh表示数据包是128字节 */
	data[1] = 0x00;
	data[2] = 0xff;

	/* 文件名 */
	for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH); i++)
	{
		data[i + PACKET_HEADER] = fileName[i];
	}

	data[i + PACKET_HEADER] = 0x00;

	/* 文件大小转换成字符 */
	Int2Str(file_ptr, *length);
	for (j = 0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0';)
	{
		data[i++] = file_ptr[j++];
	}

	/* 其余补0 */
	for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
	{
		data[j] = 0;
	}
	LOG_D("Ymodem_PrepareIntialPacket %s %d\n", fileName, *length);
}

/*
*********************************************************************************************************
*	函 数 名: Ymodem_PreparePacket
*	功能说明: 准备发送数据包
*	形    参: SourceBuf 要发送的原数据
*             data      最终要发送的数据包，已经包含的头文件和原数据
*             pktNo     数据包序号
*             sizeBlk   要发送数据数
*	返 回 值: 无
*********************************************************************************************************
*/
void Ymodem_PreparePacket(uint8_t *SourceBuf, uint8_t *data, uint8_t pktNo, uint32_t sizeBlk)
{
	uint16_t i, size, packetSize;
	uint8_t *file_ptr;

	/* 设置好要发送数据包的前三个字符data[0]，data[1]，data[2] */
	/* 根据sizeBlk的大小设置数据区数据个数是取1024字节还是取128字节*/
	packetSize = sizeBlk > PACKET_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;
	/* 数据大小进一步确定 */
	size = sizeBlk < packetSize ? sizeBlk : packetSize;

	/* 首字节：确定是1024字节还是用128字节 */
	if (packetSize == PACKET_1K_SIZE)
	{
		data[0] = STX;
	}
	else
	{
		data[0] = SOH;
	}

	/* 第2个字节：数据序号 */
	data[1] = pktNo;
	/* 第3个字节：数据序号取反 */
	data[2] = (~pktNo);
	file_ptr = SourceBuf;

	/* 填充要发送的原始数据 */
	for (i = PACKET_HEADER; i < size + PACKET_HEADER; i++)
	{
		data[i] = *file_ptr++;
	}

	/* 不足的补 EOF (0x1A) 或 0x00 */
	if (size <= packetSize)
	{
		for (i = size + PACKET_HEADER; i < packetSize + PACKET_HEADER; i++)
		{
			data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: UpdateCRC16
*	功能说明: 上次计算的CRC结果 crcIn 再加上一个字节数据计算CRC
*	形    参: crcIn 上一次CRC计算结果
*             byte  新添加字节
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
	uint32_t crc = crcIn;
	uint32_t in = byte | 0x100;

	do
	{
		crc <<= 1;
		in <<= 1;
		if (in & 0x100)
			++crc;
		if (crc & 0x10000)
			crc ^= 0x1021;
	} while (!(in & 0x10000));

	return crc & 0xffffu;
}

static uint16_t Cal_CRC16(const uint8_t *data, uint32_t size)
{
	uint32_t crc = 0;
	const uint8_t *dataEnd = data + size;

	while (data < dataEnd)
		crc = UpdateCRC16(crc, *data++);

	crc = UpdateCRC16(crc, 0);
	crc = UpdateCRC16(crc, 0);

	return crc & 0xffffu;
}

/*
*********************************************************************************************************
*	函 数 名: CalChecksum
*	功能说明: 计算一串数据总和
*	形    参: data  数据
*             size  数据长度
*	返 回 值: 计算结果的后8位
*********************************************************************************************************
*/
uint8_t CalChecksum(const uint8_t *data, uint32_t size)
{
	uint32_t sum = 0;
	const uint8_t *dataEnd = data + size;

	while (data < dataEnd)
		sum += *data++;

	return (sum & 0xffu);
}
int Ymodem_SendPacket(uint8_t *data, uint16_t length, ymodem_ctx_t *ctx)
{
	ssize_t ret = 0;
	if (data == NULL || ctx == NULL)
		return -1;
	// printf("%s Ymodem_SendPacket %d\n", ctx->dev_name, length);
	// for (int i = 0; i < length; i++)
	// {
	// 	printf("%02x ", data[i]);
	// }
	// printf("\n");
	ret = write(ctx->bus->fd, data, length);
	if (ret != length)
	{
		LOG_E("write failed %d %d\n", ret, length);
		return -1;
	}
	return ret;
}

uint8_t ymodem_transmit(user_ymodem_ctx_t *user_ctx)
{
	uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD] = {0};
	uint8_t filename[FILE_NAME_LENGTH] = {0};
	uint8_t buf_ptr[PACKET_1K_SIZE + PACKET_OVERHEAD] = {0};
	uint8_t tempCheckSum = 0;
	uint16_t tempCRC = 0;
	uint16_t blkNumber = 0;
	uint8_t receivedC[2] = {0};
	uint32_t errors = 0, ackReceived = 0, size = 0, pktSize = 0;
	uint32_t ca_stop_cnt = 0;
	ymodem_ctx_t *ctx = (ymodem_ctx_t *)(*user_ctx);
	path_to_file_name(ctx->filepath, filename);

	/* 初始化要发送的第一个数据包 */
	Ymodem_PrepareIntialPacket(&packet_data[0], filename, &ctx->file_size);
	tempCRC = Cal_CRC16(&packet_data[3], PACKET_SIZE);
	packet_data[PACKET_SIZE + PACKET_HEADER] = tempCRC >> 8;
	packet_data[PACKET_SIZE + PACKET_HEADER + 1] = tempCRC & 0xFF;

	do // 起始包
	{
		/* 发送数据包 */
		Ymodem_SendPacket(packet_data, PACKET_SIZE + PACKET_OVERHEAD, ctx);

		/* 等待 Ack 和字符 'C' */
		if (Receive_Byte(&receivedC[0], TIME_OUT, ctx) == 0)
		{
			// LOG_D("receivedC[0] = %02x", receivedC[0]);
			if (receivedC[0] == ACK)
			{
				if (Receive_Byte(&receivedC[0], TIME_OUT, ctx) == 0)
				{
					if (receivedC[0] == CRC16)
						/* 接收到应答 */
						ackReceived = 1;
				}
			}
		}
		/* 没有等到 */
		else
		{
			errors++;
		}
		/* 发送数据包后接收到应答或者没有等到就推出 */
	} while (!ackReceived && (errors < 0x02));

	/* 超过最大错误次数就退出 */
	if (errors >= 0x02)
	{
		LOG_E("ymodem_transmit falied%d\n", errors);
		return errors;
	}

	blkNumber = 0x01;

	/* 下面使用的是发送1024字节数据包 */
	/* Resend packet if NAK  for a count of 10 else end of communication */
	uint32_t total_size = 0;
	while (size = fread(buf_ptr, 1, PACKET_1K_SIZE, ctx->fp))
	{
		total_size += size;
		/* 准备下一包数据 */
		Ymodem_PreparePacket(buf_ptr, &packet_data[0], blkNumber, size);
		ackReceived = 0;
		receivedC[0] = 0;
		errors = 0;
		do
		{
			/* 发送下一包数据 */
			if (size > PACKET_SIZE)
			{
				pktSize = PACKET_1K_SIZE;
			}
			else
			{
				pktSize = PACKET_SIZE;
			}
			tempCRC = Cal_CRC16(&packet_data[3], pktSize);
			packet_data[pktSize + PACKET_HEADER] = tempCRC >> 8;
			packet_data[pktSize + PACKET_HEADER + 1] = tempCRC & 0xFF;

			Ymodem_SendPacket(packet_data, pktSize + PACKET_OVERHEAD, ctx);
			// printf("send packet %d size %d\n", blkNumber, pktSize + PACKET_HEADER);
			// for (int i = 0; i < pktSize + PACKET_OVERHEAD; i++)
			// {
			// 	printf("%02x ", packet_data[i]);
			// }
			// printf("\n");

			/* 等到Ack信号 */
			if ((Receive_Byte(&receivedC[0], TIME_OUT, ctx) == 0))
			{
				if (receivedC[0] == ACK)
				{
					ackReceived = 1;
					blkNumber++;
					ca_stop_cnt = 0;
				}
				else if (receivedC[0] == CA)
				{
					/* 取消传输 */
					ca_stop_cnt++;
					if (ca_stop_cnt >= 2)
						return 0xFF;
				}
			}
			else
			{
				errors++;
			}

			LOG_I("ymodem_transmit SendPacket%d\n", blkNumber);

		} while (!ackReceived && (errors < 0x02));

		/* 超过10次没有收到应答就退出 */
		if (errors >= 0x02)
		{
			LOG_E("ymodem_transmit falied%d\n", errors);
			return errors;
		}
	}
	// printf("total_size = %d\n", total_size);

	ackReceived = 0;
	receivedC[0] = 0x00;
	errors = 0;
	do
	{
		Send_Byte(EOT, ctx);

		/* 发送EOT信号 */
		/* 等待Ack应答 */
		if ((Receive_Byte(&receivedC[0], TIME_OUT, ctx) == 0) && receivedC[0] == NAK)
		{
			ackReceived = 1;
		}
		else
		{
			errors++;
		}

	} while (!ackReceived && (errors < 0x02));

	/* 超过10次没有收到应答就退出 */
	if (errors >= 0x02)
	{
		return errors;
	}

	do
	{
		Send_Byte(EOT, ctx);
		/* 发送EOT信号 */
		/* 等待Ack应答 */
		if ((Receive_Byte(&receivedC[0], TIME_OUT, ctx) == 0) && receivedC[0] == ACK)
		{
			ackReceived = 1;
		}
		else
		{
			LOG_D("receivedC[0] = %02x\n", receivedC[0]);
			errors++;
		}
	} while (!ackReceived && (errors < 0x02));

	if (errors >= 0x02)
	{
		return errors;
	}
	return 0; /* 文件发送成功 */
}
