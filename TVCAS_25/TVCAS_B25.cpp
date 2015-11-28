#include "stdafx.h"
#include "TVCAS.h"
#include "TVCAS_B25.h"
#include "TsPacketParser.h"
#include "TsDescrambler.h"
#include "Multi2Decoder.h"


#define LTEXT_(text) L##text
#define LTEXT(text) LTEXT_(text)


class CCasDevice
	: public TVCAS::ICasDevice
	, protected TVCAS::Helper::CBaseImpl
{
public:
	CCasDevice(const TVCAS::CasDeviceInfo &DeviceInfo, CCardReader *pCardReader);

// IBase
	TVCAS_DECLARE_BASE
	LPCWSTR GetName() const override;

// ICasDevice
	bool GetDeviceInfo(TVCAS::CasDeviceInfo *pInfo) const override;
	int GetCardCount() const override;
	bool GetCardName(int Index, LPWSTR pszName, int MaxName) const override;
	bool IsCardAvailable(LPCWSTR pszName) override;

private:
	~CCasDevice();

	TVCAS::CasDeviceInfo m_CasDeviceInfo;
	CCardReader *m_pCardReader;
	WCHAR m_Name[9 + TVCAS::MAX_DEVICE_NAME];
};

CCasDevice::CCasDevice(const TVCAS::CasDeviceInfo &DeviceInfo, CCardReader *pCardReader)
	: m_CasDeviceInfo(DeviceInfo)
	, m_pCardReader(pCardReader)
{
	::wsprintfW(m_Name, L"CasDevice%s", DeviceInfo.Name);
}

CCasDevice::~CCasDevice()
{
	delete m_pCardReader;
}

LPCWSTR CCasDevice::GetName() const
{
	return m_Name;
}

bool CCasDevice::GetDeviceInfo(TVCAS::CasDeviceInfo *pInfo) const
{
	if (pInfo == NULL)
		return false;

	*pInfo = m_CasDeviceInfo;

	return true;
}

int CCasDevice::GetCardCount() const
{
	return m_pCardReader->NumReaders();
}

bool CCasDevice::GetCardName(int Index, LPWSTR pszName, int MaxName) const
{
	LPCTSTR pszReader = m_pCardReader->EnumReader(Index);

	if (pszReader == NULL)
		return false;

	if (pszName != NULL && MaxName > 0)
		::lstrcpynW(pszName, pszReader, MaxName);

	return true;
}

bool CCasDevice::IsCardAvailable(LPCWSTR pszName)
{
	if (pszName == NULL)
		return false;

	return m_pCardReader->IsReaderAvailable(pszName);
}


class CCasManagerB25
	: public TVCAS::ICasManager
	, protected TVCAS::Helper::CBaseImpl
	, protected CMediaDecoder::IEventHandler
	, protected CTracer
{
public:
	CCasManagerB25();

// IBase
	TVCAS_DECLARE_BASE
	LPCWSTR GetName() const override;

// ICasManager
	bool Initialize(TVCAS::ICasClient *pClient) override;
	bool Reset() override;

	bool EnableDescramble(bool Enable) override;
	bool IsDescrambleEnabled() const override;
	bool EnableContract(bool Enable) override;
	bool IsContractEnabled() const override;

	int GetCasDeviceCount() const override;
	bool GetCasDeviceInfo(int Device, TVCAS::CasDeviceInfo *pInfo) const override;
	TVCAS::ICasDevice * OpenCasDevice(int Device) override;
	bool IsCasDeviceAvailable(int Device) override;
	bool CheckCasDeviceAvailability(int Device, bool *pAvailable, LPWSTR pszMessage, int MaxLength) override;
	int GetDefaultCasDevice() override;
	int GetCasDeviceByID(DWORD DeviceID) const override;
	int GetCasDeviceByName(LPCWSTR pszName) const override;

	bool OpenCasCard(int Device, LPCWSTR pszName) override;
	bool CloseCasCard() override;
	bool IsCasCardOpen() const override;
	int GetCasDevice() const override;
	int GetCasCardName(LPWSTR pszName, int MaxName) const override;
	bool GetCasCardInfo(TVCAS::CasCardInfo *pInfo) const override;
	bool SendCasCommand(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize) override;

	bool ProcessStream(const void *pSrcData, const DWORD SrcSize,
					   void **ppDstData, DWORD *pDstSize) override;
	bool ProcessPacket(void *pData, DWORD PacketSize) override;

	ULONGLONG GetInputPacketCount() const override;
	ULONGLONG GetScramblePacketCount() const override;
	void ResetScramblePacketCount() override;

	bool SetDescrambleServiceID(WORD ServiceID) override;
	WORD GetDescrambleServiceID() const override;
	bool SetDescrambleServices(const WORD *pServiceIDList, int ServiceCount) override;
	bool GetDescrambleServices(WORD *pServiceIDList, int *pServiceCount) const override;
	WORD GetEcmPIDByServiceID(WORD ServiceID) const override;

	TVCAS::ContractStatus GetContractStatus(WORD NetworkID, WORD ServiceID, const SYSTEMTIME *pTime) override;
	TVCAS::ContractStatus GetContractPeriod(WORD NetworkID, WORD ServiceID, SYSTEMTIME *pTime) override;
	bool HasContractInfo(WORD NetworkID, WORD ServiceID) const override;

	int GetInstructionName(int Instruction, LPWSTR pszName, int MaxName) const override;
	UINT GetAvailableInstructions() const override;
	bool SetInstruction(int Instruction) override;
	int GetInstruction() const override;
	bool DescrambleBenchmarkTest(int Instruction, DWORD Round, DWORD *pTime) override;

private:
	~CCasManagerB25();
	void OnError(const CBonException &Exception);

// CMediaDecoder::IEventHandler
	const DWORD OnDecoderEvent(CMediaDecoder *pDecoder, const DWORD dwEventID, PVOID pParam) override;

// CTracer
	void OnTrace(LPCTSTR pszOutput) override;

	class CDescrambleOutput : public CMediaDecoder
	{
	public:
		CDescrambleOutput(CMediaData *pData);

	// CMediaDecdoer
		const bool InputMedia(CMediaData *pMediaData, const DWORD dwInputIndex) override;

	private:
		CMediaData *m_pData;
	};

	struct ReaderDeviceInfo
	{
		CCardReader::ReaderType Type;
		TVCAS::CasDeviceInfo Info;
	};

	enum {
		DEVICE_SCARD,
#ifdef CARDREADER_BONCASCLIENT_SUPPORT
		DEVICE_BONCASCLIENT,
#endif
		NUM_DEVICES
	};

	enum {
		DEVICE_ID_SCARD        = 1
#ifdef CARDREADER_BONCASCLIENT_SUPPORT
		, DEVICE_ID_BONCASCLIENT = 2
#endif
	};

	static const ReaderDeviceInfo m_CasDeviceList[NUM_DEVICES];

	LONG m_RefCount;

	TVCAS::ICasClient *m_pClient;

	CTsPacketParser m_TsPacketParser;
	CTsDescrambler m_TsDescrambler;
	CDescrambleOutput m_Output;

	CCriticalLock m_GraphLock;

	CMediaData m_InputBuff;
	CMediaData m_OutputBuff;
};

const CCasManagerB25::ReaderDeviceInfo CCasManagerB25::m_CasDeviceList[NUM_DEVICES] =
{
	{CCardReader::READER_SCARD,			{DEVICE_ID_SCARD,			0,	TEXT("SmartCard"),		TEXT("スマートカードリーダー")}},
#ifdef CARDREADER_BONCASCLIENT_SUPPORT
	{CCardReader::READER_BONCASCLIENT,	{DEVICE_ID_BONCASCLIENT,	0,	TEXT("BonCasClient"),	TEXT("BonCasClient")}},
#endif
};

CCasManagerB25::CCasManagerB25()
	: m_pClient(NULL)
	, m_TsPacketParser(this)
	, m_TsDescrambler(this)
	, m_InputBuff(0x100000UL)
	, m_OutputBuff(0x100000UL)
	, m_Output(&m_OutputBuff)
{
	m_TsPacketParser.SetTracer(this);
	m_TsDescrambler.SetTracer(this);

	m_TsPacketParser.SetOutputNullPacket(true);

	m_TsPacketParser.SetOutputDecoder(&m_TsDescrambler);
	m_TsDescrambler.SetOutputDecoder(&m_Output);
}

CCasManagerB25::~CCasManagerB25()
{
	if (m_pClient != NULL)
		m_pClient->Release();
}

LPCWSTR CCasManagerB25::GetName() const
{
	return L"CasManagerB25";
}

bool CCasManagerB25::Initialize(TVCAS::ICasClient *pClient)
{
	if (pClient == NULL)
		return false;

	m_pClient = pClient;
	m_pClient->Refer();

	return Reset();
}

bool CCasManagerB25::Reset()
{
	CBlockLock Lock(&m_GraphLock);

	m_OutputBuff.ClearSize();

	m_TsPacketParser.ResetGraph();

	return true;
}

bool CCasManagerB25::EnableDescramble(bool Enable)
{
	return m_TsDescrambler.EnableDescramble(Enable);
}

bool CCasManagerB25::IsDescrambleEnabled() const
{
	return m_TsDescrambler.IsDescrambleEnabled();
}

bool CCasManagerB25::EnableContract(bool Enable)
{
	return m_TsDescrambler.EnableEmmProcess(Enable);
}

bool CCasManagerB25::IsContractEnabled() const
{
	return m_TsDescrambler.IsEmmProcessEnabled();
}

int CCasManagerB25::GetCasDeviceCount() const
{
	return NUM_DEVICES;
}

bool CCasManagerB25::GetCasDeviceInfo(int Device, TVCAS::CasDeviceInfo *pInfo) const
{
	if (Device < 0 || Device >= NUM_DEVICES || pInfo == NULL)
		return false;

	*pInfo = m_CasDeviceList[Device].Info;

	return true;
}

TVCAS::ICasDevice * CCasManagerB25::OpenCasDevice(int Device)
{
	if (Device < 0 || Device >= NUM_DEVICES)
		return NULL;

	CCardReader *pCardReader = CCardReader::CreateCardReader(m_CasDeviceList[Device].Type);
	if (pCardReader == NULL)
		return NULL;

	return new CCasDevice(m_CasDeviceList[Device].Info, pCardReader);
}

bool CCasManagerB25::IsCasDeviceAvailable(int Device)
{
	if (Device < 0 || Device >= NUM_DEVICES)
		return false;

	CCardReader *pCardReader = CCardReader::CreateCardReader(m_CasDeviceList[Device].Type);
	if (pCardReader == NULL)
		return false;

	bool Available = pCardReader->IsAvailable();

	delete pCardReader;

	return Available;
}

bool CCasManagerB25::CheckCasDeviceAvailability(int Device, bool *pAvailable, LPWSTR pszMessage, int MaxLength)
{
	if (Device < 0 || Device >= NUM_DEVICES
			|| pAvailable == NULL)
		return false;

	CCardReader *pCardReader = CCardReader::CreateCardReader(m_CasDeviceList[Device].Type);
	if (pCardReader == NULL)
		return false;

	bool Result = pCardReader->CheckAvailability(pAvailable, pszMessage, MaxLength);

	delete pCardReader;

	return Result;
}

int CCasManagerB25::GetDefaultCasDevice()
{
	CCardReader *pCardReader;
	int Device = -1;

	pCardReader = CCardReader::CreateCardReader(CCardReader::READER_SCARD);
	if (pCardReader != NULL) {
		if (pCardReader->IsAvailable())
			Device = DEVICE_SCARD;
		delete pCardReader;
	}

	return Device;
}

int CCasManagerB25::GetCasDeviceByID(DWORD DeviceID) const
{
	for (int i = 0; i < NUM_DEVICES; i++) {
		if (m_CasDeviceList[i].Info.DeviceID == DeviceID)
			return i;
	}

	return -1;
}

int CCasManagerB25::GetCasDeviceByName(LPCWSTR pszName) const
{
	if (pszName == NULL)
		return -1;

	for (int i = 0; i < NUM_DEVICES; i++) {
		if (::lstrcmpiW(m_CasDeviceList[i].Info.Name, pszName) == 0)
			return i;
	}

	return -1;
}

bool CCasManagerB25::OpenCasCard(int Device, LPCWSTR pszName)
{
	if (Device < 0 || Device >= NUM_DEVICES)
		return false;

	CCardReader::ReaderType Reader = m_CasDeviceList[Device].Type;

	if (Reader == CCardReader::READER_SCARD
			&& pszName != NULL
			&& (::PathMatchSpecW(pszName,L"*.scard")
			 || ::PathMatchSpecW(pszName,L"*.dll"))) {
#ifdef CARDREADER_SCARD_DYNAMIC_SUPPORT
		Reader = CCardReader::READER_SCARD_DYNAMIC;
#else
		pszName = NULL;
#endif
	}

	if (!m_TsDescrambler.OpenCasCard(Reader, pszName)) {
		OnError(m_TsDescrambler.GetLastErrorException());
		return false;
	}

	return true;
}

bool CCasManagerB25::CloseCasCard()
{
	m_TsDescrambler.CloseCasCard();

	return true;
}

bool CCasManagerB25::IsCasCardOpen() const
{
	return m_TsDescrambler.IsCasCardOpen();
}

int CCasManagerB25::GetCasDevice() const
{
	switch (m_TsDescrambler.GetCardReaderType()) {
	case CCardReader::READER_SCARD:
#ifdef CARDREADER_SCARD_DYNAMIC_SUPPORT
	case CCardReader::READER_SCARD_DYNAMIC:
#endif
		return DEVICE_SCARD;

#ifdef CARDREADER_BONCASCLIENT_SUPPORT
	case CCardReader::READER_BONCASCLIENT:
		return DEVICE_BONCASCLIENT;
#endif
	}

	return -1;
}

int CCasManagerB25::GetCasCardName(LPWSTR pszName, int MaxName) const
{
	LPCTSTR pszReader = m_TsDescrambler.GetCardReaderName();

	if (pszReader == NULL)
		return 0;

	if (pszName != NULL && MaxName > 0)
		::lstrcpyn(pszName, pszReader, MaxName);

	return ::lstrlen(pszReader);
}

bool CCasManagerB25::GetCasCardInfo(TVCAS::CasCardInfo *pInfo) const
{
	if (pInfo == NULL)
		return false;

	CCasCard::CasCardInfo CardInfo;

	if (!m_TsDescrambler.GetCasCardInfo(&CardInfo))
		return false;

	pInfo->CASystemID = CardInfo.CASystemID;
	::CopyMemory(pInfo->CardID, CardInfo.CardID, 6);
	pInfo->CardType = CardInfo.CardType;
	pInfo->MessagePartitionLength = CardInfo.MessagePartitionLength;
	::CopyMemory(pInfo->SystemKey, CardInfo.SystemKey, sizeof(CardInfo.SystemKey));
	::CopyMemory(pInfo->InitialCBC, CardInfo.InitialCbc, sizeof(CardInfo.InitialCbc));
	pInfo->CardManufacturerID = CardInfo.CardManufacturerID;
	pInfo->CardVersion = CardInfo.CardVersion;
	pInfo->CheckCode = CardInfo.CheckCode;
	m_TsDescrambler.FormatCasCardID(pInfo->CardIDText, _countof(pInfo->CardIDText));

	return true;
}

bool CCasManagerB25::SendCasCommand(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize)
{
	return m_TsDescrambler.SendCasCommand(
		static_cast<const BYTE *>(pSendData), SendSize,
		static_cast<BYTE *>(pRecvData), pRecvSize);
}

bool CCasManagerB25::ProcessStream(
	const void *pSrcData, const DWORD SrcSize,
	void **ppDstData, DWORD *pDstSize)
{
	CBlockLock Lock(&m_GraphLock);

	if (pSrcData == NULL || ppDstData == NULL || pDstSize == NULL)
		return false;

	m_OutputBuff.ClearSize();

	m_InputBuff.SetData(pSrcData, SrcSize);

	if (!m_TsPacketParser.InputMedia(&m_InputBuff))
		return false;

	*ppDstData = m_OutputBuff.GetData();
	*pDstSize = m_OutputBuff.GetSize();

	return true;
}

bool CCasManagerB25::ProcessPacket(void *pData, DWORD PacketSize)
{
	CBlockLock Lock(&m_GraphLock);

	m_OutputBuff.ClearSize();

	if (!m_TsPacketParser.InputPacket(pData, PacketSize))
		return false;

	if (m_OutputBuff.GetSize() != PacketSize)
		return false;

	::CopyMemory(pData, m_OutputBuff.GetData(), PacketSize);

	return true;
}

ULONGLONG CCasManagerB25::GetInputPacketCount() const
{
	return m_TsDescrambler.GetInputPacketCount();
}

ULONGLONG CCasManagerB25::GetScramblePacketCount() const
{
	return m_TsDescrambler.GetScramblePacketCount();
}

void CCasManagerB25::ResetScramblePacketCount()
{
	return m_TsDescrambler.ResetScramblePacketCount();
}

bool CCasManagerB25::SetDescrambleServiceID(WORD ServiceID)
{
	return m_TsDescrambler.SetTargetServiceID(ServiceID);
}

WORD CCasManagerB25::GetDescrambleServiceID() const
{
	return m_TsDescrambler.GetTargetServiceID();
}

bool CCasManagerB25::SetDescrambleServices(const WORD *pServiceIDList, int ServiceCount)
{
	// Not implemented
	return false;
}

bool CCasManagerB25::GetDescrambleServices(WORD *pServiceIDList, int *pServiceCount) const
{
	// Not implemented
	return false;
}

WORD CCasManagerB25::GetEcmPIDByServiceID(WORD ServiceID) const
{
	return m_TsDescrambler.GetEcmPIDByServiceID(ServiceID);
}

TVCAS::ContractStatus CCasManagerB25::GetContractStatus(WORD NetworkID, WORD ServiceID, const SYSTEMTIME *pTime)
{
	return (TVCAS::ContractStatus)m_TsDescrambler.GetContractStatus(NetworkID, ServiceID, pTime);
}

TVCAS::ContractStatus CCasManagerB25::GetContractPeriod(WORD NetworkID, WORD ServiceID, SYSTEMTIME *pTime)
{
	return (TVCAS::ContractStatus)m_TsDescrambler.GetContractPeriod(NetworkID, ServiceID, pTime);
}

bool CCasManagerB25::HasContractInfo(WORD NetworkID, WORD ServiceID) const
{
	return m_TsDescrambler.HasContractInfo(NetworkID, ServiceID);
}

int CCasManagerB25::GetInstructionName(int Instruction, LPWSTR pszName, int MaxName) const
{
	LPCWSTR pszInstruction;

	switch (Instruction) {
	case CTsDescrambler::INSTRUCTION_NORMAL:
		pszInstruction = L"拡張命令なし";
		break;

	case CTsDescrambler::INSTRUCTION_SSE2:
		pszInstruction = L"SSE2";
		break;

	case CTsDescrambler::INSTRUCTION_SSSE3:
		pszInstruction = L"SSSE3";
		break;

	default:
		return 0;
	}

	if (pszName != NULL && MaxName > 0)
		::lstrcpyn(pszName, pszInstruction, MaxName);

	return ::lstrlen(pszInstruction);
}

UINT CCasManagerB25::GetAvailableInstructions() const
{
	UINT Flags = 1 << CTsDescrambler::INSTRUCTION_NORMAL;

	if (CTsDescrambler::IsSSE2Available())
		Flags |= 1 << CTsDescrambler::INSTRUCTION_SSE2;
	if (CTsDescrambler::IsSSSE3Available())
		Flags |= 1 << CTsDescrambler::INSTRUCTION_SSSE3;

	return Flags;
}

bool CCasManagerB25::SetInstruction(int Instruction)
{
	CTsDescrambler::InstructionType Type;

	if (Instruction < 0) {
		if (CTsDescrambler::IsSSE2Available())
			Type = CTsDescrambler::INSTRUCTION_SSE2;
		if (CTsDescrambler::IsSSSE3Available())
			Type = CTsDescrambler::INSTRUCTION_SSSE3;
	} else {
		Type = (CTsDescrambler::InstructionType)Instruction;
	}

	return m_TsDescrambler.SetInstruction(Type);
}

int CCasManagerB25::GetInstruction() const
{
	return (int)m_TsDescrambler.GetInstruction();
}

static void FillRandomData(BYTE *pData, size_t Size)
{
	for (size_t i = 0; i < Size; i++)
		pData[i] = (BYTE)(::rand() & 0xFF);
}

bool CCasManagerB25::DescrambleBenchmarkTest(int Instruction, DWORD Round, DWORD *pTime)
{
	if (Instruction < CTsDescrambler::INSTRUCTION_NORMAL
			|| Instruction > CTsDescrambler::INSTRUCTION_SSSE3
			|| Round == 0 || pTime == NULL)
		return false;

	static const DWORD PACKETS_PER_SECOND = 10000;

	BYTE SystemKey[32], InitialCbc[8], ScrambleKey[4][16];
	__declspec(align(16)) BYTE PacketData[192];

	::srand(1234);

	FillRandomData(SystemKey, sizeof(SystemKey));
	FillRandomData(InitialCbc, sizeof(InitialCbc));
	FillRandomData(PacketData, sizeof(PacketData));
	FillRandomData(&ScrambleKey[0][0], sizeof(ScrambleKey));

	CMulti2Decoder Multi2DecoderNormal(
#ifdef MULTI2_SIMD
		CMulti2Decoder::INSTRUCTION_NORMAL
#endif
		);
	Multi2DecoderNormal.Initialize(SystemKey, InitialCbc);

	CMulti2Decoder *pMulti2Decoder = &Multi2DecoderNormal;

#ifdef MULTI2_SSE2
	CMulti2Decoder Multi2DecoderSSE2(CMulti2Decoder::INSTRUCTION_SSE2);
	if (Instruction == CTsDescrambler::INSTRUCTION_SSE2) {
		if (!CMulti2Decoder::IsSSE2Available())
			return false;
		Multi2DecoderSSE2.Initialize(SystemKey, InitialCbc);
		pMulti2Decoder = &Multi2DecoderSSE2;
	}
#else
	if (Instruction == CTsDescrambler::INSTRUCTION_SSE2)
		return false;
#endif

#ifdef MULTI2_SSSE3
	CMulti2Decoder Multi2DecoderSSSE3(CMulti2Decoder::INSTRUCTION_SSSE3);
	if (Instruction == CTsDescrambler::INSTRUCTION_SSSE3) {
		if (!CMulti2Decoder::IsSSSE3Available())
			return false;
		Multi2DecoderSSSE3.Initialize(SystemKey, InitialCbc);
		pMulti2Decoder = &Multi2DecoderSSSE3;
	}
#else
	if (Instructions == CTsDescrambler::INSTRUCTION_SSSE3)
		return false;
#endif

	BYTE ScramblingCtrl = 2;
	DWORD StartTime = ::timeGetTime();
	for (DWORD i = 0; i < Round; i++) {
		if (i % PACKETS_PER_SECOND == 0) {
			pMulti2Decoder->SetScrambleKey(
				ScrambleKey[(i / PACKETS_PER_SECOND) % _countof(ScrambleKey)]);
			ScramblingCtrl = ScramblingCtrl == 2 ? 3 : 2;
		}
		pMulti2Decoder->Decode(PacketData, 184, ScramblingCtrl);
	}

	*pTime = ::timeGetTime() - StartTime;

	return true;
}

void CCasManagerB25::OnError(const CBonException &Exception)
{
	if (m_pClient != NULL) {
		TVCAS::ErrorInfo Info;

		Info.Code = Exception.GetErrorCode();
		Info.pszText = Exception.GetText();
		Info.pszAdvise = Exception.GetAdvise();
		Info.pszSystemMessage = Exception.GetSystemMessage();

		m_pClient->OnError(&Info);
	}
}

const DWORD CCasManagerB25::OnDecoderEvent(CMediaDecoder *pDecoder, const DWORD dwEventID, PVOID pParam)
{
	if (pDecoder == &m_TsDescrambler) {
		switch (dwEventID) {
		case CTsDescrambler::EVENT_EMM_PROCESSED:
			if (m_pClient!=NULL)
				m_pClient->OnEvent(TVCAS::EVENT_EMM_PROCESSED, NULL);
			return 0;

		case CTsDescrambler::EVENT_EMM_ERROR:
			if (m_pClient!=NULL) {
				const CTsDescrambler::EmmErrorInfo *pInfo =
					static_cast<const CTsDescrambler::EmmErrorInfo *>(pParam);
				TVCAS::EmmErrorInfo Info;

				Info.pszText = pInfo->pszText;
				m_pClient->OnEvent(TVCAS::EVENT_EMM_ERROR, &Info);
			}
			return 0;

		case CTsDescrambler::EVENT_ECM_ERROR:
			if (m_pClient!=NULL) {
				const CTsDescrambler::EcmErrorInfo *pInfo =
					static_cast<const CTsDescrambler::EcmErrorInfo *>(pParam);
				TVCAS::EcmErrorInfo Info;

				Info.pszText = pInfo->pszText;
				Info.EcmPID = pInfo->EcmPID;
				m_pClient->OnEvent(TVCAS::EVENT_ECM_ERROR, &Info);
			}
			return 0;

		case CTsDescrambler::EVENT_ECM_REFUSED:
			if (m_pClient!=NULL) {
				const CTsDescrambler::EcmErrorInfo *pInfo =
					static_cast<const CTsDescrambler::EcmErrorInfo *>(pParam);
				TVCAS::EcmErrorInfo Info;

				Info.pszText = pInfo->pszText;
				Info.EcmPID = pInfo->EcmPID;
				m_pClient->OnEvent(TVCAS::EVENT_ECM_REFUSED, &Info);
			}
			return 0;

		case CTsDescrambler::EVENT_CARD_READER_HUNG:
			if (m_pClient!=NULL)
				m_pClient->OnEvent(TVCAS::EVENT_CARD_READER_HUNG, NULL);
			return 0;
		}
	}

	return 0;
}

void CCasManagerB25::OnTrace(LPCTSTR pszOutput)
{
	if (m_pClient != NULL)
		m_pClient->OutLog(TVCAS::LOG_INFO, pszOutput);
}


CCasManagerB25::CDescrambleOutput::CDescrambleOutput(CMediaData *pData)
	: CMediaDecoder(NULL, 1, 0)
	, m_pData(pData)
{
}

const bool CCasManagerB25::CDescrambleOutput::InputMedia(CMediaData *pMediaData, const DWORD dwInputIndex)
{
	m_pData->AddData(pMediaData);

	return true;
}




BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
		break;
	}

	return TRUE;
}

extern "C" __declspec(dllexport) BOOL WINAPI TVCAS_GetModuleInfo(TVCAS::ModuleInfo *pInfo)
{
	if (pInfo == NULL)
		return FALSE;

	pInfo->LibVersion = TVCAS::LIB_VERSION;
	pInfo->Flags = 0;
	pInfo->Name = LTEXT(TVCAS_MODULE_NAME);
	pInfo->Version = LTEXT(TVCAS_MODULE_VERSION);

	return TRUE;
}

extern "C" __declspec(dllexport) TVCAS::IBase * WINAPI TVCAS_CreateInstance(REFIID riid)
{
	if (riid == __uuidof(TVCAS::ICasManager))
		return new CCasManagerB25;

	return NULL;
}
