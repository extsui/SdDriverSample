#ifndef SD_SAMPLE_HPP
#define SD_SAMPLE_HPP

#include "main.h"
#include <stdio.h>

#include "stm32f3xx_hal_spi.h"

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

public:
	SdDriver(SPI_HandleTypeDef *spi);
	~SdDriver();

	void Initialize();
	void MainLoop();

private:
	void IssueCommand(uint8_t command, const uint8_t *arg);
	uint8_t GetResponseR1();
	uint8_t GetResponseR3R7(uint32_t *pReturnValue);
};


#endif /* SD_SAMPLE_HPP */
