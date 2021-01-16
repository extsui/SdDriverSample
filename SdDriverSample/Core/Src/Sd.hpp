#ifndef SD_HPP
#define SD_HPP

#include <cstdint>

namespace SD {

constexpr uint32_t SECTOR_SIZE = 512;

constexpr uint8_t DATA_START_TOKEN_EXCEPT_CMD25 = 0xFE;
constexpr uint8_t DATA_START_TOKEN_CMD25 = 0xFC;
constexpr uint8_t DATA_STOP_TOKEN = 0xFD;

#pragma pack(push, 1)

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

}

#endif /* SD_SAMPLE_HPP */
