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

} // namespace

// ----------------------------------------------------------------------
//  class public methods
// ----------------------------------------------------------------------
SdDriver::SdDriver(SPI_HandleTypeDef *spi)
	: m_Spi(spi)
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
	IssueCommand(0, 0x00000000);
	response = GetResponseR1();
	ASSERT(response == 0x01);

	printf("[SD] CMD0 Response: 0x%02X\n", response);

	// CMD8
	IssueCommand(8, 0x000001AA);
	response = GetResponseR3R7(&returnValue);

	printf("[SD] CMD8 Response: 0x%02X 0x%08lX\n", response, returnValue);

	if ((returnValue & 0x000003FF) == 0x000001AA) {
		// SD Ver2
		printf("[SD] SD Version is 2\n");
	} else {
		printf("[SD] SD Version is not 2\n");
		ASSERT(0);
	}

	do {
		// ACMD41 (CMD55 -> CMD41)
		IssueCommand(55, 0x00000000);
		response = GetResponseR1();
		printf("[SD] CMD55 Response: 0x%02X\n", response);

		IssueCommand(41, 0x40000000);
		response = GetResponseR1();
		printf("[SD] CMD41 Response: 0x%02X\n", response);

	} while (response != 0x00);

	// CMD58
	// OCR 読み込み
	uint32_t ocr = 0;
	IssueCommand(58, 0x00000000);
	response = GetResponseR3R7(&ocr);
	printf("[SD] CMD58 Response: 0x%02X, OCR: 0x%08lX\n", response, ocr);
	// bit30 が CCS ビット。
	// 0 なら SD Ver.2 バイト・アドレッシング、1 なら　SD Ver.2 ブロック・アドレッシング。
	if ((ocr & 0x40000000) == 0x00000000) {
		printf("[SD] Byte Addressing\n");

		// ブロック・サイズを 512 バイトに設定
		IssueCommand(16, 0x00000200);
		response = GetResponseR1();
		printf("[SD] CMD16 Response: 0x%02X\n", response);

	} else {
		printf("[SD] Block Addressing\n");
	}
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

	/*
	// Byte Addressing の SD カードの場合は 1-513 バイト目になる
	// Block Addresssing の SD カードの場合は 513-1023 バイト目になる
	ReadSector(1, buffer);
	Hexdump(buffer, sizeof(buffer));
	*/

	while (1) {

	}
}

// ----------------------------------------------------------------------
//  class private methods
// ----------------------------------------------------------------------
void SdDriver::IssueCommand(uint8_t command, uint32_t argument)
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

// TODO: GetResponseR1 との共通化
// @param [out] pOutErrorStatus エラーステータス (8 ビット)
uint8_t SdDriver::GetResponseR2(uint8_t *pOutErrorStatus)
{
	CsEnable();

	uint8_t txData[2] = { 0xFF, 0xFF, };	// Dummy
	uint8_t rxData[2];
	bool responseOk = false;

	// 8 バイト以内に応答があるはず
	for (int i = 0; i < 8; i++) {
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, 1, 0xFFFF);
		if (rxData[0] == 0xFF) {
			continue;
		} else {
			// TODO: response がエラーでないことを確認する必要があるかもしれない
			responseOk = true;
			break;
		}
	}

	ASSERT(responseOk == true);

	// rxData[0] には有効な 1 バイト目が入っている
	// 残り 1 バイトを受信する
	HAL_SPI_TransmitReceive(m_Spi, &txData[1], &rxData[1], sizeof(rxData) - 1, 0xFFFF);

	CsDisable();

	*pOutErrorStatus = rxData[1];
	return rxData[0];
}

// @param [out] pReturnValue R3/R7 の戻り値 (32 ビット)
uint8_t SdDriver::GetResponseR3R7(uint32_t *pOutReturnValue)
{
	CsEnable();

	uint8_t txData[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, };	// Dummy
	uint8_t rxData[5];
	bool responseOk = false;

	// 8 バイト以内に応答があるはず
	for (int i = 0; i < 8; i++) {
		HAL_SPI_TransmitReceive(m_Spi, txData, rxData, 1, 0xFFFF);
		if (rxData[0] == 0xFF) {
			continue;
		} else {
			// TODO: response がエラーでないことを確認する必要があるかもしれない
			responseOk = true;
			break;
		}
	}

	ASSERT(responseOk == true);

	// rxData[0] には有効な 1 バイト目が入っている
	// 残り 4 バイトを受信する
	HAL_SPI_TransmitReceive(m_Spi, &txData[1], &rxData[1], sizeof(rxData) - 1, 0xFFFF);

	CsDisable();

	*pOutReturnValue = (((uint32_t)rxData[1] << 24) |
				     ((uint32_t)rxData[2] << 16) |
					 ((uint32_t)rxData[3] <<  8) |
   	   	   	   	  	 ((uint32_t)rxData[4] <<  0));
	return rxData[0];
}

void SdDriver::ReadSector(uint32_t sectorNumber, uint8_t *pOutBuffer)
{
	uint8_t response;

	// 1 セクタ読み込む
	// TODO: バイトアドレッシングとブロックアドレッシングで引数が変わってくるらしい
	IssueCommand(17, sectorNumber);
	response = GetResponseR1();
	printf("[SD] CMD17 Response: 0x%02X\n", response);

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

void SdDriver::ReadRegister(SD::CID *pOutRegister)
{
	uint8_t response;

	IssueCommand(10, 0x00000000);
	response = GetResponseR1();
	printf("[SD] CMD10 Response: 0x%02X\n", response);

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
	uint8_t response;
	uint32_t ocr;
	IssueCommand(58, 0x00000000);
	response = GetResponseR3R7(&ocr);
	printf("[SD] CMD58 Response: 0x%02X, OCR: 0x%08lX\n", response, ocr);

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
	uint8_t response;

	IssueCommand(9, 0x00000000);
	response = GetResponseR1();
	printf("[SD] CMD9 Response: 0x%02X\n", response);

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
	uint8_t response;

	// ACMD51 (CMD55 -> CMD51)
	IssueCommand(55, 0x00000000);
	response = GetResponseR1();
	printf("[SD] CMD55 Response: 0x%02X\n", response);

	IssueCommand(51, 0x40000000);
	response = GetResponseR1();
	printf("[SD] CMD51 Response: 0x%02X\n", response);

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
	uint8_t response;

	// ACMD51 (CMD55 -> CMD51)
	IssueCommand(55, 0x00000000);
	response = GetResponseR1();
	printf("[SD] CMD55 Response: 0x%02X\n", response);

	IssueCommand(13, 0x40000000);
	response = GetResponseR1();
	printf("[SD] CMD51 Response: 0x%02X\n", response);

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
