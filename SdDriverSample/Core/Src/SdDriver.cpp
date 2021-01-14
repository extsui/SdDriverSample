#include "SdDriver.hpp"

// ----------------------------------------------------------------------
//  static private functions
// ----------------------------------------------------------------------
namespace {

void CsEnable()
{
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);
}

void CsDisable()
{
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
}

/**
 * SD 用の CRC7 を取得する
 * @param buf 長さ 5 の計算対象バッファ
 * @return CRC
 */
uint8_t GetSdCrc(const uint8_t *buf)
{
	int crc;
	int crc_prev;

	crc = buf[0];
	for(int i = 1; i < 6; i++) {
		for(int j = 7; j >= 0; j--) {
			crc <<= 1;
			crc_prev = crc;
			if (i < 5) {
				crc |= (buf[i] >> j) & 1;
			}
			if (crc & 0x80) {
				crc ^= 0x89;	// Generator
			}
		}
	}
	return (uint8_t)(crc_prev | 1);
}

} // namespace

// ----------------------------------------------------------------------
//  class public methods
// ----------------------------------------------------------------------
SdDriver::SdDriver(SPI_HandleTypeDef *spi)
	: m_Spi(spi)
{
}

SdDriver::~SdDriver()
{
	// no reach
	ASSERT(0);
}

void SdDriver::Initialize()
{
	uint8_t arg[4];
	uint8_t response;		// R1/R2/R3/R7 兼用
	uint32_t returnValue;	// R3/R7 専用

	// 1ms 以上待つ (余裕をもって 10ms)
	HAL_Delay(10);

	// 74 以上のダミークロック (余裕をもって 80 クロック == 10 バイト)
	// (CS=Hi, DI=Hi)
	CsDisable();
	for (int i = 0; i < 10; i++) {
		uint8_t dummy[1] = { 0xFF };
		HAL_SPI_Transmit(m_Spi, dummy, 1, 0xFFFF);
	}

	// CMD0
	arg[0] = 0x00;
	arg[1] = 0x00;
	arg[2] = 0x00;
	arg[3] = 0x00;
	IssueCommand(0, arg);
	response = GetResponseR1();
	ASSERT(response == 0x01);

	printf("[SD] CMD0 Response: 0x%02x\n", response);

	// CMD8
	arg[0] = 0x00;
	arg[1] = 0x00;
	arg[2] = 0x01;
	arg[3] = 0xAA;
	IssueCommand(8, arg);
	response = GetResponseR3R7(&returnValue);

	printf("[SD] CMD8 Response: 0x%02x 0x%08lx\n", response, returnValue);

	if ((returnValue & 0x000003FF) == 0x000001AA) {
		// SD Ver2
		printf("[SD] SD Version is 2\n");
	} else {
		printf("[SD] SD Version is not 2\n");
	}
}

void SdDriver::MainLoop()
{
	while (1) {


	}
}

// ----------------------------------------------------------------------
//  class private methods
// ----------------------------------------------------------------------
void SdDriver::IssueCommand(uint8_t command, const uint8_t *arg)
{
	CsEnable();

	uint8_t txData[6];
	// 01xxxxxx (x: CMDn, 初期化コマンドは CMD0)
	txData[0] = (0x40 | command);
	txData[1] = arg[0];
	txData[2] = arg[1];
	txData[3] = arg[2];
	txData[4] = arg[3];
	txData[5] = (uint8_t)GetSdCrc(txData);	// CMD0 以外は不要だが他のコマンドでもついでに計算
	HAL_SPI_Transmit(m_Spi, txData, sizeof(txData), 0xFFFF);

	CsDisable();
}

uint8_t SdDriver::GetResponseR1()
{
	CsEnable();

	uint8_t txData[1] = { 0xFF };	// Dummy
	uint8_t rxData[1];
	bool responseOk = false;

	// 8 バイト以内に応答があるはず
	for (int i = 0; i < 8; i++) {
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, sizeof(rxData), 0xFFFF);
		if ((rxData[0] & 0x80) == 0x00) {
			responseOk = true;
			break;
		}
	}
	ASSERT(responseOk == true);

	CsDisable();

	return rxData[0];
}

// @param [out] pReturnValue R3/R7 の戻り値 (32 ビット)
uint8_t SdDriver::GetResponseR3R7(uint32_t *pReturnValue)
{
	CsEnable();

	uint8_t txData[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, };	// Dummy
	uint8_t rxData[5];
	bool responseOk = false;


	// TODO: 1/13 向けメモ
	// - 下のようなデータ列で受信する。つまり、0xFF の間は空読みしなければならない。
	// - { 0xFF, 0x01, 0x00, 0x00, 0x01, 0xAA, 0xFF, 0xFF, 0xFF, ... }


	// 8 バイト以内に応答があるはず
	for (int i = 0; i < 8; i++) {
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, sizeof(rxData), 0xFFFF);
		if ((rxData[0] & 0x80) == 0x00) {
			responseOk = true;
			break;
		}
	}
	ASSERT(responseOk == true);

	CsDisable();

	*pReturnValue = (((uint32_t)rxData[1] << 24) |
				     ((uint32_t)rxData[2] << 16) |
					 ((uint32_t)rxData[3] <<  8) |
   	   	   	   	  	 ((uint32_t)rxData[4] <<  0));
	return rxData[0];
}


// TODO: GetResponseR2()
// TODO: GetResponseR3()

