// TsDescrambler.cpp: CTsDescrambler クラスのインプリメンテーション
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Common.h"
#include "TsDescrambler.h"
#include "Multi2Decoder.h"
#include "TsEncode.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#pragma intrinsic(memcmp)


// EMM処理を行う期間
#define EMM_PROCESS_TIME	(7 * 24)

#if EMMLOG
#include <fstream>
#define LOGNAME		"Emmlog.txt"
#endif

// ECM処理内部クラス
class CEcmProcessor
	: public CPsiSingleTable
	, public CDynamicReferenceable
{
public:
	CEcmProcessor(CTsDescrambler *pDescrambler);
#if EMMLOG
	virtual ~CEmmProcessor(){ emmlog << "// Log close()" << std::endl; emmlog.close(); }
#endif
	const bool DescramblePacket(CTsPacket *pTsPacket);
	const bool SetScrambleKey(CCasCard *pCasCard, const BYTE *pEcmData, DWORD EcmSize);

	static void ResetCardReaderStatus() { m_bCardReaderHung = false; }

private:
// CTsPidMapTarget
	void OnPidMapped(const WORD wPID, const PVOID pParam) override;
	void OnPidUnmapped(const WORD wPID) override;

// CPsiSingleTable
	const bool OnTableUpdate(const CPsiSection *pCurSection, const CPsiSection *pOldSection) override;

// CEcmProcessor
	void OnCardReaderHung();

	CTsDescrambler *m_pDescrambler;
	CMulti2Decoder m_Multi2Decoder;
	WORD m_EcmPID;
	CLocalEvent m_EcmProcessEvent;
	CCriticalLock m_Multi2Lock;

	bool m_bEcmReceived;
	bool m_bLastEcmSucceed;
	bool m_bOddKeyValid;
	bool m_bEvenKeyValid;
	int m_LastChangedKey;
	bool m_bEcmErrorSent;
	BYTE m_LastScramblingCtrl;
	BYTE m_LastKsData[16];

	static DWORD m_EcmErrorCount;
	static bool m_bCardReaderHung;
};

// EMM処理内部クラス
class CEmmProcessor
	: public CPsiStreamTable
	, public CDynamicReferenceable
{
public:
	CEmmProcessor(CTsDescrambler *pDescrambler);
	const bool ProcessEmm(CCasCard *pCasCard, const BYTE *pData, const DWORD DataSize);

private:
// CTsPidMapTarget
	void OnPidMapped(const WORD wPID, const PVOID pParam) override;
	void OnPidUnmapped(const WORD wPID) override;

// CPsiStreamTable
	const bool OnTableUpdate(const CPsiSection *pCurSection) override;

	CTsDescrambler *m_pDescrambler;
#if EMMLOG
	std::ofstream emmlog;
#endif
};

// ESスクランブル解除内部クラス
class CTsDescrambler::CEsProcessor : public CTsPidMapTarget
{
public:
	CEsProcessor(CEcmProcessor *pEcmProcessor);
	virtual ~CEsProcessor();
	const CEcmProcessor *GetEcmProcessor() const { return m_pEcmProcessor; }

private:
// CTsPidMapTarget
	const bool StorePacket(const CTsPacket *pPacket) override;
	void OnPidMapped(const WORD wPID, const PVOID pParam) override;
	void OnPidUnmapped(const WORD wPID) override;

	CEcmProcessor *m_pEcmProcessor;
};

class CDescramblePmtTable : public CPmtTable
{
	CTsDescrambler *m_pDescrambler;
	CTsPidMapManager *m_pMapManager;
	CEcmProcessor *m_pEcmProcessor;
	WORD m_EcmPID;
	WORD m_ServiceID;
	std::vector<WORD> m_EsPIDList;

	void UnmapEcmTarget();
	void UnmapEsTarget();

// CTsPidMapTarget
	void OnPidUnmapped(const WORD wPID) override;

public:
	CDescramblePmtTable(CTsDescrambler *pDescrambler);
	void SetTarget();
	void ResetTarget();
};

class CEcmAccess : public CCasAccess
{
	BYTE m_EcmData[MAX_ECM_DATA_SIZE];
	DWORD m_EcmSize;
	CEcmProcessor *m_pEcmProcessor;
	CLocalEvent *m_pEvent;

	// コピー禁止
	CEcmAccess(const CEcmAccess &Src);
	CEcmAccess &operator=(const CEcmAccess &Src);

public:
	CEcmAccess(CEcmProcessor *pEcmProcessor, const BYTE *pData, DWORD Size, CLocalEvent *pEvent)
		: m_EcmSize(Size)
		, m_pEcmProcessor(pEcmProcessor)
		, m_pEvent(pEvent)
	{
		::CopyMemory(m_EcmData, pData, Size);
		m_pEcmProcessor->AddRef();
	}

	~CEcmAccess()
	{
		m_pEvent->Set();
		m_pEcmProcessor->ReleaseRef();
	}

	bool Process(CCasCard *pCasCard) override
	{
		return m_pEcmProcessor->SetScrambleKey(pCasCard, m_EcmData, m_EcmSize);
	}
};

class CEmmAccess : public CCasAccess
{
	BYTE m_EmmData[MAX_EMM_DATA_SIZE];
	DWORD m_EmmSize;
	CEmmProcessor *m_pEmmProcessor;

	// コピー禁止
	CEmmAccess(const CEmmAccess &Src);
	CEmmAccess &operator=(const CEmmAccess &Src);

public:
	CEmmAccess(CEmmProcessor *pEmmProcessor, const BYTE *pData, DWORD Size)
		: m_EmmSize(Size)
		, m_pEmmProcessor(pEmmProcessor)
	{
		::CopyMemory(m_EmmData, pData, Size);
		m_pEmmProcessor->AddRef();
	}

	~CEmmAccess()
	{
		m_pEmmProcessor->ReleaseRef();
	}

	bool Process(CCasCard *pCasCard) override
	{
		return m_pEmmProcessor->ProcessEmm(pCasCard, m_EmmData, m_EmmSize);
	}
};

class CConfirmContractAccess : public CCasAccess
{
	BYTE m_VerificationInfo[255];
	BYTE m_VerificationInfoSize;
	WORD m_Date;
	CLocalEvent *m_pEvent;
	bool *m_pbSuccess;

public:
	CConfirmContractAccess(const BYTE *pVerificationInfo, BYTE VerificationInfoSize, WORD Date,
						   CLocalEvent *pEvent, bool *pbSuccess)
		: m_VerificationInfoSize(VerificationInfoSize)
		, m_Date(Date)
		, m_pEvent(pEvent)
		, m_pbSuccess(pbSuccess)
	{
		::CopyMemory(m_VerificationInfo, pVerificationInfo, VerificationInfoSize);
	}

	~CConfirmContractAccess()
	{
		if (m_pEvent)
			m_pEvent->Set();
	}

	bool Process(CCasCard *pCasCard) override
	{
		bool bSuccess = pCasCard->ConfirmContract(m_VerificationInfo, m_VerificationInfoSize, m_Date);
		if (m_pbSuccess)
			*m_pbSuccess = bSuccess;
		return true;
	}
};

class CCasSendCommandAccess : public CCasAccess
{
	BYTE *m_pSendData;
	DWORD m_SendSize;
	BYTE *m_pReceiveData;
	DWORD *m_pReceiveSize;
	CLocalEvent *m_pEvent;
	bool *m_pbSuccess;

public:
	CCasSendCommandAccess(const BYTE *pSendData, DWORD SendSize,
						   BYTE *pReceiveData, DWORD *pReceiveSize,
						   CLocalEvent *pEvent, bool *pbSuccess)
		: m_SendSize(SendSize)
		, m_pReceiveData(pReceiveData)
		, m_pReceiveSize(pReceiveSize)
		, m_pEvent(pEvent)
		, m_pbSuccess(pbSuccess)
	{
		m_pSendData = new BYTE[SendSize];
		::CopyMemory(m_pSendData, pSendData, SendSize);
	}

	~CCasSendCommandAccess()
	{
		if (m_pEvent)
			m_pEvent->Set();
		delete [] m_pSendData;
	}

	bool Process(CCasCard *pCasCard) override
	{
		bool bSuccess = pCasCard->SendCommand(m_pSendData, m_SendSize, m_pReceiveData, m_pReceiveSize);
		if (m_pbSuccess)
			*m_pbSuccess = bSuccess;
		return true;
	}
};


//////////////////////////////////////////////////////////////////////
// CTsDescrambler 構築/消滅
//////////////////////////////////////////////////////////////////////

CTsDescrambler::CTsDescrambler(IEventHandler *pEventHandler)
	: CMediaDecoder(pEventHandler, 1UL, 1UL)
	, m_bDescramble(true)
	, m_bProcessEmm(false)
	, m_InputPacketCount(0)
	, m_ScramblePacketCount(0)
	, m_CurTransportStreamID(0)
	, m_DescrambleServiceID(0)
	, m_Queue(&m_CasCard)
	, m_Instruction(INSTRUCTION_NORMAL)
	, m_EmmPID(0xFFFF)
{
	if (IsSSSE3Available())
		m_Instruction = INSTRUCTION_SSSE3;
	else if (IsSSE2Available())
		m_Instruction = INSTRUCTION_SSE2;

	Reset();
}

CTsDescrambler::~CTsDescrambler()
{
	CloseCasCard();
}

void CTsDescrambler::Reset(void)
{
	CBlockLock Lock(&m_DecoderLock);

	m_Queue.Clear();

	// 内部状態を初期化する
	m_PidMapManager.UnmapAllTarget();

	CEcmProcessor::ResetCardReaderStatus();

	// PATテーブルPIDマップ追加
	m_PidMapManager.MapTarget(PID_PAT, new CPatTable, OnPatUpdated, this);

	// SDTテーブルPIDマップ追加
	m_PidMapManager.MapTarget(PID_SDT, new CSdtTable, OnSdtUpdated, this);

	if (m_bProcessEmm) {
		// CATテーブルPIDマップ追加
		m_PidMapManager.MapTarget(PID_CAT, new CCatTable, OnCatUpdated, this);

		// TOTテーブルPIDマップ追加
		m_PidMapManager.MapTarget(PID_TOT, new CTotTable);
	}

	// 統計データ初期化
	m_InputPacketCount = 0;
	m_ScramblePacketCount = 0;

	m_CurTransportStreamID = 0;

	// スクランブル解除ターゲット初期化
	m_DescrambleServiceID = 0;
	m_ServiceList.clear();

	m_EmmPID = 0xFFFF;
}

const bool CTsDescrambler::InputMedia(CMediaData *pMediaData, const DWORD dwInputIndex)
{
	CBlockLock Lock(&m_DecoderLock);

	/*
	if(dwInputIndex >= GetInputNum())return false;

	CTsPacket *pTsPacket = dynamic_cast<CTsPacket *>(pMediaData);

	// 入力メディアデータは互換性がない
	if(!pTsPacket)return false;
	*/

	CTsPacket *pTsPacket = static_cast<CTsPacket *>(pMediaData);

	// 入力パケット数カウント
	m_InputPacketCount++;

	if (!pTsPacket->IsScrambled() || m_bDescramble) {
		// PIDルーティング
		m_PidMapManager.StorePacket(pTsPacket);
	} else {
		// 復号漏れパケット数カウント
		m_ScramblePacketCount++;
	}

	// パケットを下流デコーダにデータを渡す
	OutputMedia(pMediaData);

	return true;
}

bool CTsDescrambler::EnableDescramble(bool bDescramble)
{
	CBlockLock Lock(&m_DecoderLock);

	m_bDescramble = bDescramble;
	return true;
}

bool CTsDescrambler::IsDescrambleEnabled() const
{
	return m_bDescramble;
}

bool CTsDescrambler::EnableEmmProcess(bool bEnable)
{
	CBlockLock Lock(&m_DecoderLock);

	if (m_bProcessEmm != bEnable) {
		if (bEnable) {
			// CATテーブルPIDマップ追加
			m_PidMapManager.MapTarget(PID_CAT, new CCatTable, OnCatUpdated, this);
			// TOTテーブルPIDマップ追加
			m_PidMapManager.MapTarget(PID_TOT, new CTotTable);
		} else {
			if (m_EmmPID < 0x1FFF) {
				m_PidMapManager.UnmapTarget(m_EmmPID);
				m_EmmPID = 0xFFFF;
			}
			m_PidMapManager.UnmapTarget(PID_CAT);
			m_PidMapManager.UnmapTarget(PID_TOT);
		}
		m_bProcessEmm = bEnable;
	}
	return true;
}

bool CTsDescrambler::IsEmmProcessEnabled() const
{
	return m_bProcessEmm;
}

bool CTsDescrambler::OpenCasCard(CCardReader::ReaderType ReaderType, LPCTSTR pszReaderName)
{
	CloseCasCard();

	bool bOK = m_Queue.BeginCasThread(ReaderType, pszReaderName);

	CEcmProcessor::ResetCardReaderStatus();

	SetError(m_Queue.GetLastErrorException());

	return bOK;
}

void CTsDescrambler::CloseCasCard(void)
{
	m_Queue.EndCasThread();
	// CASカードを閉じる
	//m_CasCard.CloseCard();
}

bool CTsDescrambler::IsCasCardOpen() const
{
	return m_CasCard.IsCardOpen();
}

CCardReader::ReaderType CTsDescrambler::GetCardReaderType() const
{
	return m_CasCard.GetCardReaderType();
}

LPCTSTR CTsDescrambler::GetCardReaderName() const
{
	return m_CasCard.GetCardReaderName();
}

bool CTsDescrambler::GetCasCardInfo(CCasCard::CasCardInfo *pInfo) const
{
	return m_CasCard.GetCasCardInfo(pInfo);
}

bool CTsDescrambler::GetCasCardID(BYTE *pCardID) const
{
	if (pCardID == NULL)
		return false;

	// カードID取得
	const BYTE *pBuff = m_CasCard.GetCardID();
	if (pBuff == NULL)
		return false;

	// バッファにコピー
	::CopyMemory(pCardID, pBuff, 6UL);

	return true;
}

int CTsDescrambler::FormatCasCardID(LPTSTR pszText,int MaxLength) const
{
	return m_CasCard.FormatCardID(pszText, MaxLength);
}

char CTsDescrambler::GetCasCardManufacturerID() const
{
	return m_CasCard.GetCardManufacturerID();
}

BYTE CTsDescrambler::GetCasCardVersion() const
{
	return m_CasCard.GetCardVersion();
}

ULONGLONG CTsDescrambler::GetInputPacketCount(void) const
{
	// 入力パケット数を返す
	return m_InputPacketCount;
}

ULONGLONG CTsDescrambler::GetScramblePacketCount(void) const
{
	// 復号漏れパケット数を返す
	return m_ScramblePacketCount;
}

void CTsDescrambler::ResetScramblePacketCount(void)
{
	m_ScramblePacketCount = 0;
}

int CTsDescrambler::GetServiceIndexByID(WORD ServiceID) const
{
	int Index;

	// プログラムIDからサービスインデックスを検索する
	for (Index = (int)m_ServiceList.size() - 1 ; Index >= 0 ; Index--) {
		if (m_ServiceList[Index].ServiceID == ServiceID)
			break;
	}

	return Index;
}

bool CTsDescrambler::SetTargetServiceID(WORD ServiceID)
{
	if (m_DescrambleServiceID != ServiceID) {
		TRACE(TEXT("CTsDescrambler::SetTargetServiceID() SID = %d (%04x)\n"),
			  ServiceID, ServiceID);

		CBlockLock Lock(&m_DecoderLock);

		m_DescrambleServiceID = ServiceID;

		for (size_t i = 0 ; i < m_ServiceList.size() ; i++) {
			CDescramblePmtTable *pPmtTable = dynamic_cast<CDescramblePmtTable *>(m_PidMapManager.GetMapTarget(m_ServiceList[i].PmtPID));

			if (pPmtTable) {
				const bool bTarget = m_DescrambleServiceID == 0
						|| m_ServiceList[i].ServiceID == m_DescrambleServiceID;

				if (bTarget && !m_ServiceList[i].bTarget) {
					pPmtTable->SetTarget();
					m_ServiceList[i].bTarget = true;
				}
			}
		}

		for (size_t i = 0 ; i < m_ServiceList.size() ; i++) {
			CDescramblePmtTable *pPmtTable = dynamic_cast<CDescramblePmtTable *>(m_PidMapManager.GetMapTarget(m_ServiceList[i].PmtPID));

			if (pPmtTable) {
				const bool bTarget = m_DescrambleServiceID == 0
						|| m_ServiceList[i].ServiceID == m_DescrambleServiceID;

				if (!bTarget && m_ServiceList[i].bTarget) {
					pPmtTable->ResetTarget();
					m_ServiceList[i].bTarget = false;
				}
			}
		}

#ifdef _DEBUG
		PrintStatus();
#endif
	}
	return true;
}

WORD CTsDescrambler::GetTargetServiceID() const
{
	return m_DescrambleServiceID;
}

WORD CTsDescrambler::GetEcmPIDByServiceID(const WORD ServiceID) const
{
	CBlockLock Lock(&m_DecoderLock);

	int Index = GetServiceIndexByID(ServiceID);
	if (Index < 0)
		return 0xFFFF;
	return m_ServiceList[Index].EcmPID;
}

bool CTsDescrambler::SendCasCommand(const BYTE *pSendData, DWORD SendSize, BYTE *pRecvData, DWORD *pRecvSize)
{
	if (pSendData == NULL || SendSize == 0 || SendSize > 256
			|| pRecvData == NULL || pRecvSize == NULL)
		return false;

	if (!m_CasCard.IsCardOpen())
		return false;

	CLocalEvent Event;
	bool bSuccess = false;
	Event.Create();
	CCasSendCommandAccess *pAccess = new CCasSendCommandAccess(pSendData, SendSize, pRecvData, pRecvSize, &Event, &bSuccess);
	if (!m_Queue.Enqueue(pAccess)) {
		delete pAccess;
		return false;
	}
	Event.Wait();
	return bSuccess;
}

CTsDescrambler::ContractStatus CTsDescrambler::CheckContractStatus(const ServiceContractInfo &Info, WORD Date)
{
	CLocalEvent Event;
	bool bSuccess = false;
	Event.Create();
	CConfirmContractAccess *pAccess =
		new CConfirmContractAccess(&Info.VerificationInfo[0], (BYTE)Info.VerificationInfo.size(), Date, &Event, &bSuccess);
	if (!m_Queue.Enqueue(pAccess)) {
		delete pAccess;
		return CONTRACT_ERROR;
	}
	Event.Wait();

	return bSuccess ? CONTRACT_CONTRACTED : CONTRACT_UNCONTRACTED;
}

CTsDescrambler::ContractStatus CTsDescrambler::GetContractStatus(WORD NetworkID, WORD ServiceID, const SYSTEMTIME *pTime)
{
	if (!m_CasCard.IsCardOpen())
		return CONTRACT_UNKNOWN;

	ServiceContractInfo ContractInfo;

	{
		CBlockLock Lock(&m_DecoderLock);

		ServiceContractMap::iterator itr =
			m_ServiceContractList.find(GetServiceContractMapKey(NetworkID, ServiceID));
		if (itr == m_ServiceContractList.end())
			return CONTRACT_UNKNOWN;
		ContractInfo = itr->second;
	}

	SYSTEMTIME st;
	if (pTime != NULL)
		st = *pTime;
	else
		::GetLocalTime(&st);
	WORD Date;
	CAribTime::SystemTimeToMjd(&st, &Date);

#ifdef _DEBUG
	WORD Year, Month, Day;
	CAribTime::SplitAribMjd(Date, &Year, &Month, &Day);
	TRACE(TEXT("Confirm contract : NID %04x / SID %04x / Date %04x(%d/%d/%d)\n"),
		  NetworkID, ServiceID, Date, Year, Month, Day);
#endif

	return CheckContractStatus(ContractInfo, Date);
}

CTsDescrambler::ContractStatus CTsDescrambler::GetContractPeriod(WORD NetworkID, WORD ServiceID, SYSTEMTIME *pTime)
{
	if (!m_CasCard.IsCardOpen())
		return CONTRACT_UNKNOWN;

	ServiceContractInfo ContractInfo;

	{
		CBlockLock Lock(&m_DecoderLock);

		ServiceContractMap::iterator itr =
			m_ServiceContractList.find(GetServiceContractMapKey(NetworkID, ServiceID));
		if (itr == m_ServiceContractList.end())
			return CONTRACT_UNKNOWN;
		ContractInfo = itr->second;
	}

	WORD Min, Max;
	ContractStatus Stat;

	Min = 51604;
	Max = 65535;

	Stat = CheckContractStatus(ContractInfo, Min);
	if (Stat != CONTRACT_CONTRACTED)
		return Stat;

	Stat = CheckContractStatus(ContractInfo, Max);
	if (Stat == CONTRACT_CONTRACTED) {
		Min = Max;
	} else if (Stat == CONTRACT_UNCONTRACTED) {
		do {
			WORD Cur = ((Max - Min) / 2) + Min;
			Stat = CheckContractStatus(ContractInfo, Cur);
			if (Stat == CONTRACT_CONTRACTED)
				Min = Cur;
			else if (Stat == CONTRACT_UNCONTRACTED)
				Max = Cur;
			else
				return Stat;
		} while (Max - Min > 1);
	} else {
		return Stat;
	}

	CAribTime::MjdToSystemTime(Min, pTime);

	return CONTRACT_CONTRACTED;
}

bool CTsDescrambler::HasContractInfo(WORD NetworkID, WORD ServiceID) const
{
	CBlockLock Lock(&m_DecoderLock);

	return m_ServiceContractList.find(GetServiceContractMapKey(NetworkID, ServiceID)) != m_ServiceContractList.end();
}

bool CTsDescrambler::SetInstruction(InstructionType Type)
{
	TRACE(TEXT("CTsDescrambler::SetInstruction(%d)\n"), Type);

	switch (Type) {
	case INSTRUCTION_NORMAL:
		break;
	case INSTRUCTION_SSE2:
		if (!IsSSE2Available())
			return false;
		break;
	case INSTRUCTION_SSSE3:
		if (!IsSSSE3Available())
			return false;
		break;
	default:
		return false;
	}

	m_Instruction = Type;

	return true;
}

CTsDescrambler::InstructionType CTsDescrambler::GetInstruction() const
{
	return m_Instruction;
}

bool CTsDescrambler::IsSSE2Available()
{
#ifdef MULTI2_SSE2
	return CMulti2Decoder::IsSSE2Available();
#else
	return false;
#endif
}

bool CTsDescrambler::IsSSSE3Available()
{
#ifdef MULTI2_SSSE3
	return CMulti2Decoder::IsSSSE3Available();
#else
	return false;
#endif
}

void CALLBACK CTsDescrambler::OnPatUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam)
{
	// PATが更新された
	CTsDescrambler *pThis = static_cast<CTsDescrambler *>(pParam);
	CPatTable *pPatTable = static_cast<CPatTable *>(pMapTarget);

	TRACE(TEXT("CTsDescrambler::OnPatUpdated()\n"));

	const WORD TsID = pPatTable->GetTransportStreamID();
	if (TsID != pThis->m_CurTransportStreamID) {
		// TSIDが変化したらリセットする
		pThis->m_Queue.Clear();
		for (WORD PID = 0x0002 ; PID < 0x2000 ; PID++) {
			if (PID != PID_TOT && PID != PID_SDT)
				pThis->m_PidMapManager.UnmapTarget(PID);
		}

		CCatTable *pCatTable = dynamic_cast<CCatTable*>(pThis->m_PidMapManager.GetMapTarget(PID_CAT));
		if (pCatTable != NULL)
			pCatTable->Reset();

		CTotTable *pTotTable = dynamic_cast<CTotTable*>(pThis->m_PidMapManager.GetMapTarget(PID_TOT));
		if (pTotTable != NULL)
			pTotTable->Reset();

		CSdtTable *pSdtTable = dynamic_cast<CSdtTable*>(pThis->m_PidMapManager.GetMapTarget(PID_SDT));
		if (pSdtTable != NULL)
			pSdtTable->Reset();

		pThis->m_ServiceList.clear();
		pThis->m_CurTransportStreamID = TsID;
		pThis->m_EmmPID = 0xFFFF;
	} else {
		// 無くなったPMTをスクランブル解除対象から除外する
		for (size_t i = 0 ; i < pThis->m_ServiceList.size() ; i++) {
			const WORD PmtPID = pThis->m_ServiceList[i].PmtPID;
			WORD j;

			for (j = 0 ; j < pPatTable->GetProgramNum() ; j++) {
				if (pPatTable->GetPmtPID(j) == PmtPID)
					break;
			}
			if (j == pPatTable->GetProgramNum()) {
				pThis->m_PidMapManager.UnmapTarget(PmtPID);
				pThis->m_ServiceList[i].bTarget = false;
			}
		}
	}

	std::vector<TAG_SERVICEINFO> ServiceList;
	ServiceList.resize(pPatTable->GetProgramNum());
	for (WORD i = 0 ; i < pPatTable->GetProgramNum() ; i++) {
		const WORD PmtPID = pPatTable->GetPmtPID(i);
		const WORD ServiceID = pPatTable->GetProgramID(i);

		ServiceList[i].bTarget = pThis->m_DescrambleServiceID == 0
								|| ServiceID == pThis->m_DescrambleServiceID;
		ServiceList[i].ServiceID = ServiceID;
		ServiceList[i].PmtPID = PmtPID;
		size_t j;
		for (j = 0 ; j < pThis->m_ServiceList.size() ; j++) {
			if (pThis->m_ServiceList[j].PmtPID == PmtPID)
				break;
		}
		if (j < pThis->m_ServiceList.size()) {
			ServiceList[i].EcmPID = pThis->m_ServiceList[j].EcmPID;
			ServiceList[i].EsPIDList = pThis->m_ServiceList[j].EsPIDList;
		} else {
			ServiceList[i].EcmPID = 0xFFFF;
			ServiceList[i].EsPIDList.clear();
		}

		CDescramblePmtTable *pPmtTable = dynamic_cast<CDescramblePmtTable *>(pThis->m_PidMapManager.GetMapTarget(PmtPID));
		if (pPmtTable == NULL)
			pThis->m_PidMapManager.MapTarget(PmtPID, new CDescramblePmtTable(pThis), OnPmtUpdated, pThis);
	}
	pThis->m_ServiceList = ServiceList;
}

void CALLBACK CTsDescrambler::OnPmtUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam)
{
	// PMTが更新された
	CTsDescrambler *pThis = static_cast<CTsDescrambler *>(pParam);
	CDescramblePmtTable *pPmtTable = static_cast<CDescramblePmtTable *>(pMapTarget);

	const WORD ServiceID = pPmtTable->GetProgramNumberID();
	const int ServiceIndex = pThis->GetServiceIndexByID(ServiceID);
	if (ServiceIndex < 0)
		return;

	TRACE(TEXT("CTsDescrambler::OnPmtUpdated() SID = %04d\n"), ServiceID);

	WORD CASystemID, EcmPID;
	if (pThis->m_CasCard.GetCASystemID(&CASystemID)) {
		EcmPID = pPmtTable->GetEcmPID(CASystemID);
	} else {
		EcmPID = 0xFFFF;
	}
	pThis->m_ServiceList[ServiceIndex].EcmPID = EcmPID;

	pThis->m_ServiceList[ServiceIndex].EsPIDList.resize(pPmtTable->GetEsInfoNum());
	for (WORD i = 0 ; i < pPmtTable->GetEsInfoNum() ; i++)
		pThis->m_ServiceList[ServiceIndex].EsPIDList[i] = pPmtTable->GetEsPID(i);

	pThis->m_ServiceList[ServiceIndex].bTarget = pThis->m_DescrambleServiceID == 0
								|| ServiceID == pThis->m_DescrambleServiceID;
	if (pThis->m_ServiceList[ServiceIndex].bTarget)
		pPmtTable->SetTarget();
	else
		pPmtTable->ResetTarget();

#ifdef _DEBUG
	pThis->PrintStatus();
#endif
}

void CALLBACK CTsDescrambler::OnCatUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam)
{
	// CATが更新された
	CTsDescrambler *pThis = static_cast<CTsDescrambler *>(pParam);
	CCatTable *pCatTable = static_cast<CCatTable *>(pMapTarget);

	// EMMのPID追加
	WORD SystemID, EmmPID;
	if (pThis->m_CasCard.GetCASystemID(&SystemID)) {
		EmmPID = pCatTable->GetEmmPID(SystemID);
		if (EmmPID >= 0x1FFF || EmmPID == 0)
			EmmPID = 0xFFFF;
	} else {
		EmmPID = 0xFFFF;
	}
	if (pThis->m_EmmPID != EmmPID) {
		if (pThis->m_EmmPID < 0x1FFF)
			pThis->m_PidMapManager.UnmapTarget(pThis->m_EmmPID);
		if (EmmPID < 0x1FFF)
			pThis->m_PidMapManager.MapTarget(EmmPID, new CEmmProcessor(pThis));
		pThis->m_EmmPID = EmmPID;
	}
}

void CALLBACK CTsDescrambler::OnSdtUpdated(const WORD wPID, CTsPidMapTarget *pMapTarget, CTsPidMapManager *pMapManager, const PVOID pParam)
{
	// SDTが更新された
	CTsDescrambler *pThis = static_cast<CTsDescrambler *>(pParam);
	CSdtTable *pSdtTable = static_cast<CSdtTable *>(pMapTarget);

	const WORD NetworkID = pSdtTable->GetNetworkID();

	// 契約確認情報を取得する
	// TODO: CA_contract_info_descriptorが複数ある場合の対応
	/*WORD CASystemID;
	if (pThis->m_CasCard.GetCASystemID(&CASystemID))*/ {
		TRACE(TEXT("Verification info : NID %04x\n"), NetworkID);
		for (WORD i = 0; i < pSdtTable->GetServiceNum(); i++) {
			const CDescBlock *pDescBlock = pSdtTable->GetItemDesc(i);
			if (pDescBlock != NULL) {
				for (WORD j = 0; j < pDescBlock->GetDescNum(); j++) {
					const CBaseDesc *pDesc = pDescBlock->GetDescByIndex(j);
					if (pDesc->GetTag() == CCaContractInfoDesc::DESC_TAG) {
						const CCaContractInfoDesc *pCaContractInfoDesc = dynamic_cast<const CCaContractInfoDesc *>(pDesc);
						if (pCaContractInfoDesc != NULL
								//&& pCaContractInfoDesc->GetCaSystemID() == CASystemID
								&& pCaContractInfoDesc->GetCaUnitID() != 0x0) {
							const WORD ServiceID = pSdtTable->GetServiceID(i);
							const DWORD MapKey = GetServiceContractMapKey(NetworkID, ServiceID);
							ServiceContractMap::iterator itr = pThis->m_ServiceContractList.find(MapKey);
							if (itr == pThis->m_ServiceContractList.end())
								itr = pThis->m_ServiceContractList.insert(std::pair<DWORD, ServiceContractInfo>(MapKey, ServiceContractInfo())).first;
							const BYTE VerificationInfoLength = pCaContractInfoDesc->GetContractVerificationInfoLength();
							itr->second.VerificationInfo.resize(VerificationInfoLength);
							if (VerificationInfoLength > 0)
								pCaContractInfoDesc->GetContractVerificationInfo(
									&itr->second.VerificationInfo[0], VerificationInfoLength);
#ifdef _DEBUG
							TCHAR szText[256];
							int Length;
							Length = ::wsprintf(szText, TEXT("  SID %04x : "), ServiceID);
							for (BYTE k = 0; k < VerificationInfoLength; k++)
								Length += ::wsprintf(&szText[Length], TEXT("%02x "), itr->second.VerificationInfo[k]);
							::lstrcpy(&szText[Length], TEXT("\n"));
							TRACE(szText);
#endif
							break;
						}
					}
				}
			}
		}
	}
}

#ifdef _DEBUG
void CTsDescrambler::PrintStatus(void) const
{
	TRACE(TEXT("****** Descramble ES PIDs ******\n"));
	for (WORD PID = 0x0001 ; PID < 0x2000 ; PID++) {
		CEsProcessor *pEsProcessor = dynamic_cast<CEsProcessor*>(m_PidMapManager.GetMapTarget(PID));

		if (pEsProcessor)
			TRACE(TEXT("ES PID = %04x (%d)\n"), PID, PID);
	}
	TRACE(TEXT("****** Descramble ECM PIDs ******\n"));
	for (WORD PID = 0x0001 ; PID < 0x2000 ; PID++) {
		CEcmProcessor *pEcmProcessor = dynamic_cast<CEcmProcessor*>(m_PidMapManager.GetMapTarget(PID));

		if (pEcmProcessor)
			TRACE(TEXT("ECM PID = %04x (%d)\n"), PID, PID);
	}
	if (m_EmmPID < 0x1FFF)
		TRACE(TEXT("EMM PID = %04x (%d)\n"), m_EmmPID, m_EmmPID);
}
#endif


CDescramblePmtTable::CDescramblePmtTable(CTsDescrambler *pDescrambler)
	: m_pDescrambler(pDescrambler)
	, m_pMapManager(&pDescrambler->m_PidMapManager)
	, m_pEcmProcessor(NULL)
	, m_EcmPID(0xFFFF)
	, m_ServiceID(0)
{
}

void CDescramblePmtTable::UnmapEcmTarget()
{
	// ECMが他のスクランブル解除対象サービスと異なる場合はアンマップ
	if (m_EcmPID < 0x1FFF) {
		bool bFound = false;
		for (size_t i = 0 ; i < m_pDescrambler->m_ServiceList.size() ; i++) {
			if (m_pDescrambler->m_ServiceList[i].ServiceID != m_ServiceID
					&& m_pDescrambler->m_ServiceList[i].bTarget
					&& m_pDescrambler->m_ServiceList[i].EcmPID == m_EcmPID) {
				bFound = true;
				break;
			}
		}
		if (!bFound)
			m_pMapManager->UnmapTarget(m_EcmPID);
	}

	UnmapEsTarget();
}

void CDescramblePmtTable::UnmapEsTarget()
{
	// ESのPIDマップ削除
	for (size_t i = 0 ; i < m_EsPIDList.size() ; i++) {
		const WORD EsPID = m_EsPIDList[i];
		bool bFound = false;

		for (size_t j = 0 ; j < m_pDescrambler->m_ServiceList.size() ; j++) {
			if (m_pDescrambler->m_ServiceList[j].ServiceID != m_ServiceID
					&& m_pDescrambler->m_ServiceList[j].bTarget) {
				for (size_t k = 0 ; k < m_pDescrambler->m_ServiceList[j].EsPIDList.size() ; k++) {
					if (m_pDescrambler->m_ServiceList[j].EsPIDList[k] == EsPID) {
						bFound = true;
						break;
					}
				}
				if (bFound)
					break;
			}
		}
		if (!bFound)
			m_pMapManager->UnmapTarget(EsPID);
	}
}

void CDescramblePmtTable::SetTarget()
{
	// スクランブル解除対象に設定

	WORD CASystemID, EcmPID;
	if (m_pDescrambler->m_CasCard.GetCASystemID(&CASystemID)) {
		EcmPID = GetEcmPID(CASystemID);
		if (EcmPID >= 0x1FFF)
			EcmPID = 0xFFFF;
	} else {
		EcmPID = 0xFFFF;
	}

	if (EcmPID < 0x1FFF) {
		if (m_EcmPID < 0x1FFF) {
			// ECMとESのPIDをアンマップ
			if (EcmPID != m_EcmPID) {
				UnmapEcmTarget();
			} else {
				UnmapEsTarget();
			}
		}

		m_pEcmProcessor = dynamic_cast<CEcmProcessor*>(m_pMapManager->GetMapTarget(EcmPID));
		if (m_pEcmProcessor == NULL) {
			// ECM処理内部クラス新規マップ
			m_pEcmProcessor = new CEcmProcessor(m_pDescrambler);
			m_pMapManager->MapTarget(EcmPID, m_pEcmProcessor);
		}
		m_EcmPID = EcmPID;

		// ESのPIDマップ追加
		m_EsPIDList.resize(GetEsInfoNum());
		for (WORD i = 0 ; i < GetEsInfoNum() ; i++) {
			const WORD EsPID = GetEsPID(i);
			const CTsDescrambler::CEsProcessor *pEsProcessor = dynamic_cast<CTsDescrambler::CEsProcessor*>(m_pMapManager->GetMapTarget(EsPID));

			if (pEsProcessor == NULL
					|| pEsProcessor->GetEcmProcessor() != m_pEcmProcessor)
				m_pMapManager->MapTarget(EsPID, new CTsDescrambler::CEsProcessor(m_pEcmProcessor));
			m_EsPIDList[i] = EsPID;
		}
	} else {
		ResetTarget();
	}

	m_ServiceID = GetProgramNumberID();
}

void CDescramblePmtTable::ResetTarget()
{
	// スクランブル解除対象から除外
	if (m_EcmPID < 0x1FFF) {
		// ECMとESのPIDをアンマップ
		UnmapEcmTarget();

		m_pEcmProcessor = NULL;
		m_EcmPID = 0xFFFF;
		m_EsPIDList.clear();
	}
}

void CDescramblePmtTable::OnPidUnmapped(const WORD wPID)
{
	ResetTarget();

	CPmtTable::OnPidUnmapped(wPID);
}


//////////////////////////////////////////////////////////////////////
// CEcmProcessor 構築/消滅
//////////////////////////////////////////////////////////////////////

DWORD CEcmProcessor::m_EcmErrorCount = 0;
bool CEcmProcessor::m_bCardReaderHung = false;

CEcmProcessor::CEcmProcessor(CTsDescrambler *pDescrambler)
	: CPsiSingleTable(true)
	, m_pDescrambler(pDescrambler)
#ifdef MULTI2_SIMD
	, m_Multi2Decoder(
#ifdef MULTI2_SSE2
		pDescrambler->m_Instruction == CTsDescrambler::INSTRUCTION_SSE2 ? CMulti2Decoder::INSTRUCTION_SSE2 :
#endif
#ifdef MULTI2_SSSE3
		pDescrambler->m_Instruction == CTsDescrambler::INSTRUCTION_SSSE3 ? CMulti2Decoder::INSTRUCTION_SSSE3 :
#endif
		CMulti2Decoder::INSTRUCTION_NORMAL
		)
#endif
	, m_EcmPID(0xFFFF)
	, m_bEcmReceived(false)
	, m_bLastEcmSucceed(true)
	, m_bOddKeyValid(false)
	, m_bEvenKeyValid(false)
	, m_LastChangedKey(0)
	, m_bEcmErrorSent(false)
	, m_LastScramblingCtrl(0)
{
	// MULTI2デコーダにシステムキーと初期CBCをセット
	if (m_pDescrambler->m_CasCard.IsCardOpen())
		m_Multi2Decoder.Initialize(m_pDescrambler->m_CasCard.GetSystemKey(),
								   m_pDescrambler->m_CasCard.GetInitialCbc());

	m_EcmProcessEvent.Create(true, true);

	::ZeroMemory(m_LastKsData, sizeof(m_LastKsData));
}

void CEcmProcessor::OnPidMapped(const WORD wPID, const PVOID pParam)
{
	TRACE(TEXT("CEcmProcessor::OnPidMapped() PID = %d (0x%04x)\n"), wPID, wPID);
	m_EcmPID = wPID;
	AddRef();
}

void CEcmProcessor::OnPidUnmapped(const WORD wPID)
{
	TRACE(TEXT("CEcmProcessor::OnPidUnmapped() PID = %d (0x%04x)\n"), wPID, wPID);

	//CPsiSingleTable::OnPidUnmapped(wPID);
	ReleaseRef();
}

const bool CEcmProcessor::DescramblePacket(CTsPacket *pTsPacket)
{
	const BYTE ScramblingCtrl = pTsPacket->m_Header.byTransportScramblingCtrl;
	if (!(ScramblingCtrl & 2))
		return true;

	if (!m_bEcmReceived) {
		// まだECMが来ていない
		m_pDescrambler->m_ScramblePacketCount++;
		return false;
	}

	const bool bEven = !(ScramblingCtrl & 1);

	m_Multi2Lock.Lock();

	if (m_LastScramblingCtrl != ScramblingCtrl) {
		if (m_LastScramblingCtrl != 0) {
			/*
				一つのECMで複数のストリームが対象になっている時、
				ストリームによってOdd/Evenの切り替わるタイミングが違う事がある?
				(未確認だが、不具合報告からの推測)
				無効にするのをECMのKsが変わったタイミングにしておく
			*/
#if 0
			// Odd/Evenが切り替わった時にもう片方を無効にする(古いキーが使われ続けるのを防ぐため)
			if (bEven)
				m_bOddKeyValid = false;
			else
				m_bEvenKeyValid = false;
#endif

			// ECM処理中であれば待ってみる
			if (((bEven && !m_bEvenKeyValid) || (!bEven && !m_bOddKeyValid))
					&& !m_EcmProcessEvent.IsSignaled()
					&& !m_bCardReaderHung) {
				m_Multi2Lock.Unlock();
				m_EcmProcessEvent.Wait(1000);
				m_Multi2Lock.Lock();
			}
		} else {
			// 最初のECM処理中であれば終わるまで待つ
			m_Multi2Lock.Unlock();
			m_EcmProcessEvent.Wait(3000);
			m_Multi2Lock.Lock();
		}

		m_LastScramblingCtrl = ScramblingCtrl;
	}

	// スクランブル解除
	if ((bEven && m_bEvenKeyValid) || (!bEven && m_bOddKeyValid)) {
		if (m_Multi2Decoder.Decode
				(pTsPacket->GetPayloadData(),
				(DWORD)pTsPacket->GetPayloadSize(),
				ScramblingCtrl)) {
			m_Multi2Lock.Unlock();

			// トランスポートスクランブル制御再設定
			pTsPacket->SetAt(3UL, pTsPacket->GetAt(3UL) & 0x3FU);
			pTsPacket->m_Header.byTransportScramblingCtrl = 0;
			return true;
		}
	}

	m_Multi2Lock.Unlock();

	m_pDescrambler->m_ScramblePacketCount++;

	return false;
}

// Ksの比較
static inline bool CompareKs(const void *pKey1, const void *pKey2)
{
#ifdef _WIN64
	return *static_cast<const ULONGLONG*>(pKey1) == *static_cast<const ULONGLONG*>(pKey2);
#else
	return static_cast<const DWORD*>(pKey1)[0] == static_cast<const DWORD*>(pKey2)[0]
		&& static_cast<const DWORD*>(pKey1)[1] == static_cast<const DWORD*>(pKey2)[1];
#endif
}

const bool CEcmProcessor::OnTableUpdate(const CPsiSection *pCurSection, const CPsiSection *pOldSection)
{
	if (pCurSection->GetTableID() != 0x82)
		return false;

	const WORD PayloadSize = pCurSection->GetPayloadSize();

	if (PayloadSize < MIN_ECM_DATA_SIZE || PayloadSize > MAX_ECM_DATA_SIZE)
		return false;

	// ECMが変わったらキー取得が成功するまで無効にする
	m_Multi2Lock.Lock();
	if (m_LastChangedKey == 1) {
		m_bEvenKeyValid = false;
	} else if (m_LastChangedKey == 2) {
		m_bOddKeyValid = false;
	}
	m_Multi2Lock.Unlock();

	// 前のECM処理が終わるまで待つ
	if (!m_EcmProcessEvent.IsSignaled()) {
		if (m_bCardReaderHung)
			return false;
		if (m_EcmProcessEvent.Wait(3000) == WAIT_TIMEOUT) {
			OnCardReaderHung();
			return false;
		}
	}

	// CASアクセスキューに追加
	m_EcmProcessEvent.Reset();
	CEcmAccess *pEcmAccess = new CEcmAccess(this, pCurSection->GetPayloadData(), PayloadSize, &m_EcmProcessEvent);
	if (m_pDescrambler->m_Queue.Enqueue(pEcmAccess)) {
		m_bEcmReceived = true;
	} else {
		delete pEcmAccess;
	}

	return true;
}

const bool CEcmProcessor::SetScrambleKey(CCasCard *pCasCard, const BYTE *pEcmData, DWORD EcmSize)
{
	// ECMをCASカードに渡してキー取得
	const BYTE *pKsData = pCasCard->GetKsFromEcm(pEcmData, EcmSize);

	if (!pKsData) {
		int ErrorCode = pCasCard->GetLastErrorCode();
		// ECM処理失敗時は一度だけ再送信する
		if (m_bLastEcmSucceed
				&& ErrorCode != CCasCard::ERR_CARDNOTOPEN
				&& ErrorCode != CCasCard::ERR_ECMREFUSED
				&& ErrorCode != CCasCard::ERR_BADARGUMENT) {
			// 再送信してみる
			pKsData = pCasCard->GetKsFromEcm(pEcmData, EcmSize);
			if (!pKsData) {
				ErrorCode = pCasCard->GetLastErrorCode();
				if (ErrorCode != CCasCard::ERR_ECMREFUSED) {
					// カードを開き直して再初期化してみる
					TRACE(TEXT("CEcmProcessor::SetScrambleKey() : エラーのためカード再初期化\n"));
					if (pCasCard->ReOpenCard()) {
						m_Multi2Decoder.Initialize(pCasCard->GetSystemKey(),
												   pCasCard->GetInitialCbc());
						pKsData = pCasCard->GetKsFromEcm(pEcmData, EcmSize);
						if (!pKsData)
							ErrorCode = pCasCard->GetLastErrorCode();
					}
				}
			}
		}

		// 連続してエラーが起きたら通知
		if (!pKsData && !m_bLastEcmSucceed
				&& ErrorCode != CCasCard::ERR_CARDNOTOPEN) {
			if (!m_bEcmErrorSent && m_EcmPID < 0x1FFF) {
				CTsDescrambler::EcmErrorInfo Info;

				Info.pszText = pCasCard->GetLastErrorText();
				Info.EcmPID = m_EcmPID;
				m_pDescrambler->SendDecoderEvent(
					ErrorCode == CCasCard::ERR_ECMREFUSED ?
						CTsDescrambler::EVENT_ECM_REFUSED : CTsDescrambler::EVENT_ECM_ERROR,
					&Info);
				m_bEcmErrorSent = true;
			}
		}
	}

	m_Multi2Lock.Lock();

	// スクランブルキー更新
	m_Multi2Decoder.SetScrambleKey(pKsData);

	// ECM処理成功状態更新
	const bool bSucceeded = pKsData != NULL;
	m_LastChangedKey = 0;
	if (bSucceeded) {
		if (m_bLastEcmSucceed) {
			// キーが変わったら有効状態更新
			const bool bOddKeyChanged  = !CompareKs(&m_LastKsData[0], &pKsData[0]);
			const bool bEvenKeyChanged = !CompareKs(&m_LastKsData[8], &pKsData[8]);
			if (bOddKeyChanged)
				m_bOddKeyValid = true;
			if (bEvenKeyChanged)
				m_bEvenKeyValid = true;
			if (bOddKeyChanged) {
				if (!bEvenKeyChanged)
					m_LastChangedKey = 1;
			} else if (bEvenKeyChanged) {
				m_LastChangedKey = 2;
			}
		} else {
			m_bOddKeyValid = true;
			m_bEvenKeyValid = true;
		}
		::CopyMemory(m_LastKsData, pKsData, 16);
	} else {
		m_bOddKeyValid = false;
		m_bEvenKeyValid = false;
		m_EcmErrorCount++;
	}
	m_bLastEcmSucceed = bSucceeded;

	m_Multi2Lock.Unlock();

	return true;
}

void CEcmProcessor::OnCardReaderHung()
{
	if (!m_bCardReaderHung) {
		m_bCardReaderHung = true;
		m_pDescrambler->SendDecoderEvent(CTsDescrambler::EVENT_CARD_READER_HUNG);
	}
}


//////////////////////////////////////////////////////////////////////
// CEmmProcessor 構築/消滅
//////////////////////////////////////////////////////////////////////

CEmmProcessor::CEmmProcessor(CTsDescrambler *pDescrambler)
	: CPsiStreamTable(NULL, true)
	, m_pDescrambler(pDescrambler)
{
}

void CEmmProcessor::OnPidMapped(const WORD wPID, const PVOID pParam)
{
	TRACE(TEXT("CEmmProcessor::OnPidMapped() PID = %d (0x%04x)\n"), wPID, wPID);
	AddRef();
}

void CEmmProcessor::OnPidUnmapped(const WORD wPID)
{
	TRACE(TEXT("CEmmProcessor::OnPidUnmapped() PID = %d (0x%04x)\n"), wPID, wPID);

	//CPsiSingleTable::OnPidUnmapped(wPID);
	ReleaseRef();
}

const bool CEmmProcessor::OnTableUpdate(const CPsiSection *pCurSection)
{
	if (pCurSection->GetTableID() != 0x84)
		return false;

	const WORD DataSize = pCurSection->GetPayloadSize();
	const BYTE *pHexData = pCurSection->GetPayloadData();

	const BYTE *pCardID = m_pDescrambler->m_CasCard.GetCardID();
	if (pCardID == NULL)
		return true;

	WORD Pos = 0;
	while (DataSize >= Pos + 17) {
		const WORD EmmSize = (WORD)pHexData[Pos + 6] + 7;
		if (EmmSize < 17 || EmmSize > MAX_EMM_DATA_SIZE || DataSize < Pos + EmmSize)
			break;

#if EMMLOG
		{
			int n = 0;
			char buf[1024];
			for (int i = 0; i < EmmSize; i++)
			{
				BYTE b = pHexData[Pos + i];

				if ((b >> 4) < 10)
					buf[n++] = '0' + (b >> 4);
				else
					buf[n++] = 'a' + (b >> 4) - 10;

				if ((b & 0x0f) < 10)
					buf[n++] = '0' + (b & 0x0f);
				else
					buf[n++] = 'a' + (b & 0x0f) - 10;

				buf[n++] = ' ';
			}
			buf[n-1] = 0;
			emmlog << buf << std::endl;
		}
#endif
		if (::memcmp(pCardID, &pHexData[Pos], 6) == 0) {
			SYSTEMTIME st;
			const CTotTable *pTotTable = dynamic_cast<const CTotTable*>(m_pDescrambler->m_PidMapManager.GetMapTarget(PID_TOT));
			if (pTotTable == NULL || !pTotTable->GetDateTime(&st))
				break;

			FILETIME ft;
			ULARGE_INTEGER TotTime, LocalTime;

			::SystemTimeToFileTime(&st, &ft);
			TotTime.LowPart = ft.dwLowDateTime;
			TotTime.HighPart = ft.dwHighDateTime;
			::GetLocalTime(&st);
			::SystemTimeToFileTime(&st, &ft);
			LocalTime.LowPart = ft.dwLowDateTime;
			LocalTime.HighPart = ft.dwHighDateTime;
			if (TotTime.QuadPart + (10000000ULL * 60ULL * 60ULL * EMM_PROCESS_TIME) > LocalTime.QuadPart) {
				// CASアクセスキューに追加
				CEmmAccess *pEmmAccess = new CEmmAccess(this, &pHexData[Pos], EmmSize);
				if (!m_pDescrambler->m_Queue.Enqueue(pEmmAccess))
					delete pEmmAccess;
			}
			break;
		}

		Pos += EmmSize;
	}

	return true;
}

const bool CEmmProcessor::ProcessEmm(CCasCard *pCasCard, const BYTE *pData, DWORD DataSize)
{
	if (pCasCard->SendEmmSection(pData, DataSize)) {
		m_pDescrambler->SendDecoderEvent(CTsDescrambler::EVENT_EMM_PROCESSED, NULL);
	} else {
		CTsDescrambler::EmmErrorInfo Info;
		Info.pszText = pCasCard->GetLastErrorText();
		m_pDescrambler->SendDecoderEvent(CTsDescrambler::EVENT_EMM_ERROR, &Info);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////
// CTsDescrambler::CEsProcessor 構築/消滅
//////////////////////////////////////////////////////////////////////

CTsDescrambler::CEsProcessor::CEsProcessor(CEcmProcessor *pEcmProcessor)
	: CTsPidMapTarget()
	, m_pEcmProcessor(pEcmProcessor)
{
#if EMMLOG
	emmlog.open(LOGNAME, std::ios::app);
	emmlog << "// Log open()" << std::endl;
#endif
}

CTsDescrambler::CEsProcessor::~CEsProcessor()
{
}

const bool CTsDescrambler::CEsProcessor::StorePacket(const CTsPacket *pPacket)
{
	// スクランブル解除
	if (pPacket->IsScrambled()
			&& !m_pEcmProcessor->DescramblePacket(const_cast<CTsPacket *>(pPacket)))
		return false;

	return true;
}

void CTsDescrambler::CEsProcessor::OnPidMapped(const WORD wPID, const PVOID pParam)
{
	TRACE(TEXT("CEsProcessor::OnPidMapped() PID = %d (0x%04x)\n"), wPID, wPID);
}

void CTsDescrambler::CEsProcessor::OnPidUnmapped(const WORD wPID)
{
	TRACE(TEXT("CEsProcessor::OnPidUnmapped() PID = %d (0x%04x)\n"), wPID, wPID);
	delete this;
}


CCasAccessQueue::CCasAccessQueue(CCasCard *pCasCard)
	: m_pCasCard(pCasCard)
	, m_hThread(NULL)
	, m_bAvailable(false)
	, m_bKillEvent(false)
{
}

CCasAccessQueue::~CCasAccessQueue()
{
	EndCasThread();
}

void CCasAccessQueue::Clear()
{
	CBlockLock Lock(&m_Lock);

	std::deque<CCasAccess*>::iterator itr;
	for (itr = m_Queue.begin() ; itr != m_Queue.end() ; itr++)
		delete *itr;
	m_Queue.clear();
}

bool CCasAccessQueue::Enqueue(CCasAccess *pAccess)
{
	if (!m_bAvailable || pAccess == NULL)
		return false;

	CBlockLock Lock(&m_Lock);

	m_Queue.push_back(pAccess);
	m_Event.Set();
	return true;
}

bool CCasAccessQueue::BeginCasThread(CCardReader::ReaderType ReaderType, LPCTSTR pszReaderName)
{
	if (m_hThread)
		return false;
	if (m_Event.IsCreated())
		m_Event.Reset();
	else
		m_Event.Create();
	m_ReaderType = ReaderType;
	m_pszReaderName = pszReaderName;
	m_bKillEvent = false;
	m_bStartEvent = false;
	m_hThread = (HANDLE)::_beginthreadex(NULL, 0, CasAccessThread, this, 0, NULL);
	if (m_hThread == NULL)
		return false;
	if (m_Event.Wait(20000) == WAIT_TIMEOUT) {
		::TerminateThread(m_hThread, -1);
		::CloseHandle(m_hThread);
		m_hThread = NULL;
		SetError(TEXT("カードリーダーのオープンで、カードリーダーが応答しません。"));
		return false;
	}
	if (!m_pCasCard->IsCardOpen()) {
		::WaitForSingleObject(m_hThread, INFINITE);
		::CloseHandle(m_hThread);
		m_hThread = NULL;
		return false;
	}
	m_bStartEvent = true;
	m_bAvailable = true;
	ClearError();
	return true;
}

bool CCasAccessQueue::EndCasThread()
{
	if (m_hThread) {
		m_bAvailable = false;
		m_bKillEvent = true;
		m_Event.Set();
		if (::WaitForSingleObject(m_hThread, 5000) == WAIT_TIMEOUT) {
			TRACE(TEXT("Terminate CasAccessThread\n"));
			::TerminateThread(m_hThread, -1);
		}
		::CloseHandle(m_hThread);
		m_hThread = NULL;
	}
	Clear();
	return true;
}

unsigned int __stdcall CCasAccessQueue::CasAccessThread(LPVOID lpParameter)
{
	CCasAccessQueue *pThis=static_cast<CCasAccessQueue*>(lpParameter);

	// カードリーダからCASカードを検索して開く
	if (!pThis->m_pCasCard->OpenCard(pThis->m_ReaderType, pThis->m_pszReaderName)) {
		pThis->SetError(pThis->m_pCasCard->GetLastErrorException());
		pThis->m_Event.Set();
		return 1;
	}
	pThis->m_Event.Set();
	while (!pThis->m_bStartEvent)
		::Sleep(0);

	while (true) {
		pThis->m_Event.Wait();
		if (pThis->m_bKillEvent)
			break;
		while (true) {
			pThis->m_Lock.Lock();
			if (pThis->m_Queue.empty()) {
				pThis->m_Lock.Unlock();
				break;
			}
			CCasAccess *pCasAccess = pThis->m_Queue.front();
			pThis->m_Queue.pop_front();
			pThis->m_Lock.Unlock();
			pCasAccess->Process(pThis->m_pCasCard);
			delete pCasAccess;
		}
	}

	// CASカードを閉じる
	pThis->m_pCasCard->CloseCard();

	TRACE(TEXT("End CasAccessThread\n"));

	return 0;
}
