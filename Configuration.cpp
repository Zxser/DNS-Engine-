﻿// This code is part of Pcap_DNSProxy
// Pcap_DNSProxy, a local DNS server based on WinPcap and LibPcap
// Copyright (C) 2012-2018 Chengr28
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Configuration.h"

//Global variables
size_t ParameterHopLimitsIndex[]{0, 0};

//Read texts
bool ReadText(
	const FILE * const FileHandle, 
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex)
{
//Initialization
	const auto FileBuffer = std::make_unique<uint8_t[]>(FILE_BUFFER_SIZE + PADDING_RESERVED_BYTES);
	const auto TextBuffer = std::make_unique<uint8_t[]>(FILE_BUFFER_SIZE + PADDING_RESERVED_BYTES);
	memset(FileBuffer.get(), 0, FILE_BUFFER_SIZE + PADDING_RESERVED_BYTES);
	memset(TextBuffer.get(), 0, FILE_BUFFER_SIZE + PADDING_RESERVED_BYTES);
	std::string TextData;
	auto LabelType_IPFilter = LABEL_IPFILTER_TYPE::NONE;
	auto LabelType_Hosts = LABEL_HOSTS_TYPE::NONE;
	size_t Encoding = 0, Index = 0, Line = 0;
	auto IsEraseBOM = true, NewLinePoint = false, IsStopLabel = false;

//Reset global variables.
	if (InputType == READ_TEXT_TYPE::PARAMETER_NORMAL || InputType == READ_TEXT_TYPE::PARAMETER_MONITOR)
	{
		ParameterHopLimitsIndex[NETWORK_LAYER_TYPE_IPV6] = 0;
		ParameterHopLimitsIndex[NETWORK_LAYER_TYPE_IPV4] = 0;
	}

//Read data.
	while (!feof(const_cast<FILE *>(FileHandle)))
	{
	//Read file and mark last read.
		_set_errno(0);
		auto ReadLength = fread_s(FileBuffer.get(), FILE_BUFFER_SIZE, sizeof(uint8_t), FILE_BUFFER_SIZE, const_cast<FILE *>(FileHandle));
		if (ReadLength == 0)
		{
			if (errno != 0)
			{
				ReadTextPrintLog(InputType, FileIndex, Line);
				return false;
			}
			else {
				continue;
			}
		}

	//Erase BOM of Unicode Transformation Format/UTF at first.
		if (IsEraseBOM)
		{
			if (ReadLength <= READ_DATA_MINSIZE)
			{
				ReadTextPrintLog(InputType, FileIndex, Line);
				return false;
			}
			else {
				IsEraseBOM = false;
			}

		//8-bit Unicode Transformation Format/UTF-8 with BOM
			if (FileBuffer.get()[0] == 0xEF && FileBuffer.get()[1U] == 0xBB && FileBuffer.get()[2U] == 0xBF) //0xEF, 0xBB, 0xBF
			{
				memmove_s(FileBuffer.get(), FILE_BUFFER_SIZE, FileBuffer.get() + BOM_UTF_8_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_8_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_8_LENGTH, 0, BOM_UTF_8_LENGTH);
				ReadLength -= BOM_UTF_8_LENGTH;
				Encoding = CODEPAGE_UTF_8;
			}
		//32-bit Unicode Transformation Format/UTF-32 Little Endian/LE
			else if (FileBuffer.get()[0] == 0xFF && FileBuffer.get()[1U] == 0xFE && 
				FileBuffer.get()[2U] == 0 && FileBuffer.get()[3U] == 0) //0xFF, 0xFE, 0x00, 0x00
			{
				memmove_s(FileBuffer.get(), FILE_BUFFER_SIZE, FileBuffer.get() + BOM_UTF_32_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH, 0, BOM_UTF_32_LENGTH);
				ReadLength -= BOM_UTF_32_LENGTH;
				Encoding = CODEPAGE_UTF_32_LE;
			}
		//32-bit Unicode Transformation Format/UTF-32 Big Endian/BE
			else if (FileBuffer.get()[0] == 0 && FileBuffer.get()[1U] == 0 && 
				FileBuffer.get()[2U] == 0xFE && FileBuffer.get()[3U] == 0xFF) //0x00, 0x00, 0xFE, 0xFF
			{
				memmove_s(FileBuffer.get(), FILE_BUFFER_SIZE, FileBuffer.get() + BOM_UTF_32_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_32_LENGTH, 0, BOM_UTF_32_LENGTH);
				ReadLength -= BOM_UTF_32_LENGTH;
				Encoding = CODEPAGE_UTF_32_BE;
			}
		//16-bit Unicode Transformation Format/UTF-16 Little Endian/LE
			else if (FileBuffer.get()[0] == 0xFF && FileBuffer.get()[1U] == 0xFE) //0xFF, 0xFE
			{
				memmove_s(FileBuffer.get(), FILE_BUFFER_SIZE, FileBuffer.get() + BOM_UTF_16_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH, 0, BOM_UTF_16_LENGTH);
				ReadLength -= BOM_UTF_16_LENGTH;
				Encoding = CODEPAGE_UTF_16_LE;
			}
		//16-bit Unicode Transformation Format/UTF-16 Big Endian/BE
			else if (FileBuffer.get()[0] == 0xFE && FileBuffer.get()[1U] == 0xFF) //0xFE, 0xFF
			{
				memmove_s(FileBuffer.get(), FILE_BUFFER_SIZE, FileBuffer.get() + BOM_UTF_16_LENGTH, FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH);
				memset(FileBuffer.get() + FILE_BUFFER_SIZE - BOM_UTF_16_LENGTH, 0, BOM_UTF_16_LENGTH);
				ReadLength -= BOM_UTF_16_LENGTH;
				Encoding = CODEPAGE_UTF_16_BE;
			}
		//8-bit Unicode Transformation Format/UTF-8 without BOM or other ASCII part of encoding
			else {
				Encoding = CODEPAGE_ASCII;
			}
		}

	//Text check
		if (Encoding == CODEPAGE_ASCII || Encoding == CODEPAGE_UTF_8)
		{
			uint16_t SingleText = 0;
			for (Index = 0;Index < ReadLength;)
			{
			//About this check process, please visit https://en.wikipedia.org/wiki/UTF-8.
				if (FileBuffer.get()[Index] > 0xE0 && Index >= 3U)
				{
					SingleText = ((static_cast<uint16_t>(FileBuffer.get()[Index] & 0x0F)) << 12U) + ((static_cast<uint16_t>(FileBuffer.get()[Index + 1U] & 0x3F)) << 6U) + static_cast<uint16_t>(FileBuffer.get()[Index + 2U] & 0x3F);

				//Next line format
					if (SingleText == UNICODE_LINE_SEPARATOR || SingleText == UNICODE_PARAGRAPH_SEPARATOR)
					{
						FileBuffer.get()[Index] = 0;
						FileBuffer.get()[Index + 1U] = 0;
						FileBuffer.get()[Index + 2U] = ASCII_LF;
						Index += 3U;
						continue;
					}
				//Space format
					else if (SingleText == UNICODE_MONGOLIAN_VOWEL_SEPARATOR || SingleText == UNICODE_EN_SPACE || SingleText == UNICODE_EM_SPACE || 
						SingleText == UNICODE_THICK_SPACE || SingleText == UNICODE_MID_SPACE || SingleText == UNICODE_SIX_PER_EM_SPACE || 
						SingleText == UNICODE_FIGURE_SPACE || SingleText == UNICODE_PUNCTUATION_SPACE || SingleText == UNICODE_THIN_SPACE || 
						SingleText == UNICODE_HAIR_SPACE || SingleText == UNICODE_ZERO_WIDTH_SPACE || SingleText == UNICODE_ZERO_WIDTH_NON_JOINER || 
						SingleText == UNICODE_ZERO_WIDTH_JOINER || SingleText == UNICODE_NARROW_NO_BREAK_SPACE || SingleText == UNICODE_MEDIUM_MATHEMATICAL_SPACE || 
						SingleText == UNICODE_WORD_JOINER || SingleText == UNICODE_IDEOGRAPHIC_SPACE)
					{
						FileBuffer.get()[Index] = ASCII_SPACE;
						FileBuffer.get()[Index + 1U] = 0;
						FileBuffer.get()[Index + 2U] = 0;
						Index += 3U;
						continue;
					}
				}
				else if (FileBuffer.get()[Index] > 0xC0 && Index >= 2U)
				{
					SingleText = ((static_cast<uint16_t>(FileBuffer.get()[Index] & 0x1F)) << 6U) + static_cast<uint16_t>(FileBuffer.get()[Index] & 0x3F);

				//Next line format
					if (SingleText == UNICODE_NEXT_LINE)
					{
						FileBuffer.get()[Index] = 0;
						FileBuffer.get()[Index + 1U] = ASCII_LF;
						Index += 2U;
						continue;
					}
				//Space format
					else if (SingleText == UNICODE_NO_BREAK_SPACE)
					{
						FileBuffer.get()[Index] = ASCII_SPACE;
						FileBuffer.get()[Index + 1U] = 0;
						Index += 2U;
						continue;
					}
				}

			//Delete all Non-ASCII.
				if (FileBuffer.get()[Index] > ASCII_MAX_NUM)
					FileBuffer.get()[Index] = 0;
			//Next line format
				else if (FileBuffer.get()[Index] == ASCII_CR)
					FileBuffer.get()[Index] = 0;
				else if (FileBuffer.get()[Index] == ASCII_VT || FileBuffer.get()[Index] == ASCII_FF)
					FileBuffer.get()[Index] = ASCII_LF;

			//Next text
				++Index;
			}
		}
		else if (Encoding == CODEPAGE_UTF_16_LE || Encoding == CODEPAGE_UTF_16_BE)
		{
			for (Index = 0;Index < ReadLength;Index += sizeof(uint16_t))
			{
				auto SingleText = reinterpret_cast<uint16_t *>(FileBuffer.get() + Index);

			//Endian
			#if BYTE_ORDER == LITTLE_ENDIAN
				if (Encoding == CODEPAGE_UTF_16_BE)
					*SingleText = ntoh16_Force(*SingleText);
			#else
				if (Encoding == CODEPAGE_UTF_16_LE)
					*SingleText = ntoh16_Force(*SingleText);
			#endif
			//Next line format
				if (*SingleText == ASCII_CR)
					*SingleText = 0;
				else if (*SingleText == ASCII_CR || *SingleText == ASCII_VT || *SingleText == ASCII_FF || *SingleText == UNICODE_NEXT_LINE || 
					*SingleText == UNICODE_LINE_SEPARATOR || *SingleText == UNICODE_PARAGRAPH_SEPARATOR)
						*SingleText = ASCII_LF;
			//Space format
				else if (*SingleText == UNICODE_NO_BREAK_SPACE || *SingleText == UNICODE_MONGOLIAN_VOWEL_SEPARATOR || *SingleText == UNICODE_EN_SPACE || 
					*SingleText == UNICODE_EM_SPACE || *SingleText == UNICODE_THICK_SPACE || *SingleText == UNICODE_MID_SPACE || 
					*SingleText == UNICODE_SIX_PER_EM_SPACE || *SingleText == UNICODE_FIGURE_SPACE || *SingleText == UNICODE_PUNCTUATION_SPACE || 
					*SingleText == UNICODE_THIN_SPACE || *SingleText == UNICODE_HAIR_SPACE || *SingleText == UNICODE_ZERO_WIDTH_SPACE || 
					*SingleText == UNICODE_ZERO_WIDTH_NON_JOINER || *SingleText == UNICODE_ZERO_WIDTH_JOINER || *SingleText == UNICODE_NARROW_NO_BREAK_SPACE || 
					*SingleText == UNICODE_MEDIUM_MATHEMATICAL_SPACE || *SingleText == UNICODE_WORD_JOINER || *SingleText == UNICODE_IDEOGRAPHIC_SPACE)
						*SingleText = ASCII_SPACE;
			//Delete all Non-ASCII.
				else if (*SingleText > ASCII_MAX_NUM)
					*SingleText = 0;
			}
		}
		else if (Encoding == CODEPAGE_UTF_32_LE || Encoding == CODEPAGE_UTF_32_BE)
		{
			for (Index = 0;Index < ReadLength;Index += sizeof(uint32_t))
			{
				auto SingleText = reinterpret_cast<uint32_t *>(FileBuffer.get() + Index);

			//Endian
			#if BYTE_ORDER == LITTLE_ENDIAN
				if (Encoding == CODEPAGE_UTF_32_BE)
					*SingleText = ntoh32_Force(*SingleText);
			#else
				if (Encoding == CODEPAGE_UTF_32_LE)
					*SingleText = ntoh32_Force(*SingleText);
			#endif
			//Next line format
				if (*SingleText == ASCII_CR)
					*SingleText = 0;
				else if (*SingleText == ASCII_CR || *SingleText == ASCII_VT || *SingleText == ASCII_FF || *SingleText == UNICODE_NEXT_LINE || 
					*SingleText == UNICODE_LINE_SEPARATOR || *SingleText == UNICODE_PARAGRAPH_SEPARATOR)
						*SingleText = ASCII_LF;
			//Space format
				else if (*SingleText == UNICODE_NO_BREAK_SPACE || *SingleText == UNICODE_MONGOLIAN_VOWEL_SEPARATOR || *SingleText == UNICODE_EN_SPACE || 
					*SingleText == UNICODE_EM_SPACE || *SingleText == UNICODE_THICK_SPACE || *SingleText == UNICODE_MID_SPACE || 
					*SingleText == UNICODE_SIX_PER_EM_SPACE || *SingleText == UNICODE_FIGURE_SPACE || *SingleText == UNICODE_PUNCTUATION_SPACE || 
					*SingleText == UNICODE_THIN_SPACE || *SingleText == UNICODE_HAIR_SPACE || *SingleText == UNICODE_ZERO_WIDTH_SPACE || 
					*SingleText == UNICODE_ZERO_WIDTH_NON_JOINER || *SingleText == UNICODE_ZERO_WIDTH_JOINER || *SingleText == UNICODE_NARROW_NO_BREAK_SPACE || 
					*SingleText == UNICODE_MEDIUM_MATHEMATICAL_SPACE || *SingleText == UNICODE_WORD_JOINER || *SingleText == UNICODE_IDEOGRAPHIC_SPACE)
						*SingleText = ASCII_SPACE;
			//Delete all Non-ASCII.
				else if (*SingleText > ASCII_MAX_NUM)
					*SingleText = 0;
			}
		}
		else {
			switch (InputType)
			{
				case READ_TEXT_TYPE::HOSTS: //ReadHosts
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::HOSTS, L"Text encoding error", 0, FileList_Hosts.at(FileIndex).FileName.c_str(), 0);
				}break;
				case READ_TEXT_TYPE::IPFILTER: //ReadIPFilter
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::IPFILTER, L"Text encoding error", 0, FileList_IPFilter.at(FileIndex).FileName.c_str(), 0);
				}break;
				case READ_TEXT_TYPE::PARAMETER_NORMAL: //ReadParameter
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::PARAMETER, L"Text encoding error", 0, FileList_Config.at(FileIndex).FileName.c_str(), 0);
				}break;
				case READ_TEXT_TYPE::PARAMETER_MONITOR: //ReadParameter(Monitor mode)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::PARAMETER, L"Text encoding error", 0, FileList_Config.at(FileIndex).FileName.c_str(), 0);
				}break;
			#if defined(ENABLE_LIBSODIUM)
				case READ_TEXT_TYPE::DNSCURVE_DATABASE: //ReadDNSCurveDatabase
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::DNSCURVE, L"Text encoding error", 0, FileList_DNSCurveDatabase.at(FileIndex).FileName.c_str(), 0);
				}break;
				case READ_TEXT_TYPE::DNSCURVE_MONITOR: //ReadDNSCurveDatabase(Monitor mode)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::DNSCURVE, L"Text encoding error", 0, FileList_DNSCurveDatabase.at(FileIndex).FileName.c_str(), 0);
				}break;
			#endif
			}

			return false;
		}

	//Delete all null characters.
		for (Index = 0;Index < ReadLength;++Index)
		{
			if (FileBuffer.get()[Index] > 0)
			{
				TextBuffer.get()[strnlen_s(reinterpret_cast<const char *>(TextBuffer.get()), FILE_BUFFER_SIZE)] = FileBuffer.get()[Index];

			//Mark next line format.
				if (!NewLinePoint && FileBuffer.get()[Index] == ASCII_LF)
					NewLinePoint = true;
			}
		}

		memset(FileBuffer.get(), 0, FILE_BUFFER_SIZE + PADDING_RESERVED_BYTES);

	//Line length check
		if (!NewLinePoint && ReadLength == FILE_BUFFER_SIZE)
		{
			switch (InputType)
			{
				case READ_TEXT_TYPE::HOSTS: //ReadHosts
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::HOSTS, L"Data of a line is too long", 0, FileList_Hosts.at(FileIndex).FileName.c_str(), Line);
				}break;
				case READ_TEXT_TYPE::IPFILTER: //ReadIPFilter
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::IPFILTER, L"Data of a line is too long", 0, FileList_IPFilter.at(FileIndex).FileName.c_str(), Line);
				}break;
				case READ_TEXT_TYPE::PARAMETER_NORMAL: //ReadParameter
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::PARAMETER, L"Data of a line is too long", 0, FileList_Config.at(FileIndex).FileName.c_str(), Line);
				}break;
				case READ_TEXT_TYPE::PARAMETER_MONITOR: //ReadParameter(Monitor mode)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::PARAMETER, L"Data of a line is too long", 0, FileList_Config.at(FileIndex).FileName.c_str(), Line);
				}break;
			#if defined(ENABLE_LIBSODIUM)
				case READ_TEXT_TYPE::DNSCURVE_DATABASE: //ReadDNSCurveDatabase
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::DNSCURVE, L"Data of a line is too long", 0, FileList_DNSCurveDatabase.at(FileIndex).FileName.c_str(), Line);
				}break;
				case READ_TEXT_TYPE::DNSCURVE_MONITOR: //ReadDNSCurveDatabase(Monitor mode)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::DNSCURVE, L"Data of a line is too long", 0, FileList_DNSCurveDatabase.at(FileIndex).FileName.c_str(), Line);
				}break;
			#endif
			}

			return false;
		}
		else {
			NewLinePoint = false;
		}

	//Read data.
		for (Index = 0;Index < strnlen_s(reinterpret_cast<const char *>(TextBuffer.get()), FILE_BUFFER_SIZE);++Index)
		{
		//New line
			if (TextBuffer.get()[Index] == ASCII_LF || (Index + 1U == strnlen_s(reinterpret_cast<const char *>(TextBuffer.get()), FILE_BUFFER_SIZE) && feof(const_cast<FILE *>(FileHandle))))
			{
				++Line;

			//Add the last character.
				if (TextBuffer.get()[Index] != ASCII_LF && Index + 1U == strnlen_s(reinterpret_cast<const char *>(TextBuffer.get()), FILE_BUFFER_SIZE) && feof(const_cast<FILE *>(FileHandle)))
					TextData.append(1U, TextBuffer.get()[Index]);

			//Read texts.
				if (TextData.length() > READ_TEXT_MINSIZE)
				{
					switch (InputType)
					{
						case READ_TEXT_TYPE::HOSTS: //ReadHosts
						{
							ReadHostsData(TextData, FileIndex, Line, LabelType_Hosts, IsStopLabel);
						}break;
						case READ_TEXT_TYPE::IPFILTER: //ReadIPFilter
						{
							ReadIPFilterData(TextData, FileIndex, Line, LabelType_IPFilter, IsStopLabel);
						}break;
						case READ_TEXT_TYPE::PARAMETER_NORMAL: //ReadParameter
						{
							if (!ReadParameterData(TextData, FileIndex, true, Line))
								return false;
						}break;
						case READ_TEXT_TYPE::PARAMETER_MONITOR: //ReadParameter(Monitor mode)
						{
							if (!ReadParameterData(TextData, FileIndex, false, Line))
								return false;
						}break;
					#if defined(ENABLE_LIBSODIUM)
						case READ_TEXT_TYPE::DNSCURVE_DATABASE: //ReadDNSCurveDatabase
						{
							ReadDNSCurveDatabaseData(TextData, READ_TEXT_TYPE::DNSCURVE_DATABASE, FileIndex, Line);
						}break;
						case READ_TEXT_TYPE::DNSCURVE_MONITOR: //ReadDNSCurveDatabase(Monitor mode)
						{
							ReadDNSCurveDatabaseData(TextData, READ_TEXT_TYPE::DNSCURVE_MONITOR, FileIndex, Line);
						}break;
					#endif
					}
				}

			//Next step
				if (Index + 1U == strnlen_s(reinterpret_cast<const char *>(TextBuffer.get()), FILE_BUFFER_SIZE) && feof(const_cast<FILE *>(FileHandle)))
					return true;
				else 
					TextData.clear();
			}
			else {
				TextData.append(1U, TextBuffer.get()[Index]);
			}
		}

		memset(TextBuffer.get(), 0, FILE_BUFFER_SIZE + PADDING_RESERVED_BYTES);
	}

	return true;
}

//Read parameter from file
bool ReadParameter(
	const bool IsFirstRead)
{
//Initialization(Part 1)
	FILE *FileHandle = nullptr;
	size_t FileIndex = 0;

//First reading initialization
	if (IsFirstRead)
	{
		FILE_DATA FileDataTemp;

	//Create file list.
		const wchar_t *WCS_ConfigFileNameList[] = CONFIG_FILE_NAME_LIST;
	#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		const char *MBS_ConfigFileNameList[] = CONFIG_FILE_NAME_LIST_MBS;
	#endif
		for (FileIndex = 0;FileIndex < sizeof(WCS_ConfigFileNameList) / sizeof(wchar_t *);++FileIndex)
		{
			FileDataTemp.FileName = GlobalRunningStatus.Path_Global->front();
			FileDataTemp.FileName.append(WCS_ConfigFileNameList[FileIndex]);
		#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			FileDataTemp.MBS_FileName = GlobalRunningStatus.MBS_Path_Global->front();
			FileDataTemp.MBS_FileName.append(MBS_ConfigFileNameList[FileIndex]);
		#endif
			FileDataTemp.ModificationTime = 0;

			FileList_Config.push_back(FileDataTemp);
		}

	//Open configuration file.
		for (FileIndex = 0;FileIndex < FileList_Config.size();++FileIndex)
		{
		#if defined(PLATFORM_WIN)
			if (_wfopen_s(&FileHandle, FileList_Config.at(FileIndex).FileName.c_str(), L"rb") != 0 || FileHandle == nullptr)
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			FileHandle = fopen(FileList_Config.at(FileIndex).MBS_FileName.c_str(), ("rb"));
			if (FileHandle == nullptr)
		#endif
			{
				if (FileIndex + 1U == FileList_Config.size())
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::PARAMETER, L"Cannot open any configuration files", 0, nullptr, 0);
					return false;
				}
			}
			else {
				fclose(FileHandle);
				FileHandle = nullptr;

				break;
			}
		}

	//Mark the only one of configuration file.
		FileDataTemp = FileList_Config.at(FileIndex);
		FileList_Config.clear();
		FileList_Config.shrink_to_fit();
		FileList_Config.push_back(FileDataTemp);
	}

//Initialization(Part 2)
	auto IsConfigFileModified = false, IsFirstMonitorRead = true;
	auto ReadConfigTextType = READ_TEXT_TYPE::PARAMETER_NORMAL;
#if defined(ENABLE_LIBSODIUM)
	auto IsDNSCurveFileModified = false;
	auto ReadDNSCurveTextType = READ_TEXT_TYPE::DNSCURVE_DATABASE;
#endif
	if (!IsFirstRead)
	{
		ReadConfigTextType = READ_TEXT_TYPE::PARAMETER_MONITOR;
	#if defined(ENABLE_LIBSODIUM)
		ReadDNSCurveTextType = READ_TEXT_TYPE::DNSCURVE_MONITOR;
	#endif
	}

//File Monitor
	for (;;)
	{
	//Jump here to restart.
	#if defined(ENABLE_LIBSODIUM)
		JumpToRestart:
	#endif

	//Check configuration file list.
		for (FileIndex = 0;FileIndex < FileList_Config.size();++FileIndex)
		{
		//Get attributes of file.
			if (!ReadFileAttributesLoop(ReadConfigTextType, FileIndex, FileList_Config.at(FileIndex), IsConfigFileModified))
			{
				if (IsFirstRead)
					return false;
				else if (!IsConfigFileModified)
					continue;
			}
			else {
				IsConfigFileModified = true;
			}

		//Read configuration file.
		#if defined(PLATFORM_WIN)
			if (_wfopen_s(&FileHandle, FileList_Config.at(FileIndex).FileName.c_str(), L"rb") != 0 || FileHandle == nullptr)
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			FileHandle = fopen(FileList_Config.at(FileIndex).MBS_FileName.c_str(), ("rb"));
			if (FileHandle == nullptr)
		#endif
			{
				if (IsFirstRead)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::PARAMETER, L"Cannot open any configuration files", 0, nullptr, 0);
					return false;
				}
				else {
					FileList_Config.at(FileIndex).ModificationTime = 0;
//					continue;
				}
			}
			else {
				if (!ReadText(FileHandle, ReadConfigTextType, FileIndex))
				{
					fclose(FileHandle);
					FileHandle = nullptr;

				//Stop reading.
					if (IsFirstRead)
						return false;
//					else 
//						continue;
				}
				else {
					fclose(FileHandle);
					FileHandle = nullptr;
				}
			}
		}

	//Check DNSCurve database file list.
	#if defined(ENABLE_LIBSODIUM)
		if (IsFirstRead)
		{
			FILE_DATA FileDataTemp;

		//Create file list.
			if (Parameter.IsDNSCurve && !DNSCurveParameter.DatabaseName->empty() && 
				(!DNSCurveParameter.Database_Target_Server_Main_IPv6->empty() || !DNSCurveParameter.Database_Target_Server_Alternate_IPv6->empty() || 
				!DNSCurveParameter.Database_Target_Server_Main_IPv4->empty() || !DNSCurveParameter.Database_Target_Server_Alternate_IPv4->empty()))
			{
				for (FileIndex = 0;FileIndex < GlobalRunningStatus.Path_Global->size();++FileIndex)
				{
					FileDataTemp.FileName = GlobalRunningStatus.Path_Global->at(FileIndex);
					FileDataTemp.FileName.append(*DNSCurveParameter.DatabaseName);
				#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
					FileDataTemp.MBS_FileName = GlobalRunningStatus.MBS_Path_Global->at(FileIndex);
					FileDataTemp.MBS_FileName.append(*DNSCurveParameter.MBS_DatabaseName);
				#endif
					FileDataTemp.ModificationTime = 0;

					FileList_DNSCurveDatabase.push_back(FileDataTemp);
				}
			}
			else {
				delete DNSCurveParameter.DatabaseName;
				DNSCurveParameter.DatabaseName = nullptr;
			#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				delete DNSCurveParameter.MBS_DatabaseName;
				DNSCurveParameter.MBS_DatabaseName = nullptr;
			#endif
				delete DNSCurveParameter.Database_Target_Server_Main_IPv6;
				delete DNSCurveParameter.Database_Target_Server_Alternate_IPv6;
				delete DNSCurveParameter.Database_Target_Server_Main_IPv4;
				delete DNSCurveParameter.Database_Target_Server_Alternate_IPv4;
				delete DNSCurveParameter.Database_LineData;
				DNSCurveParameter.Database_Target_Server_Main_IPv6 = nullptr;
				DNSCurveParameter.Database_Target_Server_Alternate_IPv6 = nullptr;
				DNSCurveParameter.Database_Target_Server_Main_IPv4 = nullptr;
				DNSCurveParameter.Database_Target_Server_Alternate_IPv4 = nullptr;
				DNSCurveParameter.Database_LineData = nullptr;
			}
		}

	//Check DNSCurve database file list.
		if (DNSCurveParameter.DatabaseName != nullptr)
		{
			for (FileIndex = 0;FileIndex < FileList_DNSCurveDatabase.size();++FileIndex)
			{
			//Get attributes of file.
				if (!ReadFileAttributesLoop(ReadDNSCurveTextType, FileIndex, FileList_DNSCurveDatabase.at(FileIndex), IsDNSCurveFileModified) && !IsConfigFileModified)
				{
					if (!IsDNSCurveFileModified)
						continue;
				}
				else {
					IsDNSCurveFileModified = true;
				}

			//Force to read configuration file again when DNSCurve database has been changed.
				if (!IsConfigFileModified)
				{
					IsConfigFileModified = true;
					goto JumpToRestart;
				}

			//Read DNSCurve database file.
			#if defined(PLATFORM_WIN)
				if (_wfopen_s(&FileHandle, FileList_DNSCurveDatabase.at(FileIndex).FileName.c_str(), L"rb") != 0 || FileHandle == nullptr)
			#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				FileHandle = fopen(FileList_DNSCurveDatabase.at(FileIndex).MBS_FileName.c_str(), ("rb"));
				if (FileHandle == nullptr)
			#endif
				{
					if (!IsFirstRead)
						FileList_DNSCurveDatabase.at(FileIndex).ModificationTime = 0;

//					continue;
				}
				else {
					if (!ReadText(FileHandle, ReadDNSCurveTextType, FileIndex))
					{
						fclose(FileHandle);
						FileHandle = nullptr;

//						continue;
					}
					else {
						fclose(FileHandle);
						FileHandle = nullptr;
					}
				}
			}

		//Read data from list.
			if (IsDNSCurveFileModified && !ReadDNSCurveDatabaseItem(ReadDNSCurveTextType))
			{
				if (IsFirstRead)
					return false;
				else 
					continue;
			}
			else {
				DNSCurveParameter.Database_LineData->clear();
			}
		}
	#endif

	//The first reading
		if (IsFirstRead)
		{
			GlobalRunningStatus.ConfigFileModifiedTime = GetCurrentSystemTime();
			return Parameter_CheckSetting(IsFirstRead, 0);
		}
	//Jump over the first reading in monitor mode.
		else if (!IsFirstMonitorRead)
		{
			if (IsConfigFileModified)
			{
			//Check parameter list and set default values.
				if (Parameter_CheckSetting(IsFirstRead, 0))
				{
					ParameterModificating.MonitorItemToUsing(&Parameter);
				#if defined(ENABLE_LIBSODIUM)
					if (Parameter.IsDNSCurve)
						DNSCurveParameterModificating.MonitorItemToUsing(&DNSCurveParameter);
				#endif
				}

			//Reset modificating list.
				ParameterModificating.MonitorItemReset();
			#if defined(ENABLE_LIBSODIUM)
				if (Parameter.IsDNSCurve)
					DNSCurveParameterModificating.MonitorItemReset();
			#endif

			//Mark configuration file modified time and flush DNS cache.
				Flush_DNS_Cache(nullptr);
				GlobalRunningStatus.ConfigFileModifiedTime = GetCurrentSystemTime();
			}
		}
	//Set first reading flag.
		else {
			IsFirstMonitorRead = false;
		}

	//Auto-refresh
		IsConfigFileModified = false;
	#if defined(ENABLE_LIBSODIUM)
		IsDNSCurveFileModified = false;
	#endif
		Sleep(Parameter.FileRefreshTime);
	}

//Monitor terminated
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Read Parameter module Monitor terminated", 0, nullptr, 0);
	return false;
}

//Read IPFilter from file
void ReadIPFilter(
	void)
{
//Create file list.
	size_t FileIndex = 0;
	for (size_t Index = 0;Index < GlobalRunningStatus.Path_Global->size();++Index)
	{
		FILE_DATA FileDataTemp;
		for (FileIndex = 0;FileIndex < GlobalRunningStatus.FileList_IPFilter->size();++FileIndex)
		{
			FileDataTemp.FileName = GlobalRunningStatus.Path_Global->at(Index);
			FileDataTemp.FileName.append(GlobalRunningStatus.FileList_IPFilter->at(FileIndex));
		#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			FileDataTemp.MBS_FileName = GlobalRunningStatus.MBS_Path_Global->at(Index);
			FileDataTemp.MBS_FileName.append(GlobalRunningStatus.MBS_FileList_IPFilter->at(FileIndex));
		#endif
			FileDataTemp.ModificationTime = 0;

			FileList_IPFilter.push_back(FileDataTemp);
		}
	}

//Initialization
	FILE *FileHandle = nullptr;
	auto IsFileModified = false;
	std::unique_lock<std::mutex> IPFilterFileMutex(IPFilterFileLock, std::defer_lock);

//File Monitor
	for (;;)
	{
	//Reset parameters.
		IsFileModified = false;

	//Check file list.
		for (FileIndex = 0;FileIndex < FileList_IPFilter.size();++FileIndex)
		{
		//Get attributes of file.
			if (!ReadFileAttributesLoop(READ_TEXT_TYPE::IPFILTER, FileIndex, FileList_IPFilter.at(FileIndex), IsFileModified))
				continue;

		//Clear list data.
			ClearModificatingListData(READ_TEXT_TYPE::IPFILTER, FileIndex);
			IsFileModified = true;

		//Open file handle.
		#if defined(PLATFORM_WIN)
			if (_wfopen_s(&FileHandle, FileList_IPFilter.at(FileIndex).FileName.c_str(), L"rb") != 0 || FileHandle == nullptr)
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			FileHandle = fopen(FileList_IPFilter.at(FileIndex).MBS_FileName.c_str(), ("rb"));
			if (FileHandle == nullptr)
		#endif
				continue;

		//Scan global list.
			DIFFERNET_FILE_SET_IPFILTER IPFilterFileSetTemp;
			if (IPFilterFileSetModificating->empty())
			{
				IPFilterFileSetTemp.FileIndex = FileIndex;
				IPFilterFileSetModificating->push_back(IPFilterFileSetTemp);
			}
			else {
				for (auto IPFilterFileSetIter = IPFilterFileSetModificating->begin();IPFilterFileSetIter != IPFilterFileSetModificating->end();++IPFilterFileSetIter)
				{
					if (IPFilterFileSetIter->FileIndex == FileIndex)
					{
						break;
					}
					else if (IPFilterFileSetIter + 1U == IPFilterFileSetModificating->end())
					{
						IPFilterFileSetTemp.FileIndex = FileIndex;
						IPFilterFileSetModificating->push_back(IPFilterFileSetTemp);
						break;
					}
				}
			}

		//Read data.
			ReadText(FileHandle, READ_TEXT_TYPE::IPFILTER, FileIndex);
			fclose(FileHandle);
			FileHandle = nullptr;
		}

	//Mark to global list.
		if (!IsFileModified)
		{
			Sleep(Parameter.FileRefreshTime);
			continue;
		}

	//Copy to using list.
		std::sort(IPFilterFileSetModificating->begin(), IPFilterFileSetModificating->end(), SortCompare_IPFilter);
		IPFilterFileMutex.lock();
		IPFilterFileSetUsing->clear();
		IPFilterFileSetUsing->shrink_to_fit();
		*IPFilterFileSetUsing = *IPFilterFileSetModificating;
		IPFilterFileMutex.unlock();
		IPFilterFileSetModificating->shrink_to_fit();

	//Flush DNS cache and auto-refresh.
		Flush_DNS_Cache(nullptr);
		Sleep(Parameter.FileRefreshTime);
	}

//Monitor terminated
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Read IPFilter module Monitor terminated", 0, nullptr, 0);
	return;
}

//Read hosts from file
void ReadHosts(
	void)
{
//Create file list.
	size_t FileIndex = 0;
	for (size_t Index = 0;Index < GlobalRunningStatus.Path_Global->size();++Index)
	{
		FILE_DATA FileDataTemp;
		for (FileIndex = 0;FileIndex < GlobalRunningStatus.FileList_Hosts->size();++FileIndex)
		{
			FileDataTemp.FileName = GlobalRunningStatus.Path_Global->at(Index);
			FileDataTemp.FileName.append(GlobalRunningStatus.FileList_Hosts->at(FileIndex));
		#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			FileDataTemp.MBS_FileName = GlobalRunningStatus.MBS_Path_Global->at(Index);
			FileDataTemp.MBS_FileName.append(GlobalRunningStatus.MBS_FileList_Hosts->at(FileIndex));
		#endif
			FileDataTemp.ModificationTime = 0;

			FileList_Hosts.push_back(FileDataTemp);
		}
	}

//Initialization
	FILE *FileHandle = nullptr;
	auto IsFileModified = false;
	std::unique_lock<std::mutex> HostsFileMutex(HostsFileLock, std::defer_lock);

//File Monitor
	for (;;)
	{
	//Reset parameters.
		IsFileModified = false;

	//Check file list.
		for (FileIndex = 0;FileIndex < FileList_Hosts.size();++FileIndex)
		{
		//Get attributes of file.
			if (!ReadFileAttributesLoop(READ_TEXT_TYPE::HOSTS, FileIndex, FileList_Hosts.at(FileIndex), IsFileModified))
				continue;

		//clear list data.
			ClearModificatingListData(READ_TEXT_TYPE::HOSTS, FileIndex);
			IsFileModified = true;

		//Open file handle.
		#if defined(PLATFORM_WIN)
			if (_wfopen_s(&FileHandle, FileList_Hosts.at(FileIndex).FileName.c_str(), L"rb") != 0 || FileHandle == nullptr)
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			FileHandle = fopen(FileList_Hosts.at(FileIndex).MBS_FileName.c_str(), ("rb"));
			if (FileHandle == nullptr)
		#endif
				continue;

		//Scan global list.
			DIFFERNET_FILE_SET_HOSTS HostsFileSetTemp;
			if (HostsFileSetModificating->empty())
			{
				HostsFileSetTemp.FileIndex = FileIndex;
				HostsFileSetModificating->push_back(HostsFileSetTemp);
			}
			else {
				for (auto HostsFileSetIter = HostsFileSetModificating->begin();HostsFileSetIter != HostsFileSetModificating->end();++HostsFileSetIter)
				{
					if (HostsFileSetIter->FileIndex == FileIndex)
					{
						break;
					}
					else if (HostsFileSetIter + 1U == HostsFileSetModificating->end())
					{
						HostsFileSetTemp.FileIndex = FileIndex;
						HostsFileSetModificating->push_back(HostsFileSetTemp);
						break;
					}
				}
			}

		//Read data.
			ReadText(FileHandle, READ_TEXT_TYPE::HOSTS, FileIndex);
			fclose(FileHandle);
			FileHandle = nullptr;
		}

	//Interval time to next loop
		if (!IsFileModified)
		{
			Sleep(Parameter.FileRefreshTime);
			continue;
		}

	//Copy to using list.
		std::sort(HostsFileSetModificating->begin(), HostsFileSetModificating->end(), SortCompare_Hosts);
		HostsFileMutex.lock();
		HostsFileSetUsing->clear();
		HostsFileSetUsing->shrink_to_fit();
		*HostsFileSetUsing = *HostsFileSetModificating;
		HostsFileMutex.unlock();
		HostsFileSetModificating->shrink_to_fit();

	//Flush DNS cache and auto-refresh.
		Flush_DNS_Cache(nullptr);
		Sleep(Parameter.FileRefreshTime);
	}

//Monitor terminated
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Read Hosts module Monitor terminated", 0, nullptr, 0);
	return;
}

//Loop of file attributes reading
bool ReadFileAttributesLoop(
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex, 
	FILE_DATA &FileListIter, 
	bool &IsFileModified)
{
//Initialization
#if defined(PLATFORM_WIN)
	WIN32_FILE_ATTRIBUTE_DATA FileAttributeData;
	LARGE_INTEGER FileSizeData;
	memset(&FileAttributeData, 0, sizeof(FileAttributeData));
	memset(&FileSizeData, 0, sizeof(FileSizeData));
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	struct stat FileStatData;
	memset(&FileStatData, 0, sizeof(FileStatData));
#endif

//Get attributes of file.
#if defined(PLATFORM_WIN)
	if (GetFileAttributesExW(
			FileListIter.FileName.c_str(), 
			GetFileExInfoStandard, 
			&FileAttributeData) == FALSE)
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	if (stat(FileListIter.MBS_FileName.c_str(), &FileStatData) != 0)
#endif
	{
	//Clear list data.
		if (InputType == READ_TEXT_TYPE::PARAMETER_NORMAL)
			PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::PARAMETER, L"Cannot open any configuration files", 0, nullptr, 0);
		else if (InputType == READ_TEXT_TYPE::HOSTS || InputType == READ_TEXT_TYPE::IPFILTER)
			ClearModificatingListData(InputType, FileIndex);

	//Reset patameters.
		if (FileListIter.ModificationTime > 0)
		{
			IsFileModified = true;
			FileListIter.ModificationTime = 0;
		}

		return false;
	}

//Check whole file size.
#if defined(PLATFORM_WIN)
	FileSizeData.HighPart = FileAttributeData.nFileSizeHigh;
	FileSizeData.LowPart = FileAttributeData.nFileSizeLow;
	if (FileSizeData.QuadPart < 0 || static_cast<uint64_t>(FileSizeData.QuadPart) >= FILE_READING_MAXSIZE)
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	if (FileStatData.st_size >= static_cast<off_t>(FILE_READING_MAXSIZE))
#endif
	{
	//Clear list data.
		if (InputType == READ_TEXT_TYPE::HOSTS || InputType == READ_TEXT_TYPE::IPFILTER)
			ClearModificatingListData(InputType, FileIndex);

	//Print error messages.
		if (InputType == READ_TEXT_TYPE::PARAMETER_NORMAL || InputType == READ_TEXT_TYPE::PARAMETER_MONITOR)
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::PARAMETER, L"Configuration file size is too large", 0, FileListIter.FileName.c_str(), 0);
	#if defined(ENABLE_LIBSODIUM)
		else if (InputType == READ_TEXT_TYPE::DNSCURVE_DATABASE || InputType == READ_TEXT_TYPE::DNSCURVE_MONITOR)
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::PARAMETER, L"DNSCurve database file size is too large", 0, FileListIter.FileName.c_str(), 0);
	#endif
		else if (InputType == READ_TEXT_TYPE::HOSTS)
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::PARAMETER, L"Hosts file size is too large", 0, FileListIter.FileName.c_str(), 0);
		else if (InputType == READ_TEXT_TYPE::IPFILTER)
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::PARAMETER, L"IPFilter file size is too large", 0, FileListIter.FileName.c_str(), 0);
		else 
			return false;

	//Reset parameters.
		if (FileListIter.ModificationTime > 0)
		{
			IsFileModified = true;
			FileListIter.ModificationTime = 0;
		}

		return false;
	}

//Check modification time of file.
#if defined(PLATFORM_WIN)
	memset(&FileSizeData, 0, sizeof(FileSizeData));
	FileSizeData.HighPart = FileAttributeData.ftLastWriteTime.dwHighDateTime;
	FileSizeData.LowPart = FileAttributeData.ftLastWriteTime.dwLowDateTime;
	if (FileListIter.ModificationTime > 0 && FileSizeData.QuadPart == FileListIter.ModificationTime)
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	if (FileListIter.ModificationTime > 0 && FileStatData.st_mtime == FileListIter.ModificationTime)
#endif
	{
		if (InputType == READ_TEXT_TYPE::PARAMETER_NORMAL)
			PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::PARAMETER, L"Cannot open any configuration files", 0, nullptr, 0);

		return false;
	}
//Mark modification time.
	else {
	#if defined(PLATFORM_WIN)
		FileListIter.ModificationTime = FileSizeData.QuadPart;
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		FileListIter.ModificationTime = FileStatData.st_mtime;
	#endif
	}

	return true;
}

//Clear data in list
void ClearModificatingListData(
	const READ_TEXT_TYPE ClearType, 
	const size_t FileIndex)
{
//Clear Hosts file set.
	if (ClearType == READ_TEXT_TYPE::HOSTS)
	{
		for (auto HostsFileSetIter = HostsFileSetModificating->begin();HostsFileSetIter != HostsFileSetModificating->end();++HostsFileSetIter)
		{
			if (HostsFileSetIter->FileIndex == FileIndex)
			{
				HostsFileSetModificating->erase(HostsFileSetIter);
				break;
			}
		}
	}
//Clear IPFilter file set.
	else if (ClearType == READ_TEXT_TYPE::IPFILTER)
	{
		for (auto IPFilterFileSetIter = IPFilterFileSetModificating->begin();IPFilterFileSetIter != IPFilterFileSetModificating->end();++IPFilterFileSetIter)
		{
			if (IPFilterFileSetIter->FileIndex == FileIndex)
			{
				IPFilterFileSetModificating->erase(IPFilterFileSetIter);
				break;
			}
		}
	}

	return;
}

//Get data list from file
void GetParameterListData(
	std::vector<std::string> &ListData, 
	const std::string &Data, 
	const size_t DataOffset, 
	const size_t Length, 
	const uint8_t SeparatedSign, 
	const bool IsCaseConvert, 
	const bool IsKeepEmptyItem)
{
//Initialization
	std::string NameString;
	ListData.clear();

//Get all list data.
	for (auto Index = DataOffset;Index < Length;++Index)
	{
	//Last data
		if (Index + 1U == Length)
		{
			if (Data.at(Index) != SeparatedSign)
				NameString.append(Data, Index, 1U);
			if (NameString.empty())
			{
				if (IsKeepEmptyItem)
					ListData.push_back(NameString);

				break;
			}
			else {
				if (IsCaseConvert)
					CaseConvert(NameString, false);

			//Add char to end.
				ListData.push_back(NameString);
				if (IsKeepEmptyItem && Data.at(Index) == SeparatedSign)
				{
					NameString.clear();
					ListData.push_back(NameString);
				}

				break;
			}
		}
	//Separated
		else if (Data.at(Index) == SeparatedSign)
		{
			if (!NameString.empty())
			{
				if (IsCaseConvert)
					CaseConvert(NameString, false);

			//Add char to end.
				ListData.push_back(NameString);
				NameString.clear();
			}
			else if (IsKeepEmptyItem)
			{
				ListData.push_back(NameString);
				NameString.clear();
			}
		}
		else {
			NameString.append(Data, Index, 1U);
		}
	}

	return;
}
