// Multi2Decoder.cpp: CMulti2Decoder クラスのインプリメンテーション
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Multi2Decoder.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


#define SCRAMBLE_ROUND 4


#ifdef MULTI2_USE_INTRINSIC
#pragma intrinsic(_byteswap_ulong, _byteswap_uint64, _lrotl)
#endif


#ifdef MULTI2_SIMD

#if defined(MULTI2_SIMD_ICC) && !defined(__INTEL_COMPILER)

// ICCでコンパイルしたライブラリの使用
#include "../Multi2Decoder/Multi2DecoderSIMD.h"
#pragma comment(lib, "Multi2Decoder.lib")

#else

#include "..Multi2Decoder/Multi2DecoderSIMD.cpp"

#endif

class CSIMDInitializer
{
public:
	CSIMDInitializer() {
		Multi2DecoderSIMD::Initialize();
	}
};

static CSIMDInitializer SIMDInitializer;

#endif	// MULTI2_SIMD


// デスクランブルの統計を取る
#ifdef MULTI2_STATISTICS

class CMulti2Statistics
{
public:
	CMulti2Statistics()
	{
		Reset();
	}

	~CMulti2Statistics()
	{
		TRACE(TEXT("Multi2 statistics\n"));
		TRACE(TEXT("  Packets %u / Size %llu bytes\n"), m_PacketCount, m_Size);
		for (size_t i = 0; i < _countof(m_SizeCount); i++) {
			TRACE(TEXT("  Data size %3d : %u\n"), (int)i, m_SizeCount[i]);
		}
	}

	void Reset()
	{
		m_PacketCount = 0;
		ZeroMemory(m_SizeCount, sizeof(m_SizeCount));
		m_Size = 0;
	}

	void OnDecode(DWORD Size)
	{
		m_PacketCount++;
		m_SizeCount[Size]++;
		m_Size += Size;
	}

private:
	DWORD m_PacketCount;
	DWORD m_SizeCount[185];
	ULONGLONG m_Size;
};

static CMulti2Statistics Multi2Statistics;

#endif	// MULTI2_STATISTICS


inline void CMulti2Decoder::DATKEY::SetHexData(const BYTE *pHexData)
{
	// バイトオーダー変換
#ifndef MULTI2_USE_INTRINSIC
	Data[7] = pHexData[0];	Data[6] = pHexData[1];	Data[5] = pHexData[2];	Data[4] = pHexData[3];
	Data[3] = pHexData[4];	Data[2] = pHexData[5];	Data[1] = pHexData[6];	Data[0] = pHexData[7];
#else
#ifndef _WIN64
	dwLeft  = _byteswap_ulong(*reinterpret_cast<const DWORD*>(pHexData + 0));
	dwRight = _byteswap_ulong(*reinterpret_cast<const DWORD*>(pHexData + 4));
#else
	Data64 = _byteswap_uint64(*reinterpret_cast<const unsigned __int64*>(pHexData));
#endif
#endif
}

inline void CMulti2Decoder::DATKEY::GetHexData(BYTE *pHexData) const
{
	// バイトオーダー変換
#ifndef MULTI2_USE_INTRINSIC
	pHexData[0] = Data[7];	pHexData[1] = Data[6];	pHexData[2] = Data[5];	pHexData[3] = Data[4];
	pHexData[4] = Data[3];	pHexData[5] = Data[2];	pHexData[6] = Data[1];	pHexData[7] = Data[0];
#else
#ifndef _WIN64
	*reinterpret_cast<DWORD*>(pHexData + 0) = _byteswap_ulong(dwLeft);
	*reinterpret_cast<DWORD*>(pHexData + 4) = _byteswap_ulong(dwRight);
#else
	*reinterpret_cast<unsigned __int64*>(pHexData) = _byteswap_uint64(Data64);
#endif
#endif
}

inline void CMulti2Decoder::SYSKEY::SetHexData(const BYTE *pHexData)
{
	// バイトオーダー変換
#ifndef MULTI2_USE_INTRINSIC
	Data[ 3] = pHexData[ 0];	Data[ 2] = pHexData[ 1];	Data[ 1] = pHexData[ 2];	Data[ 0] = pHexData[ 3];
	Data[ 7] = pHexData[ 4];	Data[ 6] = pHexData[ 5];	Data[ 5] = pHexData[ 6];	Data[ 4] = pHexData[ 7];
	Data[11] = pHexData[ 8];	Data[10] = pHexData[ 9];	Data[ 9] = pHexData[10];	Data[ 8] = pHexData[11];
	Data[15] = pHexData[12];	Data[14] = pHexData[13];	Data[13] = pHexData[14];	Data[12] = pHexData[15];
	Data[19] = pHexData[16];	Data[18] = pHexData[17];	Data[17] = pHexData[18];	Data[16] = pHexData[19];
	Data[23] = pHexData[20];	Data[22] = pHexData[21];	Data[21] = pHexData[22];	Data[20] = pHexData[23];
	Data[27] = pHexData[24];	Data[26] = pHexData[25];	Data[25] = pHexData[26];	Data[24] = pHexData[27];
	Data[31] = pHexData[28];	Data[30] = pHexData[29];	Data[29] = pHexData[30];	Data[28] = pHexData[31];
#else
#ifndef _WIN64
	const DWORD *p = reinterpret_cast<const DWORD*>(pHexData);
	dwKey1 = _byteswap_ulong(p[0]);
	dwKey2 = _byteswap_ulong(p[1]);
	dwKey3 = _byteswap_ulong(p[2]);
	dwKey4 = _byteswap_ulong(p[3]);
	dwKey5 = _byteswap_ulong(p[4]);
	dwKey6 = _byteswap_ulong(p[5]);
	dwKey7 = _byteswap_ulong(p[6]);
	dwKey8 = _byteswap_ulong(p[7]);
#else
	const unsigned __int64 *p = reinterpret_cast<const unsigned __int64*>(pHexData);
	Data64[0] = _byteswap_uint64(p[0]);
	Data64[1] = _byteswap_uint64(p[1]);
	Data64[2] = _byteswap_uint64(p[2]);
	Data64[3] = _byteswap_uint64(p[3]);
#endif
#endif
}

inline void CMulti2Decoder::SYSKEY::GetHexData(BYTE *pHexData) const
{
	// バイトオーダー変換
#ifndef MULTI2_USE_INTRINSIC
	pHexData[ 0] = Data[ 3];	pHexData[ 1] = Data[ 2];	pHexData[ 2] = Data[ 1];	pHexData[ 3] = Data[ 0];
	pHexData[ 4] = Data[ 7];	pHexData[ 5] = Data[ 6];	pHexData[ 6] = Data[ 5];	pHexData[ 7] = Data[ 4];
	pHexData[ 8] = Data[11];	pHexData[ 9] = Data[10];	pHexData[10] = Data[ 9];	pHexData[11] = Data[ 8];
	pHexData[12] = Data[15];	pHexData[13] = Data[14];	pHexData[14] = Data[13];	pHexData[15] = Data[12];
	pHexData[16] = Data[19];	pHexData[17] = Data[18];	pHexData[18] = Data[17];	pHexData[19] = Data[16];
	pHexData[20] = Data[23];	pHexData[21] = Data[22];	pHexData[22] = Data[21];	pHexData[23] = Data[20];
	pHexData[24] = Data[27];	pHexData[25] = Data[26];	pHexData[26] = Data[25];	pHexData[27] = Data[24];
	pHexData[28] = Data[31];	pHexData[29] = Data[30];	pHexData[30] = Data[29];	pHexData[31] = Data[28];
#else
#ifndef _WIN64
	DWORD *p = reinterpret_cast<DWORD*>(pHexData);
	p[0] = _byteswap_ulong(dwKey1);
	p[1] = _byteswap_ulong(dwKey2);
	p[2] = _byteswap_ulong(dwKey3);
	p[3] = _byteswap_ulong(dwKey4);
	p[4] = _byteswap_ulong(dwKey5);
	p[5] = _byteswap_ulong(dwKey6);
	p[6] = _byteswap_ulong(dwKey7);
	p[7] = _byteswap_ulong(dwKey8);
#else
	unsigned __int64 *p = reinterpret_cast<unsigned __int64*>(pHexData);
	p[0] = _byteswap_uint64(Data64[0]);
	p[1] = _byteswap_uint64(Data64[1]);
	p[2] = _byteswap_uint64(Data64[2]);
	p[3] = _byteswap_uint64(Data64[3]);
#endif
#endif
}


CMulti2Decoder::CMulti2Decoder(
#ifdef MULTI2_SIMD
	InstructionType Instruction
#endif
	)
	: m_bIsSysKeyValid(false)
	, m_bIsWorkKeyValid(false)
#ifdef MULTI2_SIMD
	, m_Instruction(Instruction)
	, m_pSSE2WorkKeyOdd(NULL)
	, m_pSSE2WorkKeyEven(NULL)
#endif
{
#ifdef MULTI2_SIMD
	if (m_Instruction != INSTRUCTION_NORMAL) {
		Multi2DecoderSIMD::AllocWorkKey(&m_pSSE2WorkKeyOdd, &m_pSSE2WorkKeyEven);
	}
	switch (m_Instruction) {
	default:
	case INSTRUCTION_NORMAL:
		m_pDecodeFunc = Multi2DecoderSIMD::Decode;
		break;
#ifdef MULTI2_SSE2
	case INSTRUCTION_SSE2:
		m_pDecodeFunc = Multi2DecoderSIMD::DecodeSSE2;
		break;
#endif
#ifdef MULTI2_SSSE3
	case INSTRUCTION_SSSE3:
		m_pDecodeFunc = Multi2DecoderSIMD::DecodeSSSE3;
		break;
#endif
	}
#endif
}

CMulti2Decoder::~CMulti2Decoder(void)
{
#ifdef MULTI2_SIMD
	if (m_Instruction != INSTRUCTION_NORMAL) {
		Multi2DecoderSIMD::FreeWorkKey(&m_pSSE2WorkKeyOdd, &m_pSSE2WorkKeyEven);
	}
#endif
}

const bool CMulti2Decoder::Initialize(const BYTE *pSystemKey, const BYTE *pInitialCbc)
{
	if (!pSystemKey || !pInitialCbc)
		return false;

	m_bIsSysKeyValid = true;
	m_bIsWorkKeyValid = false;

	// Descrambling System Keyセット
	m_SystemKey.SetHexData(pSystemKey);

	// Descrambler CBC Initial Valueセット
	m_InitialCbc.SetHexData(pInitialCbc);

	return true;
}

const bool CMulti2Decoder::SetScrambleKey(const BYTE *pScrambleKey)
{
	if(!m_bIsSysKeyValid)return false;

	if(!pScrambleKey){
		// キーが設定されない場合はデコード不能にする(不正な復号による破損防止のため)
		m_bIsWorkKeyValid = false;
		return false;
		}

	// Scramble Key Odd/Even をセットする
	DATKEY ScrKeyOdd, ScrKeyEven;

	// バイトオーダー変換
	ScrKeyOdd.SetHexData(&pScrambleKey[0]);
	ScrKeyEven.SetHexData(&pScrambleKey[8]);

	// キースケジュール
	KeySchedule(m_WorkKeyOdd, m_SystemKey, ScrKeyOdd);
	KeySchedule(m_WorkKeyEven, m_SystemKey, ScrKeyEven);

#ifdef MULTI2_SIMD
	if (m_Instruction != INSTRUCTION_NORMAL) {
		Multi2DecoderSIMD::SetWorkKey(m_pSSE2WorkKeyOdd, m_WorkKeyOdd);
		Multi2DecoderSIMD::SetWorkKey(m_pSSE2WorkKeyEven, m_WorkKeyEven);
	}
#endif

	m_bIsWorkKeyValid = true;

	return true;
}

const bool CMulti2Decoder::Decode(BYTE *pData, const DWORD dwSize, const BYTE byScrCtrl) const
{
	if(!byScrCtrl)return true;										// スクランブルなし
	else if(!m_bIsSysKeyValid || !m_bIsWorkKeyValid)return false;	// スクランブルキー未設定
	else if((byScrCtrl != 2U) && (byScrCtrl != 3U))return false;	// スクランブル制御不正

#ifdef MULTI2_STATISTICS
	Multi2Statistics.OnDecode(dwSize);
#endif

	// ワークキー選択
	const SYSKEY &WorkKey = (byScrCtrl == 3)? m_WorkKeyOdd : m_WorkKeyEven;

#ifdef MULTI2_SIMD

	m_pDecodeFunc(pData, dwSize,
				  &WorkKey,
				  (byScrCtrl == 3) ? m_pSSE2WorkKeyOdd : m_pSSE2WorkKeyEven,
				  &m_InitialCbc);

#else	// MULTI2_SIMD

	DATKEY CbcData = m_InitialCbc;
	//DWORD RemainSize = dwSize / sizeof(DATKEY) * sizeof(DATKEY);
	DWORD RemainSize = dwSize & 0xFFFFFFF8UL;
	BYTE *pEnd = pData + RemainSize;
	BYTE *p = pData;

	// CBCモード
	DATKEY SrcData;
	while (p < pEnd) {
		SrcData.SetHexData(p);
		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi4(SrcData, WorkKey.dwKey8);
			RoundFuncPi3(SrcData, WorkKey.dwKey6, WorkKey.dwKey7);
			RoundFuncPi2(SrcData, WorkKey.dwKey5);
			RoundFuncPi1(SrcData);
			RoundFuncPi4(SrcData, WorkKey.dwKey4);
			RoundFuncPi3(SrcData, WorkKey.dwKey2, WorkKey.dwKey3);
			RoundFuncPi2(SrcData, WorkKey.dwKey1);
			RoundFuncPi1(SrcData);
		}
		SrcData ^= CbcData;
		CbcData.SetHexData(p);
		SrcData.GetHexData(p);
		p += sizeof(DATKEY);
	}

	// OFBモード
	//RemainSize = dwSize % sizeof(DATKEY);
	RemainSize = dwSize & 0x00000007UL;
	if (RemainSize > 0) {
		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi1(CbcData);
			RoundFuncPi2(CbcData, WorkKey.dwKey1);
			RoundFuncPi3(CbcData, WorkKey.dwKey2, WorkKey.dwKey3);
			RoundFuncPi4(CbcData, WorkKey.dwKey4);
			RoundFuncPi1(CbcData);
			RoundFuncPi2(CbcData, WorkKey.dwKey5);
			RoundFuncPi3(CbcData, WorkKey.dwKey6, WorkKey.dwKey7);
			RoundFuncPi4(CbcData, WorkKey.dwKey8);
		}

		BYTE Remain[sizeof(DATKEY)];
		CbcData.GetHexData(Remain);
#if 0
		pEnd += RemainSize;
		for (BYTE *q = Remain ; p < pEnd ; q++) {
			*p++ ^= *q;
		}
#else
		switch (RemainSize) {
		default: __assume(0);
		case 7: p[6] ^= Remain[6];
		case 6: p[5] ^= Remain[5];
		case 5: p[4] ^= Remain[4];
		case 4: p[3] ^= Remain[3];
		case 3: p[2] ^= Remain[2];
		case 2: p[1] ^= Remain[1];
		case 1: p[0] ^= Remain[0];
		}
#endif
	}

#endif	// MULTI2_SIMD

	return true;
}

void CMulti2Decoder::KeySchedule(SYSKEY &WorkKey, const SYSKEY &SysKey, DATKEY &DataKey)
{
	// Key Schedule
	RoundFuncPi1(DataKey);									// π1

	RoundFuncPi2(DataKey, SysKey.dwKey1);					// π2
	WorkKey.dwKey1 = DataKey.dwLeft;

	RoundFuncPi3(DataKey, SysKey.dwKey2, SysKey.dwKey3);	// π3
	WorkKey.dwKey2 = DataKey.dwRight;

	RoundFuncPi4(DataKey, SysKey.dwKey4);					// π4
	WorkKey.dwKey3 = DataKey.dwLeft;

	RoundFuncPi1(DataKey);									// π1
	WorkKey.dwKey4 = DataKey.dwRight;

	RoundFuncPi2(DataKey, SysKey.dwKey5);					// π2
	WorkKey.dwKey5 = DataKey.dwLeft;

	RoundFuncPi3(DataKey, SysKey.dwKey6, SysKey.dwKey7);	// π3
	WorkKey.dwKey6 = DataKey.dwRight;

	RoundFuncPi4(DataKey, SysKey.dwKey8);					// π4
	WorkKey.dwKey7 = DataKey.dwLeft;

	RoundFuncPi1(DataKey);									// π1
	WorkKey.dwKey8 = DataKey.dwRight;
}

inline void CMulti2Decoder::RoundFuncPi1(DATKEY &Block)
{
	// Elementary Encryption Function π1
	Block.dwRight ^= Block.dwLeft;
}

inline void CMulti2Decoder::RoundFuncPi2(DATKEY &Block, const DWORD dwK1)
{
	// Elementary Encryption Function π2
	const DWORD dwY = Block.dwRight + dwK1;
	const DWORD dwZ = LeftRotate(dwY, 1UL) + dwY - 1UL;
	Block.dwLeft ^= LeftRotate(dwZ, 4UL) ^ dwZ;
}

inline void CMulti2Decoder::RoundFuncPi3(DATKEY &Block, const DWORD dwK2, const DWORD dwK3)
{
	// Elementary Encryption Function π3
	const DWORD dwY = Block.dwLeft + dwK2;
	const DWORD dwZ = LeftRotate(dwY, 2UL) + dwY + 1UL;
	const DWORD dwA = LeftRotate(dwZ, 8UL) ^ dwZ;
	const DWORD dwB = dwA + dwK3;
	const DWORD dwC = LeftRotate(dwB, 1UL) - dwB;
	Block.dwRight ^= (LeftRotate(dwC, 16UL) ^ (dwC | Block.dwLeft));
}

inline void CMulti2Decoder::RoundFuncPi4(DATKEY &Block, const DWORD dwK4)
{
	// Elementary Encryption Function π4
	const DWORD dwY = Block.dwRight + dwK4;
	Block.dwLeft ^= (LeftRotate(dwY, 2UL) + dwY + 1UL);
}

inline const DWORD CMulti2Decoder::LeftRotate(const DWORD dwValue, const DWORD dwRotate)
{
	// 左ローテート
#ifndef MULTI2_USE_INTRINSIC
	return (dwValue << dwRotate) | (dwValue >> (32UL - dwRotate));
#else
	// 実は上のコードでもrolが使われる
	return _lrotl(dwValue, dwRotate);
#endif
}


#ifdef MULTI2_SIMD

bool CMulti2Decoder::IsSSE2Available()
{
	return Multi2DecoderSIMD::IsSSE2Available();
}

bool CMulti2Decoder::IsSSSE3Available()
{
	return Multi2DecoderSIMD::IsSSSE3Available();
}

#endif	// MULTI2_SIMD
