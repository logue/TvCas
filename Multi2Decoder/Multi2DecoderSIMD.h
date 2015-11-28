#ifndef MULTI2_DECODER_SIMD
#define MULTI2_DECODER_SIMD


namespace Multi2DecoderSIMD
{

	struct WORKKEY;

	bool IsSSE2Available();
	bool IsSSSE3Available();
	bool Initialize();

	void AllocWorkKey(WORKKEY **ppWorkKeyOdd, WORKKEY **ppWorkKeyEven);
	void FreeWorkKey(WORKKEY **ppWorkKeyOdd, WORKKEY **ppWorkKeyEven);
	void SetWorkKey(WORKKEY *pWorkKey, const CMulti2Decoder::SYSKEY &SrcKey);

	void Decode(BYTE * __restrict pData, const DWORD dwSize,
				const CMulti2Decoder::SYSKEY * __restrict pWorkKey,
				const WORKKEY * __restrict pPackedWorkKey,
				const CMulti2Decoder::DATKEY * __restrict pInitialCbc);
#ifdef MULTI2_SSE2
	void DecodeSSE2(BYTE * __restrict pData, const DWORD dwSize,
					const CMulti2Decoder::SYSKEY * __restrict pWorkKey,
					const WORKKEY * __restrict pPackedWorkKey,
					const CMulti2Decoder::DATKEY * __restrict pInitialCbc);
#endif
#ifdef MULTI2_SSSE3
	void DecodeSSSE3(BYTE * __restrict pData, const DWORD dwSize,
					 const CMulti2Decoder::SYSKEY * __restrict pWorkKey,
					 const WORKKEY * __restrict pPackedWorkKey,
					 const CMulti2Decoder::DATKEY * __restrict pInitialCbc);
#endif

}	// namespace Multi2DecoderSIMD


#endif
