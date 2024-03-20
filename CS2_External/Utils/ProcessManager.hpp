#pragma once
#include <iostream>
#include <Windows.h>
#include <vector>
#include <Tlhelp32.h>
#include <atlconv.h>
#include <leechcore.h>
#include <vmmdll.h>
#include <chrono>

#define _is_invalid(v) if(v==NULL) return false
#define _is_invalid(v,n) if(v==NULL) return n

/*
	@Liv github.com/TKazer
*/

/// <summary>
/// ����״̬��
/// </summary>
enum StatusCode
{
	SUCCEED,
	FAILE_PROCESSID,
	FAILE_HPROCESS,
	FAILE_MODULE,
};
struct Info
{
	uint32_t index;
	uint32_t process_id;
	uint64_t dtb;
	uint64_t kernelAddr;
	std::string name;
};


/// <summary>
/// ���̹���
/// </summary>
class ProcessManager
{
private:

	bool   Attached = false;

	uint64_t gafAsyncKeyStateExport = 0;
	uint8_t state_bitmap[64]{ };
	uint8_t previous_state_bitmap[256 / 8]{ };
	uint64_t win32kbase = 0;

	int win_logon_pid = 0;
	std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

	DWORD  ProcessID2 = 0;

public:
	std::string AttachProcessName;
	HANDLE hProcess = 0;
	DWORD  ProcessID = 0;
	VMM_HANDLE HANDLE;
public:
	~ProcessManager()
	{
		//if (hProcess)
			//CloseHandle(hProcess);
	}

	/// <summary>
	/// ����
	/// </summary>
	/// <param name="ProcessName">������</param>
	/// <returns>����״̬��</returns>
	StatusCode Attach(std::string ProcessName)
	{
		this->AttachProcessName = ProcessName;
		LPSTR args[] = { (LPSTR)"",(LPSTR)"-device", (LPSTR)"FPGA",(LPSTR)"-norefresh" };
		this->HANDLE = VMMDLL_Initialize(3, args);

		if (this->HANDLE) {
			SIZE_T pcPIDs;
			VMMDLL_PidList(this->HANDLE, nullptr, &pcPIDs);
			DWORD* pPIDs = (DWORD*)new char[pcPIDs * 4];
			VMMDLL_PidList(this->HANDLE, pPIDs, &pcPIDs);
			for (int i = 0; i < pcPIDs; i++)
			{
				VMMDLL_PROCESS_INFORMATION ProcessInformation = { 0 };
				ProcessInformation.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
				ProcessInformation.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;
				SIZE_T pcbProcessInformation = sizeof(VMMDLL_PROCESS_INFORMATION);
				VMMDLL_ProcessGetInformation(this->HANDLE, pPIDs[i], &ProcessInformation, &pcbProcessInformation);


				if (strcmp(ProcessInformation.szName, "cs2.exe") == 0) {
					//std::cout << pPIDs[i] << "---" << ProcessInformation.szName << std::endl;  // �����ǰ���̵�PID������
					ProcessID = pPIDs[i];
				}


			}
		}
		//VMMDLL_PidGetFromName((LPSTR)ProcessName.c_str(), &ProcessID);
		_is_invalid(ProcessID, FAILE_PROCESSID);

		//hProcess = OpenProcess(PROCESS_ALL_ACCESS | PROCESS_CREATE_THREAD, TRUE, ProcessID);
		//_is_invalid(hProcess, FAILE_HPROCESS);

		Attached = true;

		return SUCCEED;
	}

	/// <summary>
	/// ȡ������
	/// </summary>
	void Detach()
	{
		if (hProcess)
			CloseHandle(hProcess);
		hProcess = 0;
		ProcessID = 0;
		Attached = false;
	}

	/// <summary>
	/// �жϽ����Ƿ񼤻�״̬
	/// </summary>
	/// <returns>�Ƿ񼤻�״̬</returns>
	bool IsActive()
	{
		if (!Attached)
			return false;
		DWORD ExitCode{};
		//GetExitCodeProcess(hProcess, &ExitCode);
		return true;
	}

	/// <summary>
	/// ��ȡ�����ڴ�
	/// </summary>
	/// <typeparam name="ReadType">��ȡ����</typeparam>
	/// <param name="Address">��ȡ��ַ</param>
	/// <param name="Value">��������</param>
	/// <param name="Size">��ȡ��С</param>
	/// <returns>�Ƿ��ȡ�ɹ�</returns>
	template <typename ReadType>
	bool ReadMemory(DWORD64 Address, ReadType& Value, int Size)
	{
		//_is_invalid(hProcess,false);
		_is_invalid(ProcessID, false);
		if (VMMDLL_MemReadEx(this->HANDLE, ProcessID, Address, (PBYTE)&Value, Size, 0, VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_NOPAGING | VMMDLL_FLAG_ZEROPAD_ON_FAIL | VMMDLL_FLAG_NOPAGING_IO))
			return true;
		return false;
	}

	template <typename ReadType>
	bool ReadMemory(DWORD64 Address, ReadType& Value)
	{
		//_is_invalid(hProcess, false);
		_is_invalid(ProcessID, false);

		if (VMMDLL_MemReadEx(this->HANDLE, ProcessID, Address, (PBYTE)&Value, sizeof(ReadType), 0, VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_NOPAGING | VMMDLL_FLAG_ZEROPAD_ON_FAIL | VMMDLL_FLAG_NOPAGING_IO))
			return true;
		return false;
	}

	VMMDLL_SCATTER_HANDLE CreateScatterHandle()
	{
		return VMMDLL_Scatter_Initialize(this->HANDLE, ProcessID, VMMDLL_FLAG_NOCACHE);
	}

	void AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
	{
		VMMDLL_Scatter_PrepareEx(handle, address, size, (PBYTE)buffer, NULL);
	}
	void ExecuteReadScatter(VMMDLL_SCATTER_HANDLE handle)
	{
		VMMDLL_Scatter_ExecuteRead(handle);
		VMMDLL_Scatter_Clear(handle, ProcessID, NULL);
	}

	/// <summary>
	/// д������ڴ�
	/// </summary>
	/// <typeparam name="ReadType">д������</typeparam>
	/// <param name="Address">д���ַ</param>
	/// <param name="Value">д������</param>
	/// <param name="Size">д���С</param>
	/// <returns>�Ƿ�д��ɹ�</returns>
	template <typename ReadType>
	bool WriteMemory(DWORD64 Address, ReadType& Value, int Size)
	{
		//_is_invalid(hProcess, false);
		_is_invalid(ProcessID, false);
		if (VMMDLL_MemWrite(this->HANDLE, ProcessID, Address, (PBYTE)&Value, Size))
			return true;
		return false;
	}

	template <typename ReadType>
	bool WriteMemory(DWORD64 Address, ReadType& Value)
	{
		//_is_invalid(hProcess, false);
		_is_invalid(ProcessID, false);

		if (VMMDLL_MemWrite(this->HANDLE, ProcessID, Address, (PBYTE)&Value, sizeof(ReadType)))
			return true;
		return false;
	}

	/// <summary>
	/// ����������
	/// </summary>
	/// <param name="Signature">������</param>
	/// <param name="StartAddress">��ʼ��ַ</param>
	/// <param name="EndAddress">������ַ</param>
	/// <returns>ƥ���������</returns>
	std::vector<DWORD64> SearchMemory(const std::string& Signature, DWORD64 StartAddress, DWORD64 EndAddress, int SearchNum = 1);



	DWORD64 TraceAddress(DWORD64 BaseAddress, std::vector<DWORD> Offsets)
	{
		//_is_invalid(hProcess,0);
		_is_invalid(ProcessID, 0);
		DWORD64 Address = 0;

		if (Offsets.size() == 0)
			return BaseAddress;

		if (!ReadMemory<DWORD64>(BaseAddress, Address))
			return 0;

		for (int i = 0; i < Offsets.size() - 1; i++)
		{
			if (!ReadMemory<DWORD64>(Address + Offsets[i], Address))
				return 0;
		}
		return Address == 0 ? 0 : Address + Offsets[Offsets.size() - 1];
	}

	template <typename T>
	T ReadMemoryExtra(uintptr_t address, DWORD pid = ProcessID2, bool cache = false, const DWORD size = sizeof(T))
	{
		T buffer{};
		DWORD bytes_read = 0;
		if (!cache)
			VMMDLL_MemReadEx(this->HANDLE, pid, address, (PBYTE)&buffer, size, &bytes_read, VMMDLL_FLAG_NOCACHE);
		else
			VMMDLL_MemReadEx(this->HANDLE, pid, address, (PBYTE)&buffer, size, &bytes_read, VMMDLL_FLAG_CACHE_RECENT_ONLY);
		return buffer;
	}

	DWORD GetProcID_Keys(LPSTR proc_name)
	{
		DWORD procID = 0;
		VMMDLL_PidGetFromName(this->HANDLE, (LPSTR)proc_name, &procID);
		return procID;
	}

	void init_keystates() {
		this->win_logon_pid = GetProcID_Keys((LPSTR)"winlogon.exe");
		std::cout << "[ Keys ] Winlogon: " << this->win_logon_pid << std::endl;

		PVMMDLL_MAP_EAT eat_map = NULL;
		PVMMDLL_MAP_EATENTRY eat_map_entry;
		bool result = VMMDLL_Map_GetEATU(this->HANDLE, GetProcID_Keys((LPSTR)"winlogon.exe") | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, (LPSTR)"win32kbase.sys", &eat_map);
		std::cout << "[ Keys ] Eatu: " << result << std::endl;
		for (int i = 0; i < eat_map->cMap; i++)
		{
			eat_map_entry = eat_map->pMap + i;
			if (strcmp(eat_map_entry->uszFunction, "gafAsyncKeyState") == 0)
			{
				gafAsyncKeyStateExport = eat_map_entry->vaFunction;

				break;
			}
		}

		VMMDLL_MemFree(eat_map);
		eat_map = NULL;
	}

	void update_key_state_bitmap() {
		uint8_t previous_key_state_bitmap[64] = { 0 };
		memcpy(previous_key_state_bitmap, state_bitmap, 64);

		VMMDLL_MemReadEx(this->HANDLE, this->win_logon_pid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, gafAsyncKeyStateExport, (PBYTE)&state_bitmap, 64, NULL, VMMDLL_FLAG_NOCACHE);
		for (int vk = 0; vk < 256; ++vk)
			if ((state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2) && !(previous_key_state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2))
				previous_state_bitmap[vk / 8] |= 1 << vk % 8;
	}

	//KeyState Functions
	bool is_key_down(uint32_t virtual_key_code) {
		if (gafAsyncKeyStateExport < 0x7FFFFFFFFFFF)
			return false;
		if (std::chrono::system_clock::now() - start > std::chrono::milliseconds(1))
		{
			update_key_state_bitmap();
			start = std::chrono::system_clock::now();
		}
		return state_bitmap[(virtual_key_code * 2 / 8)] & 1 << virtual_key_code % 4 * 2;
	}


};

inline ProcessManager ProcessMgr;