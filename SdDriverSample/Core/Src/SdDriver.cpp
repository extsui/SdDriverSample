#include "SdDriver.hpp"
#include <cstring>
#include <cctype>

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

void Hexdump(const uint8_t *buffer, uint32_t size)
{
    uint32_t i;

    for (i = 0; i < size; i++) {
        uint32_t mod = i & 0xf;
        if (mod == 0x0) {
            printf("%08lX  ", reinterpret_cast<uint32_t>(i));
        } else if (mod == 0x8) {
            printf(" ");
        }

        printf("%02X", *(reinterpret_cast<const uint8_t*>(buffer + i)));

        if (mod == 0xf) {
            char c;
            uint32_t j;
            printf("  |");
            for (j = 0; j <= 0xf; j++) {
                c = *(char*)(buffer + i - 0xf + j);
                // 表示不可能文字なら'.'に置き換える
                if (!std::isprint(c)) {
                    c = '.';
                }
                printf("%c", c);
            }
            printf("|\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");
}

// テスト用データ (元々のセクタ 0 の内容)
const uint8_t g_TestWriteData1[] = {
	0xEB, 0x58, 0x90, 0x6D, 0x6B, 0x66, 0x73, 0x2E,  0x66, 0x61, 0x74, 0x00, 0x02, 0x08, 0x20, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00,  0x10, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xE7, 0x00, 0xA4, 0x39, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x00, 0x29, 0x1E, 0xE8, 0x92, 0x4A, 0x4E,  0x4F, 0x20, 0x4E, 0x41, 0x4D, 0x45, 0x20, 0x20,
	0x20, 0x20, 0x46, 0x41, 0x54, 0x33, 0x32, 0x20,  0x20, 0x20, 0x0E, 0x1F, 0xBE, 0x77, 0x7C, 0xAC,
	0x22, 0xC0, 0x74, 0x0B, 0x56, 0xB4, 0x0E, 0xBB,  0x07, 0x00, 0xCD, 0x10, 0x5E, 0xEB, 0xF0, 0x32,
	0xE4, 0xCD, 0x16, 0xCD, 0x19, 0xEB, 0xFE, 0x54,  0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6E,
	0x6F, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6F, 0x6F,  0x74, 0x61, 0x62, 0x6C, 0x65, 0x20, 0x64, 0x69,
	0x73, 0x6B, 0x2E, 0x20, 0x20, 0x50, 0x6C, 0x65,  0x61, 0x73, 0x65, 0x20, 0x69, 0x6E, 0x73, 0x65,
	0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6F, 0x6F,  0x74, 0x61, 0x62, 0x6C, 0x65, 0x20, 0x66, 0x6C,
	0x6F, 0x70, 0x70, 0x79, 0x20, 0x61, 0x6E, 0x64,  0x0D, 0x0A, 0x70, 0x72, 0x65, 0x73, 0x73, 0x20,
	0x61, 0x6E, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20,  0x74, 0x6F, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61,
	0x67, 0x61, 0x69, 0x6E, 0x20, 0x2E, 0x2E, 0x2E,  0x20, 0x0D, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA,
};

const uint8_t g_TestWriteData2[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,  0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,  0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,  0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,  0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,  0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,  0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,  0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,  0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,  0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,  0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,  0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,  0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,  0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,  0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,  0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,  0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

} // namespace

// ----------------------------------------------------------------------
//  class public methods
// ----------------------------------------------------------------------
SdDriver::SdDriver(SPI_HandleTypeDef *spi)
	: m_Spi(spi)
	, m_IsInitialized(false)
	, m_SectorCount(0xFFFFFFFF)
{
	std::memset(m_Dummy, 0xFF, SD::SECTOR_SIZE);
}

SdDriver::~SdDriver()
{
	// no reach
	ASSERT(0);
}

void SdDriver::Initialize()
{
	// 1ms 以上待つ (余裕をもって 10ms)
	HAL_Delay(10);

	// 74 以上のダミークロック (余裕をもって 80 クロック == 10 バイト)
	// (CS=Hi, DI=Hi)
	CsDisable();
	for (int i = 0; i < 10; i++) {
		uint8_t dummy[1] = { 0xFF };
		HAL_SPI_Transmit(m_Spi, dummy, 1, 0xFFFF);
	}

	// CMD0: SPI モードへの移行
	IssueCommandGoIdleState();
	// CMD8: SD Ver 判定
	IssueCommandSendIfCond();
	// ACMD41: SD 初期化
	IssueCommandAppSendOpCond();
	// CMD58: アドレッシング確認
	SD::OCR ocr;
	ReadRegister(&ocr);
	// 2GB 以下の SD カードはバイトアドレッシング
	if (ocr.CARD_CAPACITY_STATUS == 0) {
		// CMD16: ブロック長の設定
		IssueCommandSetBlocklen();
	
		// TODO: バイトアドレッシングは面倒なので未対応
		// (データ転送で *512 するだけではある)
		ASSERT(0);
		printf("[SD] Error: Byte Addressing is not supported.\n");
	}

	// -- ここまでで初期化は完了 --

	// CMD9: SD カードの容量取得
	SD::CSD csd;
	ReadRegister(&csd);
	m_SectorCount = csd.C_SIZE * 1024;
	printf("[SD] Sector Count: %lu\n", m_SectorCount);
	// 容量 = セクタ総数 * 512 --> m_SectorCount * 512 / 1024 / 1024 / 1024 [GiB]
	printf("[SD] SD Card Capacity: about %lu GiB\n", m_SectorCount / 2 / 1024 / 1024);

	m_IsInitialized = true;
}

void SdDriver::MainLoop()
{
	static uint8_t buffer[512];

	SD::CID cid;
	ReadRegister(&cid);

	printf("CID ----------------------------------------\n");
	printf("  MID  : %02X\n", cid.MID);
	printf("  OID  : %04X\n", cid.OID);
	printf("  PNM  : \'%c%c%c%c%c\'\n", cid.PNM[0], cid.PNM[1], cid.PNM[2], cid.PNM[3], cid.PNM[4]);
	printf("  PSN  : %08lX\n", cid.PSN);
	printf("  MDT  : %04X\n", cid.MDT);
	printf("  CRC7 : %02X\n", cid.CRC7);

	SD::CSD csd;
	ReadRegister(&csd);

	printf("CSD ----------------------------------------\n");
	printf("  CSD_STRUCTURE      : %02X\n",  csd.CSD_STRUCTURE     );
	printf("  TAAC               : %02X\n",  csd.TAAC              );
	printf("  NSAC               : %02X\n",  csd.NSAC              );
	printf("  TRAN_SPEED         : %02X\n",  csd.TRAN_SPEED        );
	printf("  CCC                : %04X\n",  csd.CCC               );
	printf("  READ_BL_LEN        : %02X\n",  csd.READ_BL_LEN       );
	printf("  READ_BL_PARTIAL    : %02X\n",  csd.READ_BL_PARTIAL   );
	printf("  WRITE_BLK_MISALIGN : %02X\n",  csd.WRITE_BLK_MISALIGN);
	printf("  READ_BLK_MISALIGN  : %02X\n",  csd.READ_BLK_MISALIGN );
	printf("  DSR_IMP            : %02X\n",  csd.DSR_IMP           );
	printf("  C_SIZE             : %08lX\n", csd.C_SIZE            );
	printf("  ERASE_BLK_EN       : %02X\n",  csd.ERASE_BLK_EN      );
	printf("  SECTOR_SIZE        : %02X\n",  csd.SECTOR_SIZE       );
	printf("  WP_GRP_SIZE        : %02X\n",  csd.WP_GRP_SIZE       );
	printf("  WP_GRP_ENABLE      : %02X\n",  csd.WP_GRP_ENABLE     );
	printf("  R2W_FACTOR         : %02X\n",  csd.R2W_FACTOR        );
	printf("  WRITE_BL_LEN       : %02X\n",  csd.WRITE_BL_LEN      );
	printf("  WRITE_BL_PARTIAL   : %02X\n",  csd.WRITE_BL_PARTIAL  );
	printf("  FILE_FORMAT_GRP    : %02X\n",  csd.FILE_FORMAT_GRP   );
	printf("  COPY               : %02X\n",  csd.COPY              );
	printf("  PERM_WRITE_PROTECT : %02X\n",  csd.PERM_WRITE_PROTECT);
	printf("  TMP_WRITE_PROTECT  : %02X\n",  csd.TMP_WRITE_PROTECT );
	printf("  FILE_FORMAT        : %02X\n",  csd.FILE_FORMAT       );
	printf("  CRC7               : %02X\n",  csd.CRC7              );

	SD::OCR ocr;
	ReadRegister(&ocr);

	printf("OCR ----------------------------------------\n");
	printf("  Busy Flag    : %d (%s)\n", ocr.CARD_POWER_UP_STATUS_BIT, ((ocr.CARD_POWER_UP_STATUS_BIT == 1) ? "Busy" : "Free"));
	printf("  CCS  Flag    : %d (%s)\n", ocr.CARD_CAPACITY_STATUS,     ((ocr.CARD_CAPACITY_STATUS     == 1) ? "Block Addressing" : "Byte Addressing"));
	printf("  Voltage Window:\n");
	printf("    3.6 - 3.5V : %d\n", ocr.VDD_VOLTAGE_WINDOW_36_35);
	printf("    3.5 - 3.4V : %d\n", ocr.VDD_VOLTAGE_WINDOW_35_34);
	printf("    3.4 - 3.3V : %d\n", ocr.VDD_VOLTAGE_WINDOW_34_33);
	printf("    3.3 - 3.2V : %d\n", ocr.VDD_VOLTAGE_WINDOW_33_32);
	printf("    3.2 - 3.1V : %d\n", ocr.VDD_VOLTAGE_WINDOW_32_31);
	printf("    3.1 - 3.0V : %d\n", ocr.VDD_VOLTAGE_WINDOW_31_30);
	printf("    3.0 - 2.9V : %d\n", ocr.VDD_VOLTAGE_WINDOW_30_29);
	printf("    2.9 - 2.8V : %d\n", ocr.VDD_VOLTAGE_WINDOW_29_28);
	printf("    2.8 - 2.7V : %d\n", ocr.VDD_VOLTAGE_WINDOW_28_27);
	printf("    2.7 - 2.6V : %d\n", ocr.VDD_VOLTAGE_WINDOW_27_26);
	printf("    2.6 - 2.5V : %d\n", ocr.VDD_VOLTAGE_WINDOW_26_25);
	printf("    2.5 - 2.4V : %d\n", ocr.VDD_VOLTAGE_WINDOW_25_24);
	printf("    2.4 - 2.3V : %d\n", ocr.VDD_VOLTAGE_WINDOW_24_23);
	printf("    2.3 - 2.2V : %d\n", ocr.VDD_VOLTAGE_WINDOW_23_22);
	printf("    2.2 - 2.1V : %d\n", ocr.VDD_VOLTAGE_WINDOW_22_21);
	printf("    2.1 - 2.0V : %d\n", ocr.VDD_VOLTAGE_WINDOW_21_20);
	printf("    2.0 - 1.9V : %d\n", ocr.VDD_VOLTAGE_WINDOW_20_19);
	printf("    1.9 - 1.8V : %d\n", ocr.VDD_VOLTAGE_WINDOW_19_18);
	printf("    1.8 - 1.7V : %d\n", ocr.VDD_VOLTAGE_WINDOW_18_17);
	printf("    1.7 - 1.6V : %d\n", ocr.VDD_VOLTAGE_WINDOW_17_16);

	SD::SCR scr;
	ReadRegister(&scr);

	printf("SCR ----------------------------------------\n");
	printf("  SCR_STRUCTURE         : %02X\n", scr.SCR_STRUCTURE        );
	printf("  SD_SPEC               : %02X\n", scr.SD_SPEC              );
	printf("  DATA_STAT_AFTER_ERASE : %02X\n", scr.DATA_STAT_AFTER_ERASE);
	printf("  SD_SECURITY           : %02X\n", scr.SD_SECURITY          );
	printf("  SD_BUS_WIDTHS         : %02X\n", scr.SD_BUS_WIDTHS        );
	printf("  SD_SPEC3              : %02X\n", scr.SD_SPEC3             );
	printf("  EX_SECURITY           : %02X\n", scr.EX_SECURITY          );
	printf("  SD_SPEC4              : %02X\n", scr.SD_SPEC4             );
	printf("  CMD_SUPPORT           : %02X\n", scr.CMD_SUPPORT          );

	SD::SSR ssr;
	ReadRegister(&ssr);

	printf("SSR ----------------------------------------\n");
    printf("  DAT_BUS_WIDTH          : %02X\n",  ssr.DAT_BUS_WIDTH         );
    printf("  SECURED_MODE           : %02X\n",  ssr.SECURED_MODE          );
    printf("  SD_CARD_TYPE           : %04X\n",  ssr.SD_CARD_TYPE          );
    printf("  SIZE_OF_PROTECTED_AREA : %08lX\n", ssr.SIZE_OF_PROTECTED_AREA);
    printf("  SPEED_CLASS            : %02X\n",  ssr.SPEED_CLASS           );
    printf("  PERFORMANCE_MOVE       : %02X\n",  ssr.PERFORMANCE_MOVE      );
    printf("  AU_SIZE                : %02X\n",  ssr.AU_SIZE               );
    printf("  ERASE_SIZE             : %04X\n",  ssr.ERASE_SIZE            );
    printf("  ERASE_TIMEOUT          : %02X\n",  ssr.ERASE_TIMEOUT         );
    printf("  ERASE_OFFSET           : %02X\n",  ssr.ERASE_OFFSET          );

	ReadSector(0, buffer);
	Hexdump(buffer, sizeof(buffer));

	// REPL
	while (1) {
		static uint8_t command[80];
		memset(command, 0, sizeof(command));
		int length = 0;
		
		printf("> ");
		fflush(stdout);

		length = ConsoleReadLine(command);
		printf("Command : [%s]\n", command);
		printf("Length  : %d\n", length);

		if (strncmp((const char*)command, "w", 1) == 0) {
			// TODO:
			printf("Write Command\n");
			WriteSector(0, g_TestWriteData2);

		} else if (strncmp((const char*)command, "r", 1) == 0) {
			printf("Read Command\n");
			// TODO: 引数で指定セクタを読み込めるようにする
			printf("[0]");
			ReadSector(0, buffer);
			Hexdump(buffer, sizeof(buffer));

			printf("[1]");
			ReadSector(1, buffer);
			Hexdump(buffer, sizeof(buffer));
			/*
			// Byte Addressing の SD カードの場合は 1-513 バイト目になる
			// Block Addresssing の SD カードの場合は 513-1023 バイト目になる
			ReadSector(1, buffer);
			Hexdump(buffer, sizeof(buffer));
			*/
		} else if (strncmp((const char*)command, "s", 1) == 0) {
			IssueCommandGetStatus();
		}
	}
}

// ----------------------------------------------------------------------
//  class private methods
// ----------------------------------------------------------------------
uint8_t SdDriver::IssueCommand(uint8_t command, uint32_t argument, SD::ResponseType responseType, void *pAdditionalResponse)
{
	CsEnable();

	uint8_t txData[6];
	// 01xxxxxx (x: CMDn, 初期化コマンドは CMD0)
	txData[0] = (0x40 | command);
	txData[1] = (uint8_t)((argument & 0xFF000000) >> 24);
	txData[2] = (uint8_t)((argument & 0x00FF0000) >> 16);
	txData[3] = (uint8_t)((argument & 0x0000FF00) >>  8);
	txData[4] = (uint8_t)((argument & 0x000000FF) >>  0);
	txData[5] = (uint8_t)GetSdCrc(txData);	// CMD0 以外は不要だが他のコマンドでもついでに計算
	HAL_SPI_Transmit(m_Spi, txData, sizeof(txData), 0xFFFF);

	printf("[SD] CMD%d 0x%08lX\n", command, argument);

	uint8_t r1Response = 0;
	uint8_t r2ErrorStatus = 0;
	uint32_t r3r7ReturnValue = 0;

	switch (responseType) {
	case SD::ResponseType::R1:
		r1Response = GetResponseR1();
		printf("[SD] R1 0x%02X\n", r1Response);
		break;

	case SD::ResponseType::R2:
		ASSERT(pAdditionalResponse != nullptr);
		r1Response = GetResponseR2(&r2ErrorStatus);
		*reinterpret_cast<uint8_t*>(pAdditionalResponse) = r2ErrorStatus;
		printf("[SD] R2 0x%02X 0x%02X\n", r1Response, r2ErrorStatus);
		break;

	case SD::ResponseType::R3:
	case SD::ResponseType::R7:
		ASSERT(pAdditionalResponse != nullptr);
		r1Response = GetResponseR3R7(&r3r7ReturnValue);
		*reinterpret_cast<uint32_t*>(pAdditionalResponse) = r3r7ReturnValue;
		printf("[SD] %s 0x%02X 0x%08lX\n", ((responseType == SD::ResponseType::R3) ? "R3" : "R7"), r1Response, r3r7ReturnValue);
		break;

	case SD::ResponseType::R1b:
		// TODO: Not Implemented
		printf("[SD] R1b is not implemented.\n");
		ASSERT(0);
		break;

	default:
		// ここに来ることは無い
		ASSERT(0);
		break;
	}

	CsDisable();

	return r1Response;
}

uint8_t SdDriver::IssueCommand(uint8_t command, uint32_t argument, SD::ResponseType responseType)
{
	return IssueCommand(command, argument, responseType, nullptr);
}

// CMD0 + アイドル状態確認
void SdDriver::IssueCommandGoIdleState()
{
	uint8_t response = IssueCommand(0, 0x00000000, SD::ResponseType::R1);
	if (response != static_cast<uint8_t>(SD::R1ResponseFormat::InIdleState)) {
		printf("[SD] Error: CMD0 Resp is not InIdleState.\n");
		ASSERT(0);
	}
}

// CMD8 + SD Version 確認 (要 ver.2)
void SdDriver::IssueCommandSendIfCond()
{
	uint32_t returnValue = 0;
	IssueCommand(8, 0x000001AA, SD::ResponseType::R7, &returnValue);
	if ((returnValue & 0x000003FF) != 0x000001AA) {
		printf("[SD] Error: SD Version must be 2.\n");
		ASSERT(0);
	}
}

// CMD9
void SdDriver::IssueCommandSendCsd()
{
	IssueCommand(9, 0x00000000, SD::ResponseType::R1);
}

// CMD10
void SdDriver::IssueCommandSendCid()
{
	IssueCommand(10, 0x00000000, SD::ResponseType::R1);
}

// CMD13 + エラー確認
void SdDriver::IssueCommandGetStatus()
{
	uint8_t errorStatus;
	IssueCommand(13, 0x00000000, SD::ResponseType::R2, &errorStatus);

	// TODO: errorStatus のハンドリング処理
}

// CMD16
void SdDriver::IssueCommandSetBlocklen()
{
	// ブロック・サイズを 512 バイトに設定
	IssueCommand(16, 0x00000200, SD::ResponseType::R1);
}

// CMD17
void SdDriver::IssueCommandReadSingleSector(uint32_t sectorNumber)
{
	IssueCommand(17, sectorNumber, SD::ResponseType::R1);
}

// CMD24
void SdDriver::IssueCommandWriteSingleSector(uint32_t sectorNumber)
{
	IssueCommand(24, sectorNumber, SD::ResponseType::R1);
}

// CMD55 (ACMDn 用)
void SdDriver::IssueCommandAppCmd()
{
	IssueCommand(55, 0x00000000, SD::ResponseType::R1);
}

// CMD58
void SdDriver::IssueCommandReadOcr(uint32_t *pOutOcr)
{
	uint32_t ocr = 0;
	IssueCommand(58, 0x00000000, SD::ResponseType::R3, &ocr);
	ASSERT(pOutOcr != nullptr);
	*pOutOcr = ocr;
}

// ACMD13
void SdDriver::IssueCommandSdStatus()
{
	IssueCommandAppCmd();
	IssueCommand(13, 0x40000000, SD::ResponseType::R1);
}

// ACMD41 + 初期化完了確認
void SdDriver::IssueCommandAppSendOpCond()
{
	uint8_t response = 0xFF;
	// 最大で 1 秒程度かかる
	while (response != 0x00) {
		IssueCommandAppCmd();
		response = IssueCommand(41, 0x40000000, SD::ResponseType::R1);
		// TODO: 初期化確認ループ周期の決定
		HAL_Delay(10);
	}
}

// ACMD51
void SdDriver::IssueCommandSendScr()
{
	IssueCommandAppCmd();
	IssueCommand(51, 0x40000000, SD::ResponseType::R1);
}

uint8_t SdDriver::GetResponseR1()
{
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

	// TODO: R1 の内容確認
	
	return rxData[0];
}

uint8_t SdDriver::GetResponseR2(uint8_t *pOutErrorStatus)
{
	uint8_t r1Response = GetResponseR1();

	uint8_t txData[1] = { 0xFF, };	// Dummy
	uint8_t rxData[1];
	HAL_SPI_TransmitReceive(m_Spi, txData, rxData, sizeof(rxData), 0xFFFF);
	*pOutErrorStatus = rxData[0];

	return r1Response;
}

uint8_t SdDriver::GetResponseR3R7(uint32_t *pOutReturnValue)
{
	uint8_t r1Response = GetResponseR1();

	uint8_t txData[4] = { 0xFF, 0xFF, 0xFF, 0xFF, };	// Dummy
	uint8_t rxData[4];
	HAL_SPI_TransmitReceive(m_Spi, txData, rxData, sizeof(rxData), 0xFFFF);

	*pOutReturnValue = (((uint32_t)rxData[0] << 24) |
				    	((uint32_t)rxData[1] << 16) |
						((uint32_t)rxData[2] <<  8) |
   	   	   	   	  		((uint32_t)rxData[3] <<  0));
	return r1Response;
}

// R1 系の後に 0xFF が来るまで待つ
// ブロック書き込みの完了で使用?
// TODO: マルチブロック系でも使用?
uint8_t SdDriver::GetDataResponse()
{
	CsEnable();

	uint8_t txData[1] = { 0xFF };	// Dummy
	uint8_t rxData[1];

	// 最初に R1 が来る
	HAL_SPI_TransmitReceive(m_Spi, txData, rxData, sizeof(rxData), 0xFFFF);
	uint8_t response = rxData[0];

	// 0xFF が来るまで待つ
	while (1) {
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, sizeof(rxData), 0xFFFF);
		if (rxData[0] == 0xFF) {
			break;
		}
	}

	CsDisable();

	return response;
}

void SdDriver::ReadSector(uint32_t sectorNumber, uint8_t *pOutBuffer)
{
	IssueCommandReadSingleSector(sectorNumber);

	// データパケット読み込み
	CsEnable();
	while (1) {
		uint8_t txData[1] = { 0xFF };
		uint8_t rxData[1];
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, 1, 0xFFFF);
		if (rxData[0] == SD::DATA_START_TOKEN_EXCEPT_CMD25) {
			break;
		}
	}

	// HAL_SPI_Receive() だと 0xFF 以外のデータが送信されてしまうので
	// HAL_SPI_TransmitReceive() を使用する必要がある
	HAL_SPI_TransmitReceive(m_Spi, m_Dummy, pOutBuffer, SD::SECTOR_SIZE, 0xFFFF);

	// データパケットに CRC が含まれているので読み込むが確認はしない
	uint8_t crc[2];
	HAL_SPI_TransmitReceive(m_Spi, m_Dummy, crc, sizeof(crc), 0xFFFF);

	// MEMO:
	// CMD17 の場合はデータパケットを受信完了すると自動的に
	// data ステートから tran ステートに戻るみたいなので CMD12 (転送完了) は不要

	CsDisable();
}

void SdDriver::WriteSector(uint32_t sectorNumber, const uint8_t *pBuffer)
{
	IssueCommandWriteSingleSector(sectorNumber);

	// MEMO: セクタ読み込みと同じ方式
	// TODO: データパケット読み込みの共通化
	// データパケット読み込み
	CsEnable();

	// 1 バイト以上空ける必要がある
	uint8_t txData;
	txData = 0xFF;
	HAL_SPI_Transmit(m_Spi, &txData, 1, 0xFFFF);

	// [データ開始トークン][書き込みデータ (512)][CRC (2)]
	txData = SD::DATA_START_TOKEN_EXCEPT_CMD25;
	HAL_SPI_Transmit(m_Spi, &txData, 1, 0xFFFF);

	HAL_SPI_Transmit(m_Spi, (uint8_t*)pBuffer, SD::SECTOR_SIZE, 0xFFFF);

	uint8_t crc[2] = { 0x00, 0x00 };
	HAL_SPI_TransmitReceive(m_Spi, m_Dummy, crc, sizeof(crc), 0xFFFF);

	uint8_t response = GetDataResponse();
	printf("[SD] Data Response: 0x%02X\n", response);

	CsDisable();
}

void SdDriver::EraseSector(uint32_t sectorNumber)
{
	// TODO: Not Implemented
	printf("[SD] Error: Not implemented.\n");
	ASSERT(0);
}

void SdDriver::ReadRegister(SD::CID *pOutRegister)
{
	IssueCommandSendCid();

	// MEMO: セクタ読み込みと同じ方式
	// TODO: データパケット読み込みの共通化
	// データパケット読み込み
	CsEnable();
	while (1) {
		uint8_t txData[1] = { 0xFF };
		uint8_t rxData[1];
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, 1, 0xFFFF);
		if (rxData[0] == SD::DATA_START_TOKEN_EXCEPT_CMD25) {
			break;
		}
	}

	uint8_t rxData[SD::CID_SIZE];
	HAL_SPI_TransmitReceive(m_Spi, m_Dummy, rxData, sizeof(rxData), 0xFFFF);
	CsDisable();

	pOutRegister->MID    = rxData[0];
	pOutRegister->OID    = (static_cast<uint16_t>(rxData[1]) << 8) | rxData[2];
	pOutRegister->PNM[0] = rxData[3];
	pOutRegister->PNM[1] = rxData[4];
	pOutRegister->PNM[2] = rxData[5];
	pOutRegister->PNM[3] = rxData[6];
	pOutRegister->PNM[4] = rxData[7];
	pOutRegister->PRV    = rxData[8];
	pOutRegister->PSN    = ((static_cast<uint32_t>(rxData[9])  << 24) |
							(static_cast<uint32_t>(rxData[10]) << 16) |
							(static_cast<uint32_t>(rxData[11]) <<  8) |
							(static_cast<uint32_t>(rxData[12]) <<  0));
	pOutRegister->MDT    = static_cast<uint16_t>(rxData[13] << 8) | rxData[14];
	pOutRegister->CRC7   = rxData[15];
}

void SdDriver::ReadRegister(SD::OCR *pOutRegister)
{
	uint32_t ocr = 0;
	IssueCommandReadOcr(&ocr);

	pOutRegister->CARD_POWER_UP_STATUS_BIT = (uint8_t)((ocr & 0x80000000) >> 31);
	pOutRegister->CARD_CAPACITY_STATUS     = (uint8_t)((ocr & 0x40000000) >> 30);
	pOutRegister->VDD_VOLTAGE_WINDOW_36_35 = (uint8_t)((ocr & 0x00800000) >> 23);
	pOutRegister->VDD_VOLTAGE_WINDOW_35_34 = (uint8_t)((ocr & 0x00400000) >> 22);
	pOutRegister->VDD_VOLTAGE_WINDOW_34_33 = (uint8_t)((ocr & 0x00200000) >> 21);
	pOutRegister->VDD_VOLTAGE_WINDOW_33_32 = (uint8_t)((ocr & 0x00100000) >> 20);
	pOutRegister->VDD_VOLTAGE_WINDOW_32_31 = (uint8_t)((ocr & 0x00080000) >> 19);
	pOutRegister->VDD_VOLTAGE_WINDOW_31_30 = (uint8_t)((ocr & 0x00040000) >> 18);
	pOutRegister->VDD_VOLTAGE_WINDOW_30_29 = (uint8_t)((ocr & 0x00020000) >> 17);
	pOutRegister->VDD_VOLTAGE_WINDOW_29_28 = (uint8_t)((ocr & 0x00010000) >> 16);
	pOutRegister->VDD_VOLTAGE_WINDOW_28_27 = (uint8_t)((ocr & 0x00008000) >> 15);
	pOutRegister->VDD_VOLTAGE_WINDOW_27_26 = (uint8_t)((ocr & 0x00004000) >> 14);
	pOutRegister->VDD_VOLTAGE_WINDOW_26_25 = (uint8_t)((ocr & 0x00002000) >> 13);
	pOutRegister->VDD_VOLTAGE_WINDOW_25_24 = (uint8_t)((ocr & 0x00001000) >> 12);
	pOutRegister->VDD_VOLTAGE_WINDOW_24_23 = (uint8_t)((ocr & 0x00000800) >> 11);
	pOutRegister->VDD_VOLTAGE_WINDOW_23_22 = (uint8_t)((ocr & 0x00000400) >> 10);
	pOutRegister->VDD_VOLTAGE_WINDOW_22_21 = (uint8_t)((ocr & 0x00000200) >>  9);
	pOutRegister->VDD_VOLTAGE_WINDOW_21_20 = (uint8_t)((ocr & 0x00000100) >>  8);
	pOutRegister->VDD_VOLTAGE_WINDOW_20_19 = (uint8_t)((ocr & 0x00000080) >>  7);
	pOutRegister->VDD_VOLTAGE_WINDOW_19_18 = (uint8_t)((ocr & 0x00000040) >>  6);
	pOutRegister->VDD_VOLTAGE_WINDOW_18_17 = (uint8_t)((ocr & 0x00000020) >>  5);
	pOutRegister->VDD_VOLTAGE_WINDOW_17_16 = (uint8_t)((ocr & 0x00000010) >>  4);
}

void SdDriver::ReadRegister(SD::CSD *pOutRegister)
{
	IssueCommandSendCsd();

	// MEMO: セクタ読み込みと同じ方式
	// TODO: データパケット読み込みの共通化
	// データパケット読み込み
	CsEnable();
	while (1) {
		uint8_t txData[1] = { 0xFF };
		uint8_t rxData[1];
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, 1, 0xFFFF);
		if (rxData[0] == SD::DATA_START_TOKEN_EXCEPT_CMD25) {
			break;
		}
	}

	uint8_t rxData[SD::CSD_SIZE];
	HAL_SPI_TransmitReceive(m_Spi, m_Dummy, rxData, sizeof(rxData), 0xFFFF);
	CsDisable();

    pOutRegister->CSD_STRUCTURE       = (rxData[0] & 0xC0) >> 6;
    pOutRegister->TAAC                = rxData[1];
    pOutRegister->NSAC                = rxData[2];
    pOutRegister->TRAN_SPEED          = rxData[3];
    pOutRegister->CCC                 = (((uint16_t)rxData[4] & 0xF0) << 4) |
                                        (((uint16_t)rxData[4] & 0x0F) << 4) |
                                        (((uint16_t)rxData[5] & 0xF0) >> 4);
    pOutRegister->READ_BL_LEN         = (rxData[5] & 0x0F);
    pOutRegister->READ_BL_PARTIAL     = (rxData[6] & 0x80) >> 7;
    pOutRegister->WRITE_BLK_MISALIGN  = (rxData[6] & 0x40) >> 6;
    pOutRegister->READ_BLK_MISALIGN   = (rxData[6] & 0x20) >> 5;
    pOutRegister->DSR_IMP             = (rxData[6] & 0x10) >> 4;
    pOutRegister->C_SIZE              = (((uint32_t)rxData[7] & 0x3F) << 16) |
                                        (((uint32_t)rxData[8])        <<  8) |
                                        (((uint32_t)rxData[9])        <<  0);
    pOutRegister->ERASE_BLK_EN        = ((rxData[10] & 0x40) >> 6);
    pOutRegister->SECTOR_SIZE         = ((rxData[10] & 0x3F) << 1) |
                                        ((rxData[11] & 0x80) >> 7);
    pOutRegister->WP_GRP_SIZE         = (rxData[11] & 0x7F);
    pOutRegister->WP_GRP_ENABLE       = (rxData[12] & 0x80) >> 7;
    pOutRegister->R2W_FACTOR          = (rxData[12] & 0x1C) >> 2;
    pOutRegister->WRITE_BL_LEN        = ((rxData[12] & 0x03) << 2) |
                                        ((rxData[13] & 0xC0) >> 6);
    pOutRegister->WRITE_BL_PARTIAL    = (rxData[13] & 0x20) >> 5;
    pOutRegister->FILE_FORMAT_GRP     = (rxData[14] & 0x80) >> 7;
    pOutRegister->COPY                = (rxData[14] & 0x40) >> 6;
    pOutRegister->PERM_WRITE_PROTECT  = (rxData[14] & 0x20) >> 5;
    pOutRegister->TMP_WRITE_PROTECT   = (rxData[14] & 0x10) >> 1;
    pOutRegister->FILE_FORMAT         = (rxData[14] & 0x0C) >> 2;
    pOutRegister->CRC7                = (rxData[15] & 0xFE) >> 1;
}

void SdDriver::ReadRegister(SD::SCR *pOutRegister)
{
	IssueCommandSendScr();

	// MEMO: セクタ読み込みと同じ方式
	// TODO: データパケット読み込みの共通化
	// データパケット読み込み
	CsEnable();
	while (1) {
		uint8_t txData[1] = { 0xFF };
		uint8_t rxData[1];
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, 1, 0xFFFF);
		if (rxData[0] == SD::DATA_START_TOKEN_EXCEPT_CMD25) {
			break;
		}
	}

	uint8_t rxData[SD::SCR_SIZE];
	HAL_SPI_TransmitReceive(m_Spi, m_Dummy, rxData, sizeof(rxData), 0xFFFF);
	CsDisable();

	pOutRegister->SCR_STRUCTURE         = (rxData[0] & 0xF0) >> 4;
	pOutRegister->SD_SPEC               = (rxData[0] & 0x0F);
	pOutRegister->DATA_STAT_AFTER_ERASE = (rxData[1] & 0x80) >> 7;
	pOutRegister->SD_SECURITY           = (rxData[1] & 0x70) >> 4;
	pOutRegister->SD_BUS_WIDTHS         = (rxData[1] & 0x0F);
	pOutRegister->SD_SPEC3              = (rxData[2] & 0x80) >> 7;
	pOutRegister->EX_SECURITY           = (rxData[2] & 0x78) >> 3;
	pOutRegister->SD_SPEC4              = (rxData[2] & 0x04) >> 2;
	pOutRegister->CMD_SUPPORT           = (rxData[3] & 0x0F);
}

void SdDriver::ReadRegister(SD::SSR *pOutRegister)
{
	IssueCommandSdStatus();

	// MEMO: セクタ読み込みと同じ方式
	// TODO: データパケット読み込みの共通化
	// データパケット読み込み
	CsEnable();
	while (1) {
		uint8_t txData[1] = { 0xFF };
		uint8_t rxData[1];
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, 1, 0xFFFF);
		if (rxData[0] == SD::DATA_START_TOKEN_EXCEPT_CMD25) {
			break;
		}
	}

	uint8_t rxData[SD::SSR_SIZE];
	HAL_SPI_TransmitReceive(m_Spi, m_Dummy, rxData, sizeof(rxData), 0xFFFF);
	CsDisable();

	pOutRegister->DAT_BUS_WIDTH			 = (rxData[0] & 0xC0) >> 6;
	pOutRegister->SECURED_MODE			 = (rxData[0] & 0x20) >> 5;
	pOutRegister->SD_CARD_TYPE			 = (((uint16_t)rxData[2] << 8) | rxData[3]);
	pOutRegister->SIZE_OF_PROTECTED_AREA = (((uint32_t)rxData[4] << 24) |
										    ((uint32_t)rxData[5] << 16) |
										    ((uint32_t)rxData[6] <<  8) |
										    ((uint32_t)rxData[7]));
	pOutRegister->SPEED_CLASS			 = rxData[8];
	pOutRegister->PERFORMANCE_MOVE		 = rxData[9];
	pOutRegister->AU_SIZE				 = (rxData[10] & 0xC0) >> 4;
	pOutRegister->ERASE_SIZE			 = (((uint16_t)rxData[11] << 8) | rxData[12]);
	pOutRegister->ERASE_TIMEOUT			 = (rxData[13] & 0xFC) >> 2;
	pOutRegister->ERASE_OFFSET			 = (rxData[13] & 0x03);
}
