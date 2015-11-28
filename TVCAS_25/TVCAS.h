#ifndef TV_CAS_H
#define TV_CAS_H


#include <pshpack1.h>


namespace TVCAS
{

	enum
	{
		LIB_VERSION = 0x00000001UL,

		MAX_DEVICE_NAME = 64,
		MAX_DEVICE_TEXT = 64
	};

	struct ModuleInfo
	{
		DWORD LibVersion;
		DWORD Flags;
		LPCWSTR Name;
		LPCWSTR Version;
	};

	struct CasDeviceInfo
	{
		DWORD DeviceID;
		DWORD Flags;
		WCHAR Name[MAX_DEVICE_NAME];
		WCHAR Text[MAX_DEVICE_TEXT];
	};

	struct CasCardInfo
	{
		WORD CASystemID;
		BYTE CardID[6];
		BYTE CardType;
		BYTE MessagePartitionLength;
		BYTE SystemKey[32];
		BYTE InitialCBC[8];
		BYTE CardManufacturerID;
		BYTE CardVersion;
		WORD CheckCode;
		WCHAR CardIDText[32];
	};

	enum
	{
		EVENT_EMM_PROCESSED		= 0x00000001U,
		EVENT_EMM_ERROR			= 0x00000002U,
		EVENT_ECM_ERROR			= 0x00000003U,
		EVENT_ECM_REFUSED		= 0x00000004U,
		EVENT_CARD_READER_HUNG	= 0x00000005U
	};

	enum LogType
	{
		LOG_VERBOSE,
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR
	};

	struct ErrorInfo
	{
		int Code;
		LPCWSTR pszText;
		LPCWSTR pszAdvise;
		LPCWSTR pszSystemMessage;
	};

	struct EcmErrorInfo
	{
		LPCWSTR pszText;
		WORD EcmPID;
	};

	struct EmmErrorInfo
	{
		LPCWSTR pszText;
	};

	enum ContractStatus
	{
		CONTRACT_CONTRACTED,
		CONTRACT_UNCONTRACTED,
		CONTRACT_UNKNOWN,
		CONTRACT_ERROR
	};

	__interface __declspec(uuid("C19221E8-CBB1-4BEF-96A8-3F294244CBB0")) IBase
	{
		void Refer();
		void Release();
		LPCWSTR GetName() const;
		IBase * GetInterface(REFIID riid);
		bool GetProperty(LPCWSTR pszName, void *pProperty, SIZE_T *pSize) const;
		bool SetProperty(LPCWSTR pszName, const void *pProperty, SIZE_T Size);
	};

	__interface __declspec(uuid("973B85B2-CF36-4DF5-A2E8-663550B3BAEA")) ICasClient : public IBase
	{
		LRESULT OnEvent(UINT Event, void *pParam);
		LRESULT OnError(const ErrorInfo *pInfo);
		void OutLog(LogType Type, LPCWSTR pszMessage);
	};

	__interface __declspec(uuid("BC023720-CB97-462E-A142-ED37CE396FB3")) ICasDevice : public IBase
	{
		bool GetDeviceInfo(CasDeviceInfo *pInfo) const;
		int GetCardCount() const;
		bool GetCardName(int Index, LPWSTR pszName, int MaxName) const;
		bool IsCardAvailable(LPCWSTR pszName);
	};

	__interface __declspec(uuid("080EDD04-8215-4037-8B66-E03954FD81BD")) ICasManager : public IBase
	{
		bool Initialize(ICasClient *pClient);
		bool Reset();

		bool EnableDescramble(bool Enable);
		bool IsDescrambleEnabled() const;
		bool EnableContract(bool Enable);
		bool IsContractEnabled() const;

		int GetCasDeviceCount() const;
		bool GetCasDeviceInfo(int Device, CasDeviceInfo *pInfo) const;
		ICasDevice * OpenCasDevice(int Device);
		bool IsCasDeviceAvailable(int Device);
		bool CheckCasDeviceAvailability(int Device, bool *pAvailable, LPWSTR pszMessage, int MaxLength);
		int GetDefaultCasDevice();
		int GetCasDeviceByID(DWORD DeviceID) const;
		int GetCasDeviceByName(LPCWSTR pszName) const;

		bool OpenCasCard(int Device, LPCWSTR pszName = NULL);
		bool CloseCasCard();
		bool IsCasCardOpen() const;
		int GetCasDevice() const;
		int GetCasCardName(LPWSTR pszName, int MaxName) const;
		bool GetCasCardInfo(CasCardInfo *pInfo) const;
		bool SendCasCommand(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize);

		bool ProcessStream(const void *pSrcData, const DWORD SrcSize,
						   void **ppDstData, DWORD *pDstSize);
		bool ProcessPacket(void *pData, DWORD PacketSize);

		ULONGLONG GetInputPacketCount() const;
		ULONGLONG GetScramblePacketCount() const;
		void ResetScramblePacketCount();

		bool SetDescrambleServiceID(WORD ServiceID);
		WORD GetDescrambleServiceID() const;
		bool SetDescrambleServices(const WORD *pServiceIDList, int ServiceCount);
		bool GetDescrambleServices(WORD *pServiceIDList, int *pServiceCount) const;
		WORD GetEcmPIDByServiceID(WORD ServiceID) const;

		ContractStatus GetContractStatus(WORD NetworkID, WORD ServiceID, const SYSTEMTIME *pTime = NULL);
		ContractStatus GetContractPeriod(WORD NetworkID, WORD ServiceID, SYSTEMTIME *pTime);
		bool HasContractInfo(WORD NetworkID, WORD ServiceID) const;

		int GetInstructionName(int Instruction, LPWSTR pszName, int MaxName) const;
		UINT GetAvailableInstructions() const;
		bool SetInstruction(int Instruction);
		int GetInstruction() const;
		bool DescrambleBenchmarkTest(int Instruction, DWORD Round, DWORD *pTime);
	};

	typedef BOOL (WINAPI *GetModuleInfoFunc)(ModuleInfo *pInfo);
	typedef IBase * (WINAPI *CreateInstanceFunc)(REFIID riid);


	namespace Helper
	{

		namespace Module
		{

			inline GetModuleInfoFunc GetModuleInfo(HMODULE hLib)
			{
				return reinterpret_cast<GetModuleInfoFunc>(::GetProcAddress(hLib, "TVCAS_GetModuleInfo"));
			}

			inline CreateInstanceFunc CreateInstance(HMODULE hLib)
			{
				return reinterpret_cast<CreateInstanceFunc>(::GetProcAddress(hLib, "TVCAS_CreateInstance"));
			}

		}

#if (__cplusplus >= 201103L) || (defined(_MSC_VER) && (_MSC_VER >= 1700))
#define TVCAS_FINAL override final
#elif defined(_MSC_VER)
#define TVCAS_FINAL override
#else
#define TVCAS_FINAL
#endif

		class CBaseImplNoRef
		{
		protected:
			virtual void ReferImpl() {}
			virtual void ReleaseImpl() {}

			virtual IBase * GetInterfaceImpl(REFIID riid)
			{
				return NULL;
			}

			virtual bool GetPropertyImpl(LPCWSTR pszName, void *pProperty, SIZE_T *pSize) const
			{
				return false;
			}

			virtual bool SetPropertyImpl(LPCWSTR pszName, const void *pProperty, SIZE_T Size)
			{
				return false;
			}
		};

		class CBaseImpl : public CBaseImplNoRef
		{
		public:
			CBaseImpl() : m_RefCount(1) {}
			virtual ~CBaseImpl() {}

		protected:
			void ReferImpl() TVCAS_FINAL
			{
				::InterlockedIncrement(&m_RefCount);
			}

			void ReleaseImpl() TVCAS_FINAL
			{
				if (::InterlockedDecrement(&m_RefCount) == 0)
					delete this;
			}

		private:
			LONG m_RefCount;
		};

#define TVCAS_DECLARE_BASE \
	void Refer() TVCAS_FINAL { ReferImpl(); } \
	void Release() TVCAS_FINAL { ReleaseImpl(); } \
	TVCAS::IBase * GetInterface(REFIID riid) TVCAS_FINAL { return GetInterfaceImpl(riid); } \
	bool GetProperty(LPCWSTR pszName, void *pProperty, SIZE_T *pSize) const TVCAS_FINAL { return GetPropertyImpl(pszName, pProperty, pSize); } \
	bool SetProperty(LPCWSTR pszName, const void *pProperty, SIZE_T Size) TVCAS_FINAL { return SetPropertyImpl(pszName, pProperty, Size); }

	}	// namespace Helper

}	// namespace TVCAS


#include <poppack.h>


#endif
