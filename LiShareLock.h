#pragma once

#define		MAX_READER_COUNT		0x7FFFFFFF
class CLiShareLockEx{
public:
	CLiShareLockEx(){
		LockFlag_				= 0;
		WriterLockWaitFlag_		= 0;
		ReaderHoldCount_		= 0;
		WaitWriterEvent_		= CreateEvent(NULL,FALSE,FALSE,NULL);
		WaitReaderEvent_		= CreateEvent(NULL,TRUE,FALSE,NULL);
	};

	virtual ~CLiShareLockEx(){
		CloseHandle(WaitWriterEvent_);
	};
private:
	BOOL _TryReadLock()
	{
		LONG  ReaderCount = LockFlag_;
		LONG  Destination;

		if (ReaderCount < 0 || ReaderCount == MAX_READER_COUNT)
			return FALSE;
		Destination = InterlockedCompareExchange(&LockFlag_,ReaderCount+1,ReaderCount);
		return Destination == (ReaderCount);
	};

	BOOL _TryReadUnlock()
	{
		LONG  Destination;
		Destination = InterlockedDecrement(&LockFlag_);
		if (Destination < 0)
		{
			OutputDebugString(_T("ShareLock::::_TryReadUnlock Error"));
			InterlockedIncrement(&LockFlag_);
			return FALSE;
		}
		return Destination == 0;
	};


	BOOL TryReadLock(ULONG Timeout)
	{
		ULONG  i = Timeout; 
		do 
		{
			if (WriterLockWaitFlag_ == 0 && _TryReadLock())
			{
				InterlockedIncrement(&ReaderHoldCount_);
				return TRUE;
			}

			if (i > 0 && WaitForSingleObject(WaitReaderEvent_,INFINITE) == WAIT_OBJECT_0) 
				i--;		
		} while (i > 0);
		return FALSE;
	}

public:
	VOID ReadLock()
	{
		while(!TryReadLock(1000)){Sleep(5);};
	}

	VOID ReadUnlock()
	{
		if (_TryReadUnlock())
		{
			SetEvent(WaitWriterEvent_);
		}
	}

private:
	BOOL _TryWriteLock()
	{
		LONG  ReaderCount = LockFlag_;
		LONG  Destination;

		if (ReaderCount != 0)
			return FALSE;
		Destination = InterlockedCompareExchange(&LockFlag_,-1,0);
		return Destination == 0;
	};

	BOOL _TryWriteUnlock()
	{
		LONG  Destination;
		Destination = InterlockedCompareExchange(&LockFlag_,0,-1);
		if (Destination != -1)
		{
			OutputDebugString(_T("ShareLock::::_TryWriteUnlock Error"));
		}
		return Destination == -1;
	};

	BOOL TryWriteLock(ULONG Timeout)
	{
		ULONG  i = Timeout; 
		BOOL   r = FALSE;

		do 
		{
			if (_TryWriteLock())
			{
				ResetEvent(WaitReaderEvent_);
				ReaderHoldCount_ = 0;
				r = TRUE;
				break;
			}

			if (ReaderHoldCount_ > 0) 
			{
				ResetEvent(WaitReaderEvent_);
				InterlockedCompareExchange(&WriterLockWaitFlag_,1,0);
			}

			if (i> 0 && WaitForSingleObject(WaitWriterEvent_,INFINITE) != WAIT_OBJECT_0)
			{
				i--;
			}
		} while(i > 0);

		InterlockedCompareExchange(&WriterLockWaitFlag_,0,1);
		return r;
	}
public:
	VOID WriteLock()
	{
		while(!TryWriteLock(1000)){};
	}

	VOID WriteUnlock()
	{
		_TryWriteUnlock();
		SetEvent(WaitReaderEvent_);
		SetEvent(WaitWriterEvent_);
	}
public:
	VOID Lock(){WriteLock();}
	VOID Unlock(){WriteUnlock();}
private:
	HANDLE		WaitWriterEvent_;
	HANDLE		WaitReaderEvent_;
	LONG	 	LockFlag_;
	LONG		ReaderHoldCount_;
	LONG		WriterLockWaitFlag_;
};

class CLiShareLock{
public:
	CLiShareLock(){
		HMODULE	hModule = LoadLibrary(_T("ntdll.dll"));
		Init = FALSE;
		if (hModule == NULL)
			return;
		RtlInitializeSRWLock = (FN_RtlInitializeSRWLock)GetProcAddress(hModule,"RtlInitializeSRWLock");
		RtlAcquireSRWLockExclusive = (FN_RtlAcquireSRWLockExclusive)GetProcAddress(hModule,"RtlAcquireSRWLockExclusive");
		RtlAcquireSRWLockShared = (FN_RtlAcquireSRWLockShared)GetProcAddress(hModule,"RtlAcquireSRWLockShared");
		RtlReleaseSRWLockExclusive = (FN_RtlReleaseSRWLockExclusive)GetProcAddress(hModule,"RtlReleaseSRWLockExclusive");
		RtlReleaseSRWLockShared = (FN_RtlReleaseSRWLockShared)GetProcAddress(hModule,"RtlReleaseSRWLockShared");

		Init =	(RtlInitializeSRWLock != NULL)	&&
				(RtlAcquireSRWLockExclusive != NULL)	&&
				(RtlAcquireSRWLockShared != NULL)	&&
				(RtlReleaseSRWLockExclusive != NULL)	&&
				(RtlReleaseSRWLockShared != NULL);
		
		if (Init)
		{
			RtlInitializeSRWLock(&SRWLock);
		}
	};

	virtual ~CLiShareLock(){
	};
public:
	VOID ReadLock()
	{
		if (!Init) 
		{
			LiShareLock.ReadLock();
			return;
		}
		
		RtlAcquireSRWLockShared(&SRWLock);
	}

	VOID ReadUnlock()
	{
		if (!Init)
		{
			LiShareLock.ReadUnlock();
			return ;
		}

		RtlReleaseSRWLockShared(&SRWLock);
	}

public:
	VOID WriteLock()
	{
		if (!Init) 
		{
			LiShareLock.WriteLock();
			return ;
		}

		RtlAcquireSRWLockExclusive(&SRWLock);
	}

	VOID WriteUnlock()
	{
		if (!Init) 
		{
			LiShareLock.WriteUnlock();
			return ;
		}

		RtlReleaseSRWLockExclusive(&SRWLock);
	}
public:
	VOID Lock(){WriteLock();}
	VOID Unlock(){WriteUnlock();}
private:
	typedef struct __SRWLOCK {                            
		PVOID Ptr;                                       
	} __SRWLOCK, *P__SRWLOCK;
	typedef VOID (WINAPI *FN_RtlInitializeSRWLock)(P__SRWLOCK SRWLock);
	typedef VOID (WINAPI *FN_RtlAcquireSRWLockExclusive)(P__SRWLOCK SRWLock);
	typedef VOID (WINAPI *FN_RtlAcquireSRWLockShared)(P__SRWLOCK SRWLock);
	typedef VOID (WINAPI *FN_RtlReleaseSRWLockExclusive)(P__SRWLOCK SRWLock);
	typedef VOID (WINAPI *FN_RtlReleaseSRWLockShared)(P__SRWLOCK SRWLock);


	FN_RtlInitializeSRWLock				RtlInitializeSRWLock;
	FN_RtlAcquireSRWLockExclusive		RtlAcquireSRWLockExclusive;
	FN_RtlAcquireSRWLockShared			RtlAcquireSRWLockShared;
	FN_RtlReleaseSRWLockExclusive		RtlReleaseSRWLockExclusive;
	FN_RtlReleaseSRWLockShared			RtlReleaseSRWLockShared;
	__SRWLOCK							SRWLock;
	BOOL								Init;
	CLiShareLockEx						LiShareLock;
};
