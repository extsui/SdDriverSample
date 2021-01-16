#ifndef SD_HPP
#define SD_HPP

#include <cstdint>

namespace SD {

constexpr uint32_t SECTOR_SIZE = 512;

constexpr uint8_t DATA_START_TOKEN_EXCEPT_CMD25 = 0xFE;
constexpr uint8_t DATA_START_TOKEN_CMD25 = 0xFC;
constexpr uint8_t DATA_STOP_TOKEN = 0xFD;

// stm32f303x8.h との衝突回避
#undef CRC

#pragma pack(push, 1)

struct CID {
	uint8_t  MID;		// 製造者 ID
	uint16_t OID;		// OEM/アプリケーション ID
	uint8_t  PNM[5];	// 製品名
	uint8_t  PRV;		// 製品リビジョン
	uint32_t PSN;		// 製造シリアル番号
	uint16_t MDT;		// 製造日 (最上位4 ビットは常に 0)
	uint8_t  CRC;		// CRC7 チェックサム
};

static_assert(sizeof(CID) == 16);

#pragma pack(pop)

}

#endif /* SD_SAMPLE_HPP */
