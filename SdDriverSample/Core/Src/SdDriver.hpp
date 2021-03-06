#ifndef SD_SAMPLE_HPP
#define SD_SAMPLE_HPP

#include "main.h"
#include <stdio.h>
#include <cstdint>

#include "stm32f3xx_hal_spi.h"

#include "Sd.hpp"

#define DEBUG_LOG(...)  printf(__VA_ARGS__)

extern "C" int ConsoleReadLine(uint8_t *pOutBuffer);

static inline void ABORT() { while (1); }

#define __ASSERT(expr, file, line)                         \
	DEBUG_LOG("Assertion failed: %s, file %s, line %d\n",  \
		expr, file, line),                                 \
	ABORT()

#define ASSERT(expr)                              \
    ((expr) ? ((void)0) :                         \
    (void)(__ASSERT(#expr, __FILE__, __LINE__)))

class SdDriver
{
private:
	SPI_HandleTypeDef *m_Spi;

	// 初期化済みフラグ
	bool m_IsInitialized;

	// セクタ総数
	uint32_t m_SectorCount;

	// SPI 送信用のダミーデータ
	// 全て 0xFF で埋めて使用すること。
	uint8_t m_Dummy[SD::SECTOR_SIZE];

public:
	SdDriver(SPI_HandleTypeDef *spi);
	~SdDriver();

	void Initialize();
	void MainLoop();

private:
	uint8_t IssueCommand(uint8_t command, uint32_t argument, SD::ResponseType responseType, void *pAdditionalResponse);
	uint8_t IssueCommand(uint8_t command, uint32_t argument, SD::ResponseType responseType);

	// CMD0
	void IssueCommandGoIdleState();
	// CMD8
	void IssueCommandSendIfCond();
	// CMD9
	void IssueCommandSendCsd();
	// CMD10
	void IssueCommandSendCid();
	// CMD12
	void IssueCommandStopTransmission();
	// CMD13
	void IssueCommandGetStatus();
	// CMD16
	void IssueCommandSetBlocklen();
	// CMD17
	void IssueCommandReadSingleBlock(uint32_t sectorIndex);
	// CMD18
	void IssueCommandReadMultipleBlock(uint32_t sectorIndex);
	// CMD24
	void IssueCommandWriteSingleBlock(uint32_t sectorIndex);
	// CMD25
	void IssueCommandWriteMultipleBlock(uint32_t sectorIndex);
	// CMD55
	void IssueCommandAppCmd();
	// CMD58
	void IssueCommandReadOcr(uint32_t *pOutOcr);
	// ACMD13
	void IssueCommandSdStatus();
	// ACMD41
	void IssueCommandAppSendOpCond();
	// ACMD51
	void IssueCommandSendScr();

	uint8_t GetResponseR1();
	uint8_t GetResponseR1b();
	uint8_t GetResponseR2(uint8_t *pOutErrorStatus);
	uint8_t GetResponseR3R7(uint32_t *pOutReturnValue);
	uint8_t GetDataResponse();
	
	void ReadSector(uint8_t *pOutBuffer, uint32_t sectorIndex);
	void ReadSector(uint8_t *pOutBuffer, uint32_t sectorIndex, uint32_t blockNum);
	void WriteSector(const uint8_t *pBuffer, uint32_t sectorIndex);
	void EraseSector(uint32_t sectorIndex);

	void ReadRegister(SD::CID *pOutRegister);
	void ReadRegister(SD::CSD *pOutRegister);
	void ReadRegister(SD::OCR *pOutRegister);
	void ReadRegister(SD::SCR *pOutRegister);
	void ReadRegister(SD::SSR *pOutRegister);
};

#endif /* SD_SAMPLE_HPP */
