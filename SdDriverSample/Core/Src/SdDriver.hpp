#ifndef SD_SAMPLE_HPP
#define SD_SAMPLE_HPP

#include "main.h"
#include <stdio.h>
#include <cstdint>

#include "stm32f3xx_hal_spi.h"

#include "Sd.hpp"

// ITM 経由のデバッグログ出力
#define DEBUG_LOG(...)  printf(__VA_ARGS__)

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
	// SPI 送信用のダミーデータ
	// 全て 0xFF で埋めて使用すること。
	uint8_t m_Dummy[SD::SECTOR_SIZE];

public:
	SdDriver(SPI_HandleTypeDef *spi);
	~SdDriver();

	void Initialize();
	void MainLoop();

private:
	void IssueCommand(uint8_t command, uint32_t argument);
	uint8_t GetResponseR1();
	uint8_t GetResponseR2(uint8_t *pOutErrorStatus);
	uint8_t GetResponseR3R7(uint32_t *pOutReturnValue);
	void ReadSector(uint32_t sectorNumber, uint8_t *pOutBuffer);
	void ReadRegister(SD::CID *pOutRegister);
	void ReadRegister(SD::CSD *pOutRegister);
	void ReadRegister(SD::OCR *pOutRegister);
	void ReadRegister(SD::SCR *pOutRegister);
};


#endif /* SD_SAMPLE_HPP */
