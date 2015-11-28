#include <windows.h>
#include "Multi2Decoder.h"
#include <intrin.h>
#include <emmintrin.h>
#ifdef MULTI2_SSSE3
#include <tmmintrin.h>
#endif
#include "Multi2DecoderSIMD.h"


// パイプライン向け最適化
#define MULTI2_OPTIMIZE_FOR_PIPELINE


namespace Multi2DecoderSIMD
{

struct WORKKEY {
	__m128i Key[8];
};

#ifndef SCRAMBLE_ROUND
#define SCRAMBLE_ROUND 4
#endif

#define MM_SHUFFLE4(a, b, c, d) (((a) << 6) | ((b) << 4) | ((c) << 2) | (d))

//#define IMMEDIATE1 Immediate1
#define IMMEDIATE1 _mm_set1_epi32(1)


//static __m128i Immediate1;

#ifdef MULTI2_SSSE3
static __m128i ByteSwapMask;
static __m128i SrcSwapMask;
static __m128i Rotate16Mask;
static __m128i Rotate8Mask;
#endif


bool IsSSE2Available()
{
#if defined(_M_IX86)
	bool b;

	__asm {
		mov		eax, 1
		cpuid
		bt		edx, 26
		setc	b
	}

	return b;
#elif defined(_M_AMD64) || defined(_M_X64)
	return true;
#else
	return ::IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE) != FALSE;
#endif
}


bool IsSSSE3Available()
{
	int Info[4];

	__cpuid(Info, 1);
	if (Info[2] & 0x200)	// bt ecx, 9
		return true;

	return false;
}


bool Initialize()
{
	if (!IsSSE2Available())
		return false;

	//Immediate1 = _mm_set1_epi32(1);

#ifdef MULTI2_SSSE3
	if (IsSSSE3Available()) {
		ByteSwapMask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);
		SrcSwapMask  = _mm_set_epi8(12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3);
		Rotate16Mask = _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
		Rotate8Mask  = _mm_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
	}
#endif

	return true;
}


void AllocWorkKey(WORKKEY **ppWorkKeyOdd, WORKKEY **ppWorkKeyEven)
{
	*ppWorkKeyOdd  = (WORKKEY*)_mm_malloc(sizeof(WORKKEY) * 2, 16);
	*ppWorkKeyEven = *ppWorkKeyOdd + 1;
}


void FreeWorkKey(WORKKEY **ppWorkKeyOdd, WORKKEY **ppWorkKeyEven)
{
	if (*ppWorkKeyOdd) {
		_mm_free(*ppWorkKeyOdd);
		*ppWorkKeyOdd  = NULL;
		*ppWorkKeyEven = NULL;
	}
}


void SetWorkKey(WORKKEY *pWorkKey, const CMulti2Decoder::SYSKEY &SrcKey)
{
	pWorkKey->Key[0] = _mm_set1_epi32(SrcKey.dwKey1);
	pWorkKey->Key[1] = _mm_set1_epi32(SrcKey.dwKey2);
	pWorkKey->Key[2] = _mm_set1_epi32(SrcKey.dwKey3);
	pWorkKey->Key[3] = _mm_set1_epi32(SrcKey.dwKey4);
	pWorkKey->Key[4] = _mm_set1_epi32(SrcKey.dwKey5);
	pWorkKey->Key[5] = _mm_set1_epi32(SrcKey.dwKey6);
	pWorkKey->Key[6] = _mm_set1_epi32(SrcKey.dwKey7);
	pWorkKey->Key[7] = _mm_set1_epi32(SrcKey.dwKey8);
}


static __forceinline const DWORD LeftRotate(const DWORD dwValue, const DWORD dwRotate)
{
	return _lrotl(dwValue, dwRotate);
}

static __forceinline void RoundFuncPi1(DWORD &Left, DWORD &Right)
{
	// Elementary Encryption Function π1
	Right ^= Left;
}

static __forceinline void RoundFuncPi2(DWORD &Left, DWORD &Right, const DWORD dwK1)
{
	// Elementary Encryption Function π2
	const DWORD dwY = Right + dwK1;
	const DWORD dwZ = LeftRotate(dwY, 1UL) + dwY - 1UL;
	Left ^= LeftRotate(dwZ, 4UL) ^ dwZ;
}

static __forceinline void RoundFuncPi3(DWORD &Left, DWORD &Right, const DWORD dwK2, const DWORD dwK3)
{
	// Elementary Encryption Function π3
	const DWORD dwY = Left + dwK2;
	const DWORD dwZ = LeftRotate(dwY, 2UL) + dwY + 1UL;
	const DWORD dwA = LeftRotate(dwZ, 8UL) ^ dwZ;
	const DWORD dwB = dwA + dwK3;
	const DWORD dwC = LeftRotate(dwB, 1UL) - dwB;
	Right ^= (LeftRotate(dwC, 16UL) ^ (dwC | Left));
}

static __forceinline void RoundFuncPi4(DWORD &Left, DWORD &Right, const DWORD dwK4)
{
	// Elementary Encryption Function π4
	const DWORD dwY = Right + dwK4;
	Left ^= (LeftRotate(dwY, 2UL) + dwY + 1UL);
}


void Decode(BYTE * __restrict pData, const DWORD dwSize,
			const CMulti2Decoder::SYSKEY * __restrict pWorkKey,
			const WORKKEY * __restrict pPackedWorkKey,
			const CMulti2Decoder::DATKEY * __restrict pInitialCbc)
{
	__assume(dwSize <= 184);

	BYTE * __restrict p = pData;
	DWORD CbcLeft = pInitialCbc->dwLeft, CbcRight = pInitialCbc->dwRight;

	for (BYTE *pEnd = p + (dwSize & 0xFFFFFFF8UL); p < pEnd; p += 8) {
		DWORD Src1, Src2, Left, Right;

		Src1 = _byteswap_ulong(*(DWORD*)(p + 0));
		Src2 = _byteswap_ulong(*(DWORD*)(p + 4));
		Left  = Src1;
		Right = Src2;

#if defined(__INTEL_COMPILER) && SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi4(Left, Right, pWorkKey->dwKey8);
			RoundFuncPi3(Left, Right, pWorkKey->dwKey6, pWorkKey->dwKey7);
			RoundFuncPi2(Left, Right, pWorkKey->dwKey5);
			RoundFuncPi1(Left, Right);
			RoundFuncPi4(Left, Right, pWorkKey->dwKey4);
			RoundFuncPi3(Left, Right, pWorkKey->dwKey2, pWorkKey->dwKey3);
			RoundFuncPi2(Left, Right, pWorkKey->dwKey1);
			RoundFuncPi1(Left, Right);
		}

		*(DWORD*)(p + 0) = _byteswap_ulong(Left  ^ CbcLeft);
		*(DWORD*)(p + 4) = _byteswap_ulong(Right ^ CbcRight);
		CbcLeft  = Src1;
		CbcRight = Src2;
	}

	// OFBモード
	DWORD RemainSize = dwSize & 0x00000007UL;
	if (RemainSize) {
		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi1(CbcLeft, CbcRight);
			RoundFuncPi2(CbcLeft, CbcRight, pWorkKey->dwKey1);
			RoundFuncPi3(CbcLeft, CbcRight, pWorkKey->dwKey2, pWorkKey->dwKey3);
			RoundFuncPi4(CbcLeft, CbcRight, pWorkKey->dwKey4);
			RoundFuncPi1(CbcLeft, CbcRight);
			RoundFuncPi2(CbcLeft, CbcRight, pWorkKey->dwKey5);
			RoundFuncPi3(CbcLeft, CbcRight, pWorkKey->dwKey6, pWorkKey->dwKey7);
			RoundFuncPi4(CbcLeft, CbcRight, pWorkKey->dwKey8);
		}

		BYTE Remain[8];
		*(DWORD*)(Remain + 0) = CbcLeft;
		*(DWORD*)(Remain + 4) = CbcRight;
		switch (RemainSize) {
		default: __assume(0);
		case 7: p[6] ^= Remain[5];
		case 6: p[5] ^= Remain[6];
		case 5: p[4] ^= Remain[7];
		case 4: p[3] ^= Remain[0];
		case 3: p[2] ^= Remain[1];
		case 2: p[1] ^= Remain[2];
		case 1: p[0] ^= Remain[3];
		}
	}
}


static __forceinline __m128i LeftRotate(const __m128i &Value, const int Rotate)
{
	return _mm_or_si128(_mm_slli_epi32(Value, Rotate), _mm_srli_epi32(Value, 32 - Rotate));
}

static __forceinline __m128i ByteSwapSSE2(const __m128i &Value)
{
	__m128i t0 = _mm_srli_epi16(Value, 8);
	__m128i t1 = _mm_slli_epi16(Value, 8);
	return LeftRotate(_mm_or_si128(t0, t1), 16);
}

static __forceinline void RoundFuncPi1SSE2(__m128i &Left, __m128i &Right)
{
	Right = _mm_xor_si128(Right, Left);
}

static __forceinline void RoundFuncPi2SSE2(__m128i &Left, __m128i &Right, const __m128i &Key1)
{
	__m128i t;

	t = _mm_add_epi32(Right, Key1);
	t = _mm_sub_epi32(_mm_add_epi32(LeftRotate(t, 1), t), IMMEDIATE1);
	t = _mm_xor_si128(LeftRotate(t, 4), t);

	Left = _mm_xor_si128(Left, t);
}

static __forceinline void RoundFuncPi3SSE2(__m128i &Left, __m128i &Right, const __m128i &Key2, const __m128i &Key3)
{
	__m128i t;

	t = _mm_add_epi32(Left, Key2);
	t = _mm_add_epi32(_mm_add_epi32(LeftRotate(t, 2), t), IMMEDIATE1);
	t = _mm_xor_si128(LeftRotate(t, 8), t);
	t = _mm_add_epi32(t, Key3);
	t = _mm_sub_epi32(LeftRotate(t, 1), t);
	t = _mm_xor_si128(LeftRotate(t, 16), _mm_or_si128(t, Left));

	Right = _mm_xor_si128(Right, t);
}

static __forceinline void RoundFuncPi4SSE2(__m128i &Left, __m128i &Right, const __m128i &Key4)
{
	__m128i t;

	t = _mm_add_epi32(Right, Key4);
	t = _mm_add_epi32(_mm_add_epi32(LeftRotate(t, 2), t), IMMEDIATE1);

	Left = _mm_xor_si128(Left, t);
}


#ifdef MULTI2_OPTIMIZE_FOR_PIPELINE

static __forceinline void RoundFuncPi1SSE2_3(__m128i &Left1, __m128i &Right1,
											 __m128i &Left2, __m128i &Right2,
											 __m128i &Left3, __m128i &Right3)
{
	Right1 = _mm_xor_si128(Right1, Left1);
	Right2 = _mm_xor_si128(Right2, Left2);
	Right3 = _mm_xor_si128(Right3, Left3);
}

static __forceinline void RoundFuncPi2SSE2_3(__m128i &Left1, __m128i &Right1,
											 __m128i &Left2, __m128i &Right2,
											 __m128i &Left3, __m128i &Right3,
											 const __m128i &Key1)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(Right1, Key1);
	t2 = _mm_add_epi32(Right2, Key1);
	t3 = _mm_add_epi32(Right3, Key1);
	t1 = _mm_add_epi32(LeftRotate(t1, 1), t1);
	t2 = _mm_add_epi32(LeftRotate(t2, 1), t2);
	t3 = _mm_add_epi32(LeftRotate(t3, 1), t3);
	t1 = _mm_sub_epi32(t1, IMMEDIATE1);
	t2 = _mm_sub_epi32(t2, IMMEDIATE1);
	t3 = _mm_sub_epi32(t3, IMMEDIATE1);
	t1 = _mm_xor_si128(LeftRotate(t1, 4), t1);
	t2 = _mm_xor_si128(LeftRotate(t2, 4), t2);
	t3 = _mm_xor_si128(LeftRotate(t3, 4), t3);
	Left1 = _mm_xor_si128(Left1, t1);
	Left2 = _mm_xor_si128(Left2, t2);
	Left3 = _mm_xor_si128(Left3, t3);
}

static __forceinline void RoundFuncPi3SSE2_3(__m128i &Left1, __m128i &Right1,
											 __m128i &Left2, __m128i &Right2,
											 __m128i &Left3, __m128i &Right3,
											 const __m128i &Key2, const __m128i &Key3)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(Left1, Key2);
	t2 = _mm_add_epi32(Left2, Key2);
	t3 = _mm_add_epi32(Left3, Key2);
	t1 = _mm_add_epi32(LeftRotate(t1, 2), t1);
	t2 = _mm_add_epi32(LeftRotate(t2, 2), t2);
	t3 = _mm_add_epi32(LeftRotate(t3, 2), t3);
	t1 = _mm_add_epi32(t1, IMMEDIATE1);
	t2 = _mm_add_epi32(t2, IMMEDIATE1);
	t3 = _mm_add_epi32(t3, IMMEDIATE1);
	t1 = _mm_xor_si128(LeftRotate(t1, 8), t1);
	t2 = _mm_xor_si128(LeftRotate(t2, 8), t2);
	t3 = _mm_xor_si128(LeftRotate(t3, 8), t3);
	t1 = _mm_add_epi32(t1, Key3);
	t2 = _mm_add_epi32(t2, Key3);
	t3 = _mm_add_epi32(t3, Key3);
	t1 = _mm_sub_epi32(LeftRotate(t1, 1), t1);
	t2 = _mm_sub_epi32(LeftRotate(t2, 1), t2);
	t3 = _mm_sub_epi32(LeftRotate(t3, 1), t3);
	t1 = _mm_xor_si128(LeftRotate(t1, 16), _mm_or_si128(t1, Left1));
	t2 = _mm_xor_si128(LeftRotate(t2, 16), _mm_or_si128(t2, Left2));
	t3 = _mm_xor_si128(LeftRotate(t3, 16), _mm_or_si128(t3, Left3));
	Right1 = _mm_xor_si128(Right1, t1);
	Right2 = _mm_xor_si128(Right2, t2);
	Right3 = _mm_xor_si128(Right3, t3);
}

static __forceinline void RoundFuncPi4SSE2_3(__m128i &Left1, __m128i &Right1,
											 __m128i &Left2, __m128i &Right2,
											 __m128i &Left3, __m128i &Right3,
											 const __m128i &Key4)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(Right1, Key4);
	t2 = _mm_add_epi32(Right2, Key4);
	t3 = _mm_add_epi32(Right3, Key4);
	t1 = _mm_add_epi32(LeftRotate(t1, 2), t1);
	t2 = _mm_add_epi32(LeftRotate(t2, 2), t2);
	t3 = _mm_add_epi32(LeftRotate(t3, 2), t3);
	t1 = _mm_add_epi32(t1, IMMEDIATE1);
	t2 = _mm_add_epi32(t2, IMMEDIATE1);
	t3 = _mm_add_epi32(t3, IMMEDIATE1);
	Left1 = _mm_xor_si128(Left1, t1);
	Left2 = _mm_xor_si128(Left2, t2);
	Left3 = _mm_xor_si128(Left3, t3);
}

#endif	// MULTI2_OPTIMIZE_FOR_PIPELINE


#ifdef MULTI2_SSE2

void DecodeSSE2(BYTE * __restrict pData, const DWORD dwSize,
				const CMulti2Decoder::SYSKEY * __restrict pWorkKey,
				const WORKKEY * __restrict pPackedWorkKey,
				const CMulti2Decoder::DATKEY * __restrict pInitialCbc)
{
	__assume(dwSize <= 184);

	BYTE * __restrict p = pData;
	__m128i Cbc = _mm_set_epi32(0, 0, pInitialCbc->dwRight, pInitialCbc->dwLeft);

	// スクランブル解除するデータのおよそ99%は184バイト
	if (dwSize == 184) {
		// 192バイト分処理しているが、パケットデータは余分にメモリを確保してあるので問題ない

#ifndef MULTI2_OPTIMIZE_FOR_PIPELINE

		for (int i = 0; i < 6; i++) {
			__m128i Src1, Src2, Left, Right;

			// r2 l2 r1 l1
			Src1 = ByteSwapSSE2(_mm_load_si128((__m128i*)(p +  0)));
			// r4 l4 r3 l3
			Src2 = ByteSwapSSE2(_mm_load_si128((__m128i*)(p + 16)));

			// r2 r1 l2 l1
			__m128i x = _mm_shuffle_epi32(Src1, MM_SHUFFLE4(3, 1, 2, 0));
			// r4 r3 l4 l3
			__m128i y = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(3, 1, 2, 0));

			// l4 l3 l2 l1
			Left  = _mm_unpacklo_epi64(x, y);
			// r4 r3 r2 r1
			Right = _mm_unpackhi_epi64(x, y);

#if defined(__INTEL_COMPILER) && SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (int i = 0 ; i < SCRAMBLE_ROUND ; i++) {
				RoundFuncPi4SSE2(Left, Right, pPackedWorkKey->Key[7]);
				RoundFuncPi3SSE2(Left, Right, pPackedWorkKey->Key[5], pPackedWorkKey->Key[6]);
				RoundFuncPi2SSE2(Left, Right, pPackedWorkKey->Key[4]);
				RoundFuncPi1SSE2(Left, Right);
				RoundFuncPi4SSE2(Left, Right, pPackedWorkKey->Key[3]);
				RoundFuncPi3SSE2(Left, Right, pPackedWorkKey->Key[1], pPackedWorkKey->Key[2]);
				RoundFuncPi2SSE2(Left, Right, pPackedWorkKey->Key[0]);
				RoundFuncPi1SSE2(Left, Right);
			}

			// r2 l2 r1 l1
			x = _mm_unpacklo_epi32(Left, Right);
			// r4 l4 r3 l3
			y = _mm_unpackhi_epi32(Left, Right);

			x = _mm_xor_si128(x, _mm_unpacklo_epi64(Cbc, Src1));
			Cbc = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(1, 0, 3, 2));
			y = _mm_xor_si128(y, _mm_unpackhi_epi64(Src1, Cbc));

			_mm_store_si128((__m128i*)(p +  0), ByteSwapSSE2(x));
			_mm_store_si128((__m128i*)(p + 16), ByteSwapSSE2(y));

			p += 32;
		}

#else	// MULTI2_OPTIMIZE_FOR_PIPELINE

		// パイプラインで処理しやすいように並列化したバージョン
		for (int i = 0; i < 2; i++) {
			__m128i Src1, Src2, Src3, Src4, Src5, Src6;
			__m128i Left1, Right1, Left2, Right2, Left3, Right3;
			__m128i x1, y1, x2, y2, x3, y3;

			Src1 = _mm_load_si128((__m128i*)(p +  0));
			Src2 = _mm_load_si128((__m128i*)(p + 16));
			Src3 = _mm_load_si128((__m128i*)(p + 32));
			Src4 = _mm_load_si128((__m128i*)(p + 48));
			Src5 = _mm_load_si128((__m128i*)(p + 64));
			Src6 = _mm_load_si128((__m128i*)(p + 80));

			Src1 = ByteSwapSSE2(Src1);
			Src2 = ByteSwapSSE2(Src2);
			Src3 = ByteSwapSSE2(Src3);
			Src4 = ByteSwapSSE2(Src4);
			Src5 = ByteSwapSSE2(Src5);
			Src6 = ByteSwapSSE2(Src6);

			x1 = _mm_shuffle_epi32(Src1, MM_SHUFFLE4(3, 1, 2, 0));
			y1 = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(3, 1, 2, 0));
			x2 = _mm_shuffle_epi32(Src3, MM_SHUFFLE4(3, 1, 2, 0));
			y2 = _mm_shuffle_epi32(Src4, MM_SHUFFLE4(3, 1, 2, 0));
			x3 = _mm_shuffle_epi32(Src5, MM_SHUFFLE4(3, 1, 2, 0));
			y3 = _mm_shuffle_epi32(Src6, MM_SHUFFLE4(3, 1, 2, 0));

			Left1  = _mm_unpacklo_epi64(x1, y1);
			Right1 = _mm_unpackhi_epi64(x1, y1);
			Left2  = _mm_unpacklo_epi64(x2, y2);
			Right2 = _mm_unpackhi_epi64(x2, y2);
			Left3  = _mm_unpacklo_epi64(x3, y3);
			Right3 = _mm_unpackhi_epi64(x3, y3);

#if defined(__INTEL_COMPILER) && SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (int i = 0 ; i < SCRAMBLE_ROUND ; i++) {
				RoundFuncPi4SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[7]);
				RoundFuncPi3SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3,
								   pPackedWorkKey->Key[5], pPackedWorkKey->Key[6]);
				RoundFuncPi2SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[4]);
				RoundFuncPi1SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3);
				RoundFuncPi4SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[3]);
				RoundFuncPi3SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3,
								   pPackedWorkKey->Key[1], pPackedWorkKey->Key[2]);
				RoundFuncPi2SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[0]);
				RoundFuncPi1SSE2_3(Left1, Right1, Left2, Right2, Left3, Right3);
			}

			x1 = _mm_unpacklo_epi32(Left1, Right1);
			y1 = _mm_unpackhi_epi32(Left1, Right1);
			x2 = _mm_unpacklo_epi32(Left2, Right2);
			y2 = _mm_unpackhi_epi32(Left2, Right2);
			x3 = _mm_unpacklo_epi32(Left3, Right3);
			y3 = _mm_unpackhi_epi32(Left3, Right3);

			Src2 = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(1, 0, 3, 2));
			Src4 = _mm_shuffle_epi32(Src4, MM_SHUFFLE4(1, 0, 3, 2));
			Src6 = _mm_shuffle_epi32(Src6, MM_SHUFFLE4(1, 0, 3, 2));
			x1 = _mm_xor_si128(x1, _mm_unpacklo_epi64(Cbc, Src1));
			y1 = _mm_xor_si128(y1, _mm_unpackhi_epi64(Src1, Src2));
			x2 = _mm_xor_si128(x2, _mm_unpacklo_epi64(Src2, Src3));
			y2 = _mm_xor_si128(y2, _mm_unpackhi_epi64(Src3, Src4));
			x3 = _mm_xor_si128(x3, _mm_unpacklo_epi64(Src4, Src5));
			y3 = _mm_xor_si128(y3, _mm_unpackhi_epi64(Src5, Src6));
			Cbc = Src6;

			x1 = ByteSwapSSE2(x1);
			y1 = ByteSwapSSE2(y1);
			x2 = ByteSwapSSE2(x2);
			y2 = ByteSwapSSE2(y2);
			x3 = ByteSwapSSE2(x3);
			y3 = ByteSwapSSE2(y3);

			_mm_store_si128((__m128i*)(p +  0), x1);
			_mm_store_si128((__m128i*)(p + 16), y1);
			_mm_store_si128((__m128i*)(p + 32), x2);
			_mm_store_si128((__m128i*)(p + 48), y2);
			_mm_store_si128((__m128i*)(p + 64), x3);
			_mm_store_si128((__m128i*)(p + 80), y3);

			p += 32 * 3;
		}

#endif	// MULTI2_OPTIMIZE_FOR_PIPELINE

		return;
	}

	// CBCモード
	for (BYTE *pEnd = p + (dwSize & 0xFFFFFFE0UL); p < pEnd; p += 32) {
		__m128i Src1, Src2, Left, Right;

		// r2 l2 r1 l1
		Src1 = ByteSwapSSE2(_mm_loadu_si128((__m128i*)p));
		// r4 l4 r3 l3
		Src2 = ByteSwapSSE2(_mm_loadu_si128((__m128i*)(p + 16)));

		// r2 r1 l2 l1
		__m128i x = _mm_shuffle_epi32(Src1, MM_SHUFFLE4(3, 1, 2, 0));
		// r4 r3 l4 l3
		__m128i y = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(3, 1, 2, 0));

		// l4 l3 l2 l1
		Left  = _mm_unpacklo_epi64(x, y);
		// r4 r3 r2 r1
		Right = _mm_unpackhi_epi64(x, y);

		for (int i = 0 ; i < SCRAMBLE_ROUND ; i++) {
			RoundFuncPi4SSE2(Left, Right, pPackedWorkKey->Key[7]);
			RoundFuncPi3SSE2(Left, Right, pPackedWorkKey->Key[5], pPackedWorkKey->Key[6]);
			RoundFuncPi2SSE2(Left, Right, pPackedWorkKey->Key[4]);
			RoundFuncPi1SSE2(Left, Right);
			RoundFuncPi4SSE2(Left, Right, pPackedWorkKey->Key[3]);
			RoundFuncPi3SSE2(Left, Right, pPackedWorkKey->Key[1], pPackedWorkKey->Key[2]);
			RoundFuncPi2SSE2(Left, Right, pPackedWorkKey->Key[0]);
			RoundFuncPi1SSE2(Left, Right);
		}

		// r2 l2 r1 l1
		x = _mm_unpacklo_epi32(Left, Right);
		// r4 l4 r3 l3
		y = _mm_unpackhi_epi32(Left, Right);

#if 0
		Cbc = _mm_or_si128(_mm_slli_si128(Src1, 8), Cbc);
		x = _mm_xor_si128(x, Cbc);

		Cbc = _mm_or_si128(_mm_slli_si128(Src2, 8), _mm_srli_si128(Src1, 8));
		y = _mm_xor_si128(y, Cbc);

		Cbc = _mm_srli_si128(Src2, 8);
#else
		x = _mm_xor_si128(x, _mm_unpacklo_epi64(Cbc, Src1));
		Cbc = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(1, 0, 3, 2));
		y = _mm_xor_si128(y, _mm_unpackhi_epi64(Src1, Cbc));
#endif

		_mm_storeu_si128((__m128i*)p,        ByteSwapSSE2(x));
		_mm_storeu_si128((__m128i*)(p + 16), ByteSwapSSE2(y));
	}

	DWORD CbcLeft, CbcRight;
	__declspec(align(16)) DWORD Data[4];
	_mm_store_si128((__m128i*)Data, Cbc);
	CbcLeft  = Data[0];
	CbcRight = Data[1];

	for (BYTE *pEnd = p + (dwSize & 0x00000018UL); p < pEnd; p += 8) {
		DWORD Src1, Src2, Left, Right;

		Src1 = _byteswap_ulong(*(DWORD*)(p + 0));
		Src2 = _byteswap_ulong(*(DWORD*)(p + 4));
		Left  = Src1;
		Right = Src2;

		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi4(Left, Right, pWorkKey->dwKey8);
			RoundFuncPi3(Left, Right, pWorkKey->dwKey6, pWorkKey->dwKey7);
			RoundFuncPi2(Left, Right, pWorkKey->dwKey5);
			RoundFuncPi1(Left, Right);
			RoundFuncPi4(Left, Right, pWorkKey->dwKey4);
			RoundFuncPi3(Left, Right, pWorkKey->dwKey2, pWorkKey->dwKey3);
			RoundFuncPi2(Left, Right, pWorkKey->dwKey1);
			RoundFuncPi1(Left, Right);
		}

		*(DWORD*)(p + 0) = _byteswap_ulong(Left  ^ CbcLeft);
		*(DWORD*)(p + 4) = _byteswap_ulong(Right ^ CbcRight);
		CbcLeft  = Src1;
		CbcRight = Src2;
	}

	// OFBモード
	DWORD RemainSize = dwSize & 0x00000007UL;
	if (RemainSize) {
		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi1(CbcLeft, CbcRight);
			RoundFuncPi2(CbcLeft, CbcRight, pWorkKey->dwKey1);
			RoundFuncPi3(CbcLeft, CbcRight, pWorkKey->dwKey2, pWorkKey->dwKey3);
			RoundFuncPi4(CbcLeft, CbcRight, pWorkKey->dwKey4);
			RoundFuncPi1(CbcLeft, CbcRight);
			RoundFuncPi2(CbcLeft, CbcRight, pWorkKey->dwKey5);
			RoundFuncPi3(CbcLeft, CbcRight, pWorkKey->dwKey6, pWorkKey->dwKey7);
			RoundFuncPi4(CbcLeft, CbcRight, pWorkKey->dwKey8);
		}

		BYTE Remain[8];
		*(DWORD*)(Remain + 0) = CbcLeft;
		*(DWORD*)(Remain + 4) = CbcRight;
		switch (RemainSize) {
		default: __assume(0);
		case 7: p[6] ^= Remain[5];
		case 6: p[5] ^= Remain[6];
		case 5: p[4] ^= Remain[7];
		case 4: p[3] ^= Remain[0];
		case 3: p[2] ^= Remain[1];
		case 2: p[1] ^= Remain[2];
		case 1: p[0] ^= Remain[3];
		}
	}
}

#endif	// MULTI2_SSE2


#ifdef MULTI2_SSSE3

static __forceinline __m128i ByteSwapSSSE3(const __m128i &Value)
{
	return _mm_shuffle_epi8(Value, ByteSwapMask);
}

#define RoundFuncPi1SSSE3 RoundFuncPi1SSE2
#define RoundFuncPi2SSSE3 RoundFuncPi2SSE2
#define RoundFuncPi4SSSE3 RoundFuncPi4SSE2

static __forceinline void RoundFuncPi3SSSE3(__m128i &Left, __m128i &Right,
											const __m128i &Key2, const __m128i &Key3)
{
	__m128i t;

	t = _mm_add_epi32(Left, Key2);
	t = _mm_add_epi32(_mm_add_epi32(LeftRotate(t, 2), t), IMMEDIATE1);
	t = _mm_xor_si128(_mm_shuffle_epi8(t, Rotate8Mask), t);
	t = _mm_add_epi32(t, Key3);
	t = _mm_sub_epi32(LeftRotate(t, 1), t);
	t = _mm_xor_si128(_mm_shuffle_epi8(t, Rotate16Mask), _mm_or_si128(t, Left));

	Right = _mm_xor_si128(Right, t);
}

#ifdef MULTI2_OPTIMIZE_FOR_PIPELINE

#define RoundFuncPi1SSSE3_3 RoundFuncPi1SSE2_3
#define RoundFuncPi2SSSE3_3 RoundFuncPi2SSE2_3
#define RoundFuncPi4SSSE3_3 RoundFuncPi4SSE2_3

static __forceinline void RoundFuncPi3SSSE3_3(__m128i &Left1, __m128i &Right1,
											  __m128i &Left2, __m128i &Right2,
											  __m128i &Left3, __m128i &Right3,
											  const __m128i &Key2, const __m128i &Key3)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(Left1, Key2);
	t2 = _mm_add_epi32(Left2, Key2);
	t3 = _mm_add_epi32(Left3, Key2);
	t1 = _mm_add_epi32(LeftRotate(t1, 2), t1);
	t2 = _mm_add_epi32(LeftRotate(t2, 2), t2);
	t3 = _mm_add_epi32(LeftRotate(t3, 2), t3);
	t1 = _mm_add_epi32(t1, IMMEDIATE1);
	t2 = _mm_add_epi32(t2, IMMEDIATE1);
	t3 = _mm_add_epi32(t3, IMMEDIATE1);
	t1 = _mm_xor_si128(_mm_shuffle_epi8(t1, Rotate8Mask), t1);
	t2 = _mm_xor_si128(_mm_shuffle_epi8(t2, Rotate8Mask), t2);
	t3 = _mm_xor_si128(_mm_shuffle_epi8(t3, Rotate8Mask), t3);
	t1 = _mm_add_epi32(t1, Key3);
	t2 = _mm_add_epi32(t2, Key3);
	t3 = _mm_add_epi32(t3, Key3);
	t1 = _mm_sub_epi32(LeftRotate(t1, 1), t1);
	t2 = _mm_sub_epi32(LeftRotate(t2, 1), t2);
	t3 = _mm_sub_epi32(LeftRotate(t3, 1), t3);
	t1 = _mm_xor_si128(_mm_shuffle_epi8(t1, Rotate16Mask), _mm_or_si128(t1, Left1));
	t2 = _mm_xor_si128(_mm_shuffle_epi8(t2, Rotate16Mask), _mm_or_si128(t2, Left2));
	t3 = _mm_xor_si128(_mm_shuffle_epi8(t3, Rotate16Mask), _mm_or_si128(t3, Left3));
	Right1 = _mm_xor_si128(Right1, t1);
	Right2 = _mm_xor_si128(Right2, t2);
	Right3 = _mm_xor_si128(Right3, t3);
}

#endif	// MULTI2_OPTIMIZE_FOR_PIPELINE


void DecodeSSSE3(BYTE * __restrict pData, const DWORD dwSize,
				 const CMulti2Decoder::SYSKEY * __restrict pWorkKey,
				 const WORKKEY * __restrict pPackedWorkKey,
				 const CMulti2Decoder::DATKEY * __restrict pInitialCbc)
{
	__assume(dwSize <= 184);

	BYTE * __restrict p = pData;
	__m128i Cbc = ByteSwapSSSE3(_mm_set_epi32(0, 0, pInitialCbc->dwRight, pInitialCbc->dwLeft));

	// スクランブル解除するデータのおよそ99%は184バイト
	if (dwSize == 184) {
		// 192バイト分処理しているが、パケットデータは余分にメモリを確保してあるので問題ない

#ifndef MULTI2_OPTIMIZE_FOR_PIPELINE

		for (int i = 0; i < 6; i++) {
			__m128i Src1, Src2, Left, Right, x, y;

			// r2 l2 r1 l1
			Src1 = _mm_load_si128((__m128i*)(p +  0));
			// r4 l4 r3 l3
			Src2 = _mm_load_si128((__m128i*)(p + 16));

			// r2 r1 l2 l1
			x = _mm_shuffle_epi8(Src1, SrcSwapMask);
			// r4 r3 l4 l3
			y = _mm_shuffle_epi8(Src2, SrcSwapMask);

			// l4 l3 l2 l1
			Left  = _mm_unpacklo_epi64(x, y);
			// r4 r3 r2 r1
			Right = _mm_unpackhi_epi64(x, y);

#if defined(__INTEL_COMPILER) && SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (int i = 0 ; i < SCRAMBLE_ROUND ; i++) {
				RoundFuncPi4SSSE3(Left, Right, pPackedWorkKey->Key[7]);
				RoundFuncPi3SSSE3(Left, Right, pPackedWorkKey->Key[5], pPackedWorkKey->Key[6]);
				RoundFuncPi2SSSE3(Left, Right, pPackedWorkKey->Key[4]);
				RoundFuncPi1SSSE3(Left, Right);
				RoundFuncPi4SSSE3(Left, Right, pPackedWorkKey->Key[3]);
				RoundFuncPi3SSSE3(Left, Right, pPackedWorkKey->Key[1], pPackedWorkKey->Key[2]);
				RoundFuncPi2SSSE3(Left, Right, pPackedWorkKey->Key[0]);
				RoundFuncPi1SSSE3(Left, Right);
			}

			// r2 l2 r1 l1
			x = ByteSwapSSSE3(_mm_unpacklo_epi32(Left, Right));
			// r4 l4 r3 l3
			y = ByteSwapSSSE3(_mm_unpackhi_epi32(Left, Right));

			x = _mm_xor_si128(x, _mm_unpacklo_epi64(Cbc, Src1));
			Cbc = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(1, 0, 3, 2));
			y = _mm_xor_si128(y, _mm_unpackhi_epi64(Src1, Cbc));

			_mm_store_si128((__m128i*)(p +  0), x);
			_mm_store_si128((__m128i*)(p + 16), y);

			p += 32;
		}

#else	// MULTI2_OPTIMIZE_FOR_PIPELINE

		// パイプラインで処理しやすいように並列化したバージョン
		for (int i = 0; i < 2; i++) {
			__m128i Src1, Src2, Src3, Src4, Src5, Src6;
			__m128i Left1, Right1, Left2, Right2, Left3, Right3;
			__m128i x1, y1, x2, y2, x3, y3;

			Src1 = _mm_load_si128((__m128i*)(p +  0));
			Src2 = _mm_load_si128((__m128i*)(p + 16));
			Src3 = _mm_load_si128((__m128i*)(p + 32));
			Src4 = _mm_load_si128((__m128i*)(p + 48));
			Src5 = _mm_load_si128((__m128i*)(p + 64));
			Src6 = _mm_load_si128((__m128i*)(p + 80));

			x1 = _mm_shuffle_epi8(Src1, SrcSwapMask);
			y1 = _mm_shuffle_epi8(Src2, SrcSwapMask);
			x2 = _mm_shuffle_epi8(Src3, SrcSwapMask);
			y2 = _mm_shuffle_epi8(Src4, SrcSwapMask);
			x3 = _mm_shuffle_epi8(Src5, SrcSwapMask);
			y3 = _mm_shuffle_epi8(Src6, SrcSwapMask);

			Left1  = _mm_unpacklo_epi64(x1, y1);
			Right1 = _mm_unpackhi_epi64(x1, y1);
			Left2  = _mm_unpacklo_epi64(x2, y2);
			Right2 = _mm_unpackhi_epi64(x2, y2);
			Left3  = _mm_unpacklo_epi64(x3, y3);
			Right3 = _mm_unpackhi_epi64(x3, y3);

#if defined(__INTEL_COMPILER) && SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (int i = 0 ; i < SCRAMBLE_ROUND ; i++) {
				RoundFuncPi4SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[7]);
				RoundFuncPi3SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3,
								    pPackedWorkKey->Key[5], pPackedWorkKey->Key[6]);
				RoundFuncPi2SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[4]);
				RoundFuncPi1SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3);
				RoundFuncPi4SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[3]);
				RoundFuncPi3SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3,
								    pPackedWorkKey->Key[1], pPackedWorkKey->Key[2]);
				RoundFuncPi2SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3, pPackedWorkKey->Key[0]);
				RoundFuncPi1SSSE3_3(Left1, Right1, Left2, Right2, Left3, Right3);
			}

			x1 = _mm_unpacklo_epi32(Left1, Right1);
			y1 = _mm_unpackhi_epi32(Left1, Right1);
			x2 = _mm_unpacklo_epi32(Left2, Right2);
			y2 = _mm_unpackhi_epi32(Left2, Right2);
			x3 = _mm_unpacklo_epi32(Left3, Right3);
			y3 = _mm_unpackhi_epi32(Left3, Right3);

			x1 = ByteSwapSSSE3(x1);
			y1 = ByteSwapSSSE3(y1);
			x2 = ByteSwapSSSE3(x2);
			y2 = ByteSwapSSSE3(y2);
			x3 = ByteSwapSSSE3(x3);
			y3 = ByteSwapSSSE3(y3);

			Src2 = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(1, 0, 3, 2));
			Src4 = _mm_shuffle_epi32(Src4, MM_SHUFFLE4(1, 0, 3, 2));
			Src6 = _mm_shuffle_epi32(Src6, MM_SHUFFLE4(1, 0, 3, 2));
			x1 = _mm_xor_si128(x1, _mm_unpacklo_epi64(Cbc, Src1));
			y1 = _mm_xor_si128(y1, _mm_unpackhi_epi64(Src1, Src2));
			x2 = _mm_xor_si128(x2, _mm_unpacklo_epi64(Src2, Src3));
			y2 = _mm_xor_si128(y2, _mm_unpackhi_epi64(Src3, Src4));
			x3 = _mm_xor_si128(x3, _mm_unpacklo_epi64(Src4, Src5));
			y3 = _mm_xor_si128(y3, _mm_unpackhi_epi64(Src5, Src6));
			Cbc = Src6;

			_mm_store_si128((__m128i*)(p +  0), x1);
			_mm_store_si128((__m128i*)(p + 16), y1);
			_mm_store_si128((__m128i*)(p + 32), x2);
			_mm_store_si128((__m128i*)(p + 48), y2);
			_mm_store_si128((__m128i*)(p + 64), x3);
			_mm_store_si128((__m128i*)(p + 80), y3);

			p += 32 * 3;
		}

#endif	// MULTI2_OPTIMIZE_FOR_PIPELINE

		return;
	}

	// CBCモード
	for (BYTE *pEnd = p + (dwSize & 0xFFFFFFE0UL); p < pEnd; p += 32) {
		__m128i Src1, Src2, Left, Right, x, y;

		// r2 l2 r1 l1
		Src1 = _mm_loadu_si128((__m128i*)p);
		// r4 l4 r3 l3
		Src2 = _mm_loadu_si128((__m128i*)(p + 16));

		// r2 r1 l2 l1
		x = _mm_shuffle_epi8(Src1, SrcSwapMask);
		// r4 r3 l4 l3
		y = _mm_shuffle_epi8(Src2, SrcSwapMask);

		// l4 l3 l2 l1
		Left  = _mm_unpacklo_epi64(x, y);
		// r4 r3 r2 r1
		Right = _mm_unpackhi_epi64(x, y);

		for (int i = 0 ; i < SCRAMBLE_ROUND ; i++) {
			RoundFuncPi4SSSE3(Left, Right, pPackedWorkKey->Key[7]);
			RoundFuncPi3SSSE3(Left, Right, pPackedWorkKey->Key[5], pPackedWorkKey->Key[6]);
			RoundFuncPi2SSSE3(Left, Right, pPackedWorkKey->Key[4]);
			RoundFuncPi1SSSE3(Left, Right);
			RoundFuncPi4SSSE3(Left, Right, pPackedWorkKey->Key[3]);
			RoundFuncPi3SSSE3(Left, Right, pPackedWorkKey->Key[1], pPackedWorkKey->Key[2]);
			RoundFuncPi2SSSE3(Left, Right, pPackedWorkKey->Key[0]);
			RoundFuncPi1SSSE3(Left, Right);
		}

		// r2 l2 r1 l1
		x = ByteSwapSSSE3(_mm_unpacklo_epi32(Left, Right));
		// r4 l4 r3 l3
		y = ByteSwapSSSE3(_mm_unpackhi_epi32(Left, Right));

		x = _mm_xor_si128(x, _mm_unpacklo_epi64(Cbc, Src1));
		Cbc = _mm_shuffle_epi32(Src2, MM_SHUFFLE4(1, 0, 3, 2));
		y = _mm_xor_si128(y, _mm_unpackhi_epi64(Src1, Cbc));

		_mm_storeu_si128((__m128i*)p,        x);
		_mm_storeu_si128((__m128i*)(p + 16), y);
	}

	DWORD CbcLeft, CbcRight;
	__declspec(align(16)) DWORD Data[4];
	_mm_store_si128((__m128i*)Data, ByteSwapSSSE3(Cbc));
	CbcLeft  = Data[0];
	CbcRight = Data[1];

	for (BYTE *pEnd = p + (dwSize & 0x00000018UL); p < pEnd; p += 8) {
		DWORD Src1, Src2, Left, Right;

		Src1 = _byteswap_ulong(*(DWORD*)(p + 0));
		Src2 = _byteswap_ulong(*(DWORD*)(p + 4));
		Left  = Src1;
		Right = Src2;

		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi4(Left, Right, pWorkKey->dwKey8);
			RoundFuncPi3(Left, Right, pWorkKey->dwKey6, pWorkKey->dwKey7);
			RoundFuncPi2(Left, Right, pWorkKey->dwKey5);
			RoundFuncPi1(Left, Right);
			RoundFuncPi4(Left, Right, pWorkKey->dwKey4);
			RoundFuncPi3(Left, Right, pWorkKey->dwKey2, pWorkKey->dwKey3);
			RoundFuncPi2(Left, Right, pWorkKey->dwKey1);
			RoundFuncPi1(Left, Right);
		}

		*(DWORD*)(p + 0) = _byteswap_ulong(Left  ^ CbcLeft);
		*(DWORD*)(p + 4) = _byteswap_ulong(Right ^ CbcRight);
		CbcLeft  = Src1;
		CbcRight = Src2;
	}

	// OFBモード
	DWORD RemainSize = dwSize & 0x00000007UL;
	if (RemainSize) {
		for (int Round = 0 ; Round < SCRAMBLE_ROUND ; Round++) {
			RoundFuncPi1(CbcLeft, CbcRight);
			RoundFuncPi2(CbcLeft, CbcRight, pWorkKey->dwKey1);
			RoundFuncPi3(CbcLeft, CbcRight, pWorkKey->dwKey2, pWorkKey->dwKey3);
			RoundFuncPi4(CbcLeft, CbcRight, pWorkKey->dwKey4);
			RoundFuncPi1(CbcLeft, CbcRight);
			RoundFuncPi2(CbcLeft, CbcRight, pWorkKey->dwKey5);
			RoundFuncPi3(CbcLeft, CbcRight, pWorkKey->dwKey6, pWorkKey->dwKey7);
			RoundFuncPi4(CbcLeft, CbcRight, pWorkKey->dwKey8);
		}

		BYTE Remain[8];
		*(DWORD*)(Remain + 0) = CbcLeft;
		*(DWORD*)(Remain + 4) = CbcRight;
		switch (RemainSize) {
		default: __assume(0);
		case 7: p[6] ^= Remain[5];
		case 6: p[5] ^= Remain[6];
		case 5: p[4] ^= Remain[7];
		case 4: p[3] ^= Remain[0];
		case 3: p[2] ^= Remain[1];
		case 2: p[1] ^= Remain[2];
		case 1: p[0] ^= Remain[3];
		}
	}
}

#endif	// MULTI2_SSSE3

}	// namespace Multi2DecoderSIMD
