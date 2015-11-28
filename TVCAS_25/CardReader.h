#ifndef CARD_READER_H
#define CARD_READER_H


#include <winscard.h>
#include "BonBaseClass.h"


// カードリーダー基底クラス
class __declspec(novtable) CCardReader : public CBonBaseClass
{
public:
	enum ReaderType {
		READER_NONE,
		READER_SCARD,
#ifdef CARDREADER_SCARD_DYNAMIC_SUPPORT
		READER_SCARD_DYNAMIC,
#endif
#ifdef CARDREADER_BONCASCLIENT_SUPPORT
		READER_BONCASCLIENT,
#endif
		READER_TRAILER,
		READER_LAST = READER_TRAILER-1
	};

	CCardReader();
	virtual ~CCardReader();
	virtual bool Open(LPCTSTR pszReader = NULL) = 0;
	virtual void Close() = 0;
	virtual LPCTSTR GetReaderName() const = 0;
	virtual int NumReaders() const { return 1; }
	virtual LPCTSTR EnumReader(int Index) const;
	ReaderType GetReaderType() const { return m_ReaderType; }
	virtual bool IsAvailable() = 0;
	virtual bool IsReaderAvailable(LPCTSTR pszReader) = 0;
	virtual bool CheckAvailability(bool *pbAvailable, LPTSTR pszMessage, int MaxLength) = 0;
	virtual bool Transmit(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize) = 0;

	static CCardReader *CreateCardReader(ReaderType Type);

private:
	ReaderType m_ReaderType;
};

// スマートカードリーダー
class CSCardReader : public CCardReader
{
	SCARDCONTEXT m_SCardContext;
	SCARDHANDLE m_hSCard;
	bool m_bIsEstablish;
	LPTSTR m_pReaderList;
	int m_NumReaders;
	LPTSTR m_pszReaderName;

public:
	CSCardReader();
	~CSCardReader();

// CCardReader
	bool Open(LPCTSTR pszReader) override;
	void Close() override;
	LPCTSTR GetReaderName() const override;
	int NumReaders() const override;
	LPCTSTR EnumReader(int Index) const override;
	bool IsAvailable() override;
	bool IsReaderAvailable(LPCTSTR pszReader) override;
	bool CheckAvailability(bool *pbAvailable, LPTSTR pszMessage, int MaxLength) override;
	bool Transmit(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize) override;
};

#ifdef CARDREADER_SCARD_DYNAMIC_SUPPORT
class CDynamicSCardReader : public CCardReader
{
	HMODULE m_hLib;
	SCARDCONTEXT m_SCardContext;
	SCARDHANDLE m_hSCard;
	LPTSTR m_pReaderList;
	LPTSTR m_pszReaderName;
	typedef LONG (WINAPI *SCardReleaseContextFunc)(SCARDCONTEXT);
	typedef LONG (WINAPI *SCardConnectFunc)(SCARDCONTEXT, LPCTSTR, DWORD, DWORD, LPSCARDHANDLE, LPDWORD);
	typedef LONG (WINAPI *SCardDisconnectFunc)(SCARDHANDLE, DWORD);
	typedef LONG (WINAPI *SCardTransmitFunc)(SCARDHANDLE, LPCSCARD_IO_REQUEST, LPCBYTE,
											 DWORD, LPSCARD_IO_REQUEST, LPBYTE, LPDWORD);
	SCardReleaseContextFunc m_pSCardReleaseContext;
	SCardConnectFunc m_pSCardConnect;
	SCardDisconnectFunc m_pSCardDisconnect;
	SCardTransmitFunc m_pSCardTransmit;

	bool Load(LPCTSTR pszFileName);
	void Unload();

public:
	CDynamicSCardReader();
	~CDynamicSCardReader();

// CCardReader
	bool Open(LPCTSTR pszReader) override;
	void Close() override;
	LPCTSTR GetReaderName() const override;
	int NumReaders() const override;
	LPCTSTR EnumReader(int Index) const override;
	bool IsAvailable() override;
	bool IsReaderAvailable(LPCTSTR pszReader) override;
	bool CheckAvailability(bool *pbAvailable, LPTSTR pszMessage, int MaxLength) override;
	bool Transmit(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize) override;
};
#endif	// CARDREADER_SCARD_DYNAMIC_SUPPORT

#ifdef CARDREADER_BONCASCLIENT_SUPPORT
// BonCasClient
class CBonCasClientCardReader : public CCardReader
{
	HMODULE m_hLib;
	SCARDCONTEXT m_SCardContext;
	SCARDHANDLE m_hSCard;
	LPTSTR m_pReaderList;
	LPTSTR m_pszReaderName;
	typedef LONG (WINAPI *CasLinkReleaseContextFunc)(SCARDCONTEXT);
	typedef LONG (WINAPI *CasLinkConnectFunc)(SCARDCONTEXT, LPCTSTR, DWORD, DWORD, LPSCARDHANDLE, LPDWORD);
	typedef LONG (WINAPI *CasLinkDisconnectFunc)(SCARDHANDLE, DWORD);
	typedef LONG (WINAPI *CasLinkTransmitFunc)(SCARDHANDLE, LPCSCARD_IO_REQUEST, LPCBYTE,
											   DWORD, LPSCARD_IO_REQUEST, LPBYTE, LPDWORD);
	CasLinkReleaseContextFunc m_pCasLinkReleaseContext;
	CasLinkConnectFunc m_pCasLinkConnect;
	CasLinkDisconnectFunc m_pCasLinkDisconnect;
	CasLinkTransmitFunc m_pCasLinkTransmit;

	void GetModulePath(LPTSTR pszPath) const;
	bool Load(LPCTSTR pszFileName);
	bool Connect(LPCTSTR pszReader);

public:
	CBonCasClientCardReader();
	~CBonCasClientCardReader();

// CCardReader
	bool Open(LPCTSTR pszReader) override;
	void Close() override;
	LPCTSTR GetReaderName() const override;
	int NumReaders() const override;
	LPCTSTR EnumReader(int Index) const override;
	bool IsAvailable() override;
	bool IsReaderAvailable(LPCTSTR pszReader) override;
	bool CheckAvailability(bool *pbAvailable, LPTSTR pszMessage, int MaxLength) override;
	bool Transmit(const void *pSendData, DWORD SendSize, void *pRecvData, DWORD *pRecvSize) override;
};
#endif	// CARDREADER_BONCASCLIENT_SUPPORT


#endif
