// TsDescrambler.h: CTsDescrambler クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////

#pragma once


#include <deque>
#include <vector>
#include <map>
#include "MediaDecoder.h"
#include "TsStream.h"
#include "TsTable.h"
#include "TsUtilClass.h"
#include "CasCard.h"


class CEcmProcessor;
class CEmmProcessor;

class CCasAccess
{
public:
	virtual ~CCasAccess() {}
	virtual bool Process(CCasCard *pCasCard) = 0;
};

class CCasAccessQueue : public CBonBaseClass
{
	std::deque<CCasAccess*> m_Queue;
	CCasCard *m_pCasCard;
	CCardReader::ReaderType m_ReaderType;
	LPCTSTR m_pszReaderName;
	HANDLE m_hThread;
	CLocalEvent m_Event;
	volatile bool m_bAvailable;
	volatile bool m_bKillEvent;
	volatile bool m_bStartEvent;
	CCriticalLock m_Lock;

	static unsigned int __stdcall CasAccessThread(LPVOID lpParameter);

public:
	CCasAccessQueue(CCasCard *pCasCard);
	~CCasAccessQueue();
	void Clear();
	bool Enqueue(CCasAccess *pAccess);
	bool BeginCasThread(CCardReader::ReaderType ReaderType, LPCTSTR pszReaderName);
	bool EndCasThread();
};


/////////////////////////////////////////////////////////////////////////////
// MULTI2スクランブル解除(ECMによりペイロードのスクランブルを解除する)
/////////////////////////////////////////////////////////////////////////////
// Input	#0	: CTsPacket		暗号TSパケット
// Output	#0	: CTsPacket		平分TSパケット
/////////////////////////////////////////////////////////////////////////////

class CTsDescrambler : public CMediaDecoder
{
public:
	enum {
		EVENT_EMM_PROCESSED		= 0x00000001UL,
		EVENT_EMM_ERROR			= 0x00000002UL,
		EVENT_ECM_ERROR			= 0x00000003UL,
		EVENT_ECM_REFUSED		= 0x00000004UL,
		EVENT_CARD_READER_HUNG	= 0x00000005UL
	};

	struct EcmErrorInfo {
		LPCTSTR pszText;
		WORD EcmPID;
	};

	struct EmmErrorInfo {
		LPCTSTR pszText;
	};

	enum InstructionType {
		INSTRUCTION_NORMAL,
		INSTRUCTION_SSE2,
		INSTRUCTION_SSSE3
	};

	enum ContractStatus {
		CONTRACT_CONTRACTED,
		CONTRACT_UNCONTRACTED,
		CONTRACT_UNKNOWN,
		CONTRACT_ERROR
	};

	CTsDescrambler(IEventHandler *pEventHandler = NULL);
	virtual ~CTsDescrambler();

// CMediaDecoder
	virtual void Reset(void);
	virtual const bool InputMedia(CMediaData *pMediaData, const DWORD dwInputIndex = 0UL);

// CTsDescrambler
	bool EnableDescramble(bool bDescramble);
	bool IsDescrambleEnabled() const;
	bool EnableEmmProcess(bool bEnable);
	bool IsEmmProcessEnabled() const;

	bool OpenCasCard(CCardReader::ReaderType ReaderType = CCardReader::READER_SCARD, LPCTSTR pszReaderName = NULL);
	void CloseCasCard(void);
	bool IsCasCardOpen() const;
	CCardReader::ReaderType GetCardReaderType() const;
	LPCTSTR GetCardReaderName() const;
	bool GetCasCardInfo(CCasCard::CasCardInfo *pInfo) const;
	bool GetCasCardID(BYTE *pCardID) const;
	int FormatCasCardID(LPTSTR pszText,int MaxLength) const;
	char GetCasCardManufacturerID() const;
	BYTE GetCasCardVersion() const;

	ULONGLONG GetInputPacketCount(void) const;
	ULONGLONG GetScramblePacketCount(void) const;
	void ResetScramblePacketCount(void);

	bool SetTargetServiceID(WORD ServiceID = 0);
	WORD GetTargetServiceID() const;
	WORD GetEcmPIDByServiceID(const WORD ServiceID) const;
	bool SendCasCommand(const BYTE *pSendData, DWORD SendSize, BYTE *pRecvData, DWORD *pRecvSize);

	ContractStatus GetContractStatus(WORD NetworkID, WORD ServiceID, const SYSTEMTIME *pTime = NULL);
	ContractStatus GetContractPeriod(WORD NetworkID, WORD ServiceID, SYSTEMTIME *pTime);
	bool HasContractInfo(WORD NetworkID, WORD ServiceID) const;

	bool SetInstruction(InstructionType Type);
	InstructionType GetInstruction() const;
	static bool IsSSE2Available();
	static bool IsSSSE3Available();

protected:
	class CEsProcessor;

	static void CALLBACK OnPatUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam);
	static void CALLBACK OnPmtUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam);
	static void CALLBACK OnCatUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam);
	static void CALLBACK OnSdtUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam);

	int GetServiceIndexByID(WORD ServiceID) const;

#ifdef _DEBUG
	void PrintStatus(void) const;
#endif

	bool m_bDescramble;
	bool m_bProcessEmm;
	CTsPidMapManager m_PidMapManager;
	CCasCard m_CasCard;
	CCasAccessQueue m_Queue;

	WORD m_CurTransportStreamID;
	WORD m_DescrambleServiceID;

	struct TAG_SERVICEINFO {
		bool bTarget;
		WORD ServiceID;
		WORD PmtPID;
		WORD EcmPID;
		std::vector<WORD> EsPIDList;
	};
	std::vector<TAG_SERVICEINFO> m_ServiceList;

	struct ServiceContractInfo {
		std::vector<BYTE> VerificationInfo;
	};
	typedef std::map<DWORD, ServiceContractInfo> ServiceContractMap;
	static DWORD GetServiceContractMapKey(WORD NetworkID, WORD ServiceID)
	{
		return ((DWORD)NetworkID << 16) | (DWORD)ServiceID;
	}
	ServiceContractMap m_ServiceContractList;
	ContractStatus CheckContractStatus(const ServiceContractInfo &Info, WORD Date);

	ULONGLONG m_InputPacketCount;
	ULONGLONG m_ScramblePacketCount;

	InstructionType m_Instruction;

	WORD m_EmmPID;

	friend class CEcmProcessor;
	friend class CEmmProcessor;
	friend class CDescramblePmtTable;
};
