#ifndef SD_HPP
#define SD_HPP

#include <cstdint>

namespace SD {

constexpr uint32_t SECTOR_SIZE = 512;

constexpr uint8_t DATA_START_TOKEN_EXCEPT_CMD25 = 0xFE;
constexpr uint8_t DATA_START_TOKEN_CMD25 = 0xFC;
constexpr uint8_t DATA_STOP_TOKEN = 0xFD;

#pragma pack(push, 1)

// CID: Card Identification (128 ビット)
// バイト境界の項目が多いので実際の通信データサイズ (128 ビット) と合わせている
struct CID {
	uint8_t  MID;		// 製造者 ID
	uint16_t OID;		// OEM/アプリケーション ID
	uint8_t  PNM[5];	// 製品名
	uint8_t  PRV;		// 製品リビジョン
	uint32_t PSN;		// 製造シリアル番号
	uint16_t MDT;		// 製造日 (最上位4 ビットは常に 0)
	uint8_t  CRC7;		// CRC7 チェックサム
};

#pragma pack(pop)

constexpr uint32_t CID_SIZE = 16;
static_assert(sizeof(CID) == 16);

// RCA: Relative Card Address (16 ビット)
// TODO:

// DSR: Driver Stage Register (16 ビット)
// TODO:

// CSD: Card Specific Data (128 ビット)
// ビット境界の項目が多いので実際の通信データサイズ (128 ビット) と合わせていないことに注意
struct CSD {
	uint8_t  CSD_STRUCTURE;			// CSD バージョン
	// - Reserved
	uint8_t  TAAC;					// データ読み出しアクセス時間
	uint8_t  NSAC;					// データ読み出しアクセス・クロック数
	uint8_t  TRAN_SPEED;			// 最大データ転送レート
	uint16_t CCC;					// カード・コマンド・クラス
	uint8_t  READ_BL_LEN;			// 読み出し時最大データ・ブロック長
	uint8_t  READ_BL_PARTIAL;		// 読み出し時複数ブロック・サイズ許可
	uint8_t  WRITE_BLK_MISALIGN;	// 書き込み時物理ブロック境界非アラインメント許可
	uint8_t  READ_BLK_MISALIGN;		// 読み込み時物理ブロック境界非アラインメント許可
	uint8_t  DSR_IMP;				// DSR 機能の有無
	// - Reserved
	uint32_t C_SIZE;				// カード・サイズ
	// - Reserved
	uint8_t  ERASE_BLK_EN;			// シングル・ブロック消去有効
	uint8_t  SECTOR_SIZE;			// 消去セクタ・サイズ
	uint8_t  WP_GRP_SIZE;			// 書き込み保護グループ・サイズ
	uint8_t  WP_GRP_ENABLE;			// グループ書込み保護許可
	// - Reserved
	uint8_t  R2W_FACTOR;			// 読み出し時間をもとにしたプログラム時間
	uint8_t  WRITE_BL_LEN;			// 書き込み時最大データ・ブロック長
	uint8_t  WRITE_BL_PARTIAL;		// 書き込み時複数ブロック・サイズ許可
	// - Reserved
	uint8_t  FILE_FORMAT_GRP;		// ファイル・フォーマット・グループ
	uint8_t  COPY;					// コンテンツはオリジナル/コピー
	uint8_t  PERM_WRITE_PROTECT;	// 永久書込み保護
	uint8_t  TMP_WRITE_PROTECT;		// 一時書込み保護
	uint8_t  FILE_FORMAT;			// ファイル・フォーマット
	// - Reserved
	uint8_t  CRC7;					// CRC
	// - Reserved
};

constexpr uint32_t CSD_SIZE = 16;
static_assert(sizeof(CSD) > 16);	// 要注意

// SCR: SD Configuration Register (64 ビット)
// ビット境界の項目が多いので実際の通信データサイズ (128 ビット) と合わせていないことに注意
struct SCR {
	uint8_t SCR_STRUCTURE;			// SCR Structure
	uint8_t SD_SPEC;				// SD Memory Card Spec. Version
	uint8_t DATA_STAT_AFTER_ERASE;	// Data Status After Erases
	uint8_t SD_SECURITY;			// CPRM Security Support
	uint8_t SD_BUS_WIDTHS;			// DAT Bus Widths Supported
	uint8_t SD_SPEC3;				// Spec. Version 3.00 or Higher
	uint8_t EX_SECURITY;			// Extended Security Support
	uint8_t SD_SPEC4;				// Spec Version 4.00 or Higher
	// - Reserved
	uint8_t CMD_SUPPORT;			// Command Support Bits
	// - Reserved
};

constexpr uint32_t SCR_SIZE = 8;
static_assert(sizeof(SCR) > 8);

// OCR: Operation Conditions Register (32 ビット)
// ビット境界の項目が多いので実際の通信データサイズと合わせていないことに注意
struct OCR {
	uint8_t CARD_POWER_UP_STATUS_BIT;	// 0: busy / 1: ready
	uint8_t CARD_CAPACITY_STATUS;		// 0: SD Memory Card / 1: SDHC Memory Card
	// - Reserved
	// 動作電圧ウィンドウ (1 の立っている範囲の電圧に対応)
	uint8_t VDD_VOLTAGE_WINDOW_36_35;	// 3.6 - 3.5V
	uint8_t VDD_VOLTAGE_WINDOW_35_34;	// 3.5 - 3.4V
	uint8_t VDD_VOLTAGE_WINDOW_34_33;	// 3.4 - 3.3V
	uint8_t VDD_VOLTAGE_WINDOW_33_32;	// 3.3 - 3.2V
	uint8_t VDD_VOLTAGE_WINDOW_32_31;	// 3.2 - 3.1V
	uint8_t VDD_VOLTAGE_WINDOW_31_30;	// 3.1 - 3.0V
	uint8_t VDD_VOLTAGE_WINDOW_30_29;	// 3.0 - 2.9V
	uint8_t VDD_VOLTAGE_WINDOW_29_28;	// 2.9 - 2.8V
	uint8_t VDD_VOLTAGE_WINDOW_28_27;	// 2.8 - 2.7V
	uint8_t VDD_VOLTAGE_WINDOW_27_26;	// 2.7 - 2.6V
	uint8_t VDD_VOLTAGE_WINDOW_26_25;	// 2.6 - 2.5V
	uint8_t VDD_VOLTAGE_WINDOW_25_24;	// 2.5 - 2.4V
	uint8_t VDD_VOLTAGE_WINDOW_24_23;	// 2.4 - 2.3V
	uint8_t VDD_VOLTAGE_WINDOW_23_22;	// 2.3 - 2.2V
	uint8_t VDD_VOLTAGE_WINDOW_22_21;	// 2.2 - 2.1V
	uint8_t VDD_VOLTAGE_WINDOW_21_20;	// 2.1 - 2.0V
	uint8_t VDD_VOLTAGE_WINDOW_20_19;	// 2.0 - 1.9V
	uint8_t VDD_VOLTAGE_WINDOW_19_18;	// 1.9 - 1.8V
	uint8_t VDD_VOLTAGE_WINDOW_18_17;	// 1.8 - 1.7V
	uint8_t VDD_VOLTAGE_WINDOW_17_16;	// 1.7 - 1.6V
	// - Reserved
};

constexpr uint32_t OCR_SIZE = 4;
static_assert(sizeof(OCR) > 4);

// SSR: SD Status Register (512 ビット)
// TODO:

// CSR: Card Status Register (32 ビット)
// TODO:

}

#endif /* SD_SAMPLE_HPP */
