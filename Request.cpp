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


#include "Request.h"

#if defined(ENABLE_PCAP)
//Get Hop Limits(IPv6) and TTL(IPv4) with normal DNS request
bool DomainTestRequest(
	const uint16_t Protocol)
{
//Initialization
	const auto SendBuffer = std::make_unique<uint8_t[]>(NORMAL_PACKET_MAXSIZE + PADDING_RESERVED_BYTES);
	const auto RecvBuffer = std::make_unique<uint8_t[]>(Parameter.LargeBufferSize + PADDING_RESERVED_BYTES);
	memset(SendBuffer.get(), 0, NORMAL_PACKET_MAXSIZE + PADDING_RESERVED_BYTES);
	memset(RecvBuffer.get(), 0, Parameter.LargeBufferSize + PADDING_RESERVED_BYTES);

//Make a DNS request with Doamin Test packet.
	const auto DNS_Header = reinterpret_cast<dns_hdr *>(SendBuffer.get());
	DNS_Header->ID = Parameter.DomainTest_ID;
	DNS_Header->Flags = htons(DNS_FLAG_STANDARD);
	DNS_Header->Question = htons(UINT16_NUM_ONE);
	size_t DataLength = 0;

//Convert domain.
	dns_qry *DNS_Query = nullptr;
	if (Parameter.DomainTest_Data != nullptr)
	{
		DataLength = StringToPacketQuery(Parameter.DomainTest_Data, RecvBuffer.get());
		if (DataLength > DOMAIN_MINSIZE && DataLength + sizeof(dns_hdr) < NORMAL_PACKET_MAXSIZE)
		{
			memcpy_s(SendBuffer.get() + sizeof(dns_hdr), Parameter.LargeBufferSize - sizeof(dns_hdr), RecvBuffer.get(), DataLength);
			memset(RecvBuffer.get(), 0, Parameter.LargeBufferSize + PADDING_RESERVED_BYTES);
			DNS_Query = reinterpret_cast<dns_qry *>(SendBuffer.get() + sizeof(dns_hdr) + DataLength);
			DNS_Query->Classes = htons(DNS_CLASS_INTERNET);
			if (Protocol == AF_INET6)
				DNS_Query->Type = htons(DNS_TYPE_AAAA);
			else if (Protocol == AF_INET)
				DNS_Query->Type = htons(DNS_TYPE_A);
			else 
				return false;

			DataLength += sizeof(dns_qry);

		//EDNS Label
			if (Parameter.EDNS_Label)
			{
				DataLength = Add_EDNS_LabelToPacket(SendBuffer.get(), DataLength + sizeof(dns_hdr), NORMAL_PACKET_MAXSIZE, nullptr);
				DataLength -= sizeof(dns_hdr);
			}
		}
		else {
			return false;
		}
	}

	DataLength += sizeof(dns_hdr);

//Send request.
	size_t TotalSleepTime = 0, Times = 0;
	auto FileModifiedTime = GlobalRunningStatus.ConfigFileModifiedTime;
	for (;;)
	{
	//Domain Test disable
		if (Parameter.DomainTest_Speed == 0)
		{
			Sleep(Parameter.FileRefreshTime);
			continue;
		}
	//Sleep time controller
		else if (TotalSleepTime > 0)
		{
		//Configuration files have been changed.
			if (FileModifiedTime != GlobalRunningStatus.ConfigFileModifiedTime)
			{
				FileModifiedTime = GlobalRunningStatus.ConfigFileModifiedTime;
				TotalSleepTime = 0;
			}
		//Interval time is not enough.
			else if (TotalSleepTime < Parameter.DomainTest_Speed)
			{
				TotalSleepTime += Parameter.FileRefreshTime;

				Sleep(Parameter.FileRefreshTime);
				continue;
			}
		//Interval time is enough, next recheck time.
			else {
				TotalSleepTime = 0;
			}
		}

	//Interval time
		if (Times == SENDING_ONCE_INTERVAL_TIMES)
		{
			Times = 0;

		//Test again check.
			if (Protocol == AF_INET6)
			{
				if (
				//Main
					(Parameter.Target_Server_Main_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Assign == 0 && 
					Parameter.Target_Server_Main_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Mark == 0) || 
					!Parameter.Target_Server_Main_IPv6.ServerPacketStatus.IsMarkSign || 
				//Alternate
					(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
					((Parameter.Target_Server_Alternate_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Assign == 0 && 
					Parameter.Target_Server_Alternate_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Mark == 0) || 
					!Parameter.Target_Server_Alternate_IPv6.ServerPacketStatus.IsMarkSign)))
						goto JumpToRetest;

			//Multiple list(IPv6)
				if (Parameter.Target_Server_IPv6_Multiple != nullptr)
				{
					for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv6_Multiple)
					{
						if ((DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Assign == 0 && 
							DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Mark == 0) || 
							!DNSServerDataIter.ServerPacketStatus.IsMarkSign)
								goto JumpToRetest;
					}
				}
			}
			else if (Protocol == AF_INET)
			{
				if (
				//Main
					(Parameter.Target_Server_Main_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Assign == 0 && 
					Parameter.Target_Server_Main_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Mark == 0) || 
					!Parameter.Target_Server_Main_IPv4.ServerPacketStatus.IsMarkSign || 
				//Alternate
					(Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
					((Parameter.Target_Server_Alternate_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Assign == 0 && 
					Parameter.Target_Server_Alternate_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Mark == 0) || 
					!Parameter.Target_Server_Alternate_IPv4.ServerPacketStatus.IsMarkSign)))
						goto JumpToRetest;

			//Multiple list(IPv4)
				if (Parameter.Target_Server_IPv4_Multiple != nullptr)
				{
					for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv4_Multiple)
					{
						if ((DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Assign == 0 && 
							DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Mark == 0) || 
							!DNSServerDataIter.ServerPacketStatus.IsMarkSign)
								goto JumpToRetest;
					}
				}
			}
			else {
				goto JumpToRetest;
			}

		//Wait for testing again.
			TotalSleepTime += Parameter.FileRefreshTime;
			continue;

		//Jump here to restart.
		JumpToRetest:
			Sleep(SENDING_INTERVAL_TIME);
		}
		else {
		//Make random domain request.
			if (Parameter.DomainTest_Data == nullptr)
			{
				memset(SendBuffer.get() + sizeof(dns_hdr), 0, NORMAL_PACKET_MAXSIZE - sizeof(dns_hdr));
				memset(RecvBuffer.get(), 0, Parameter.LargeBufferSize + PADDING_RESERVED_BYTES);
				MakeRandomDomain(RecvBuffer.get());
				DataLength = StringToPacketQuery(RecvBuffer.get(), SendBuffer.get() + sizeof(dns_hdr)) + sizeof(dns_hdr);

			//Make DNS query data.
				DNS_Query = reinterpret_cast<dns_qry *>(SendBuffer.get() + DataLength);
				DNS_Query->Classes = htons(DNS_CLASS_INTERNET);
				if (Protocol == AF_INET6)
					DNS_Query->Type = htons(DNS_TYPE_AAAA);
				else if (Protocol == AF_INET)
					DNS_Query->Type = htons(DNS_TYPE_A);
				else 
					break;
				DataLength += sizeof(dns_qry);

			//EDNS Label
				if (Parameter.EDNS_Label)
				{
					DNS_Header->Additional = 0;
					DataLength = Add_EDNS_LabelToPacket(SendBuffer.get(), DataLength, NORMAL_PACKET_MAXSIZE, nullptr);
				}
			}

		//Send process, both TCP and UDP protocol
			memset(RecvBuffer.get(), 0, Parameter.LargeBufferSize + PADDING_RESERVED_BYTES);
			if (Parameter.DomainTest_Protocol == REQUEST_MODE_TEST::BOTH)
			{
				UDP_RequestMultiple(REQUEST_PROCESS_TYPE::UDP_WITHOUT_MARKING, 0, SendBuffer.get(), DataLength, nullptr);
				TCP_RequestMultiple(REQUEST_PROCESS_TYPE::TCP_WITHOUT_MARKING, SendBuffer.get(), DataLength, RecvBuffer.get(), Parameter.LargeBufferSize, nullptr);
			}
			else if (Parameter.DomainTest_Protocol == REQUEST_MODE_TEST::TCP)
			{
				TCP_RequestMultiple(REQUEST_PROCESS_TYPE::TCP_WITHOUT_MARKING, SendBuffer.get(), DataLength, RecvBuffer.get(), Parameter.LargeBufferSize, nullptr);
			}
			else if (Parameter.DomainTest_Protocol == REQUEST_MODE_TEST::UDP)
			{
				UDP_RequestMultiple(REQUEST_PROCESS_TYPE::UDP_WITHOUT_MARKING, 0, SendBuffer.get(), DataLength, nullptr);
			}

		//Interval time
			Sleep(SENDING_INTERVAL_TIME);
			++Times;
		}
	}

//Monitor terminated
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Domain Test module Monitor terminated", 0, nullptr, 0);
	return true;
}

//Internet Control Message Protocol/ICMP echo request(Ping)
bool ICMP_TestRequest(
	const uint16_t Protocol)
{
//Initialization
	size_t Length = 0;
	if (Protocol == AF_INET6)
		Length = sizeof(icmpv6_hdr) + Parameter.ICMP_PaddingLength;
	else if (Protocol == AF_INET)
		Length = sizeof(icmp_hdr) + Parameter.ICMP_PaddingLength;
	else 
		return false;
	const auto SendBuffer = std::make_unique<uint8_t[]>(Length + PADDING_RESERVED_BYTES);
	memset(SendBuffer.get(), 0, Length + PADDING_RESERVED_BYTES);
	const auto ICMP_Header = reinterpret_cast<icmp_hdr *>(SendBuffer.get());
	const auto ICMPv6_Header = reinterpret_cast<icmpv6_hdr *>(SendBuffer.get());
	std::vector<SOCKET_DATA> ICMP_SocketData;
#if defined(PLATFORM_LINUX)
#if !defined(ENABLE_LIBSODIUM)
	std::uniform_int_distribution<uint32_t> RandomDistribution(0, UINT32_MAX);
#endif
#endif
	time_t Timestamp = 0;

//ICMPv6
	if (Protocol == AF_INET6)
	{
		Timestamp = time(nullptr);
		if (Timestamp <= 0)
			return false;

	//Make a ICMPv6 request echo packet.
	//ICMPv6 protocol checksum will always be calculated by network stack in all platforms.
		ICMPv6_Header->Type = ICMPV6_TYPE_REQUEST;
		ICMPv6_Header->Code = ICMPV6_CODE_REQUEST;
		ICMPv6_Header->ID = Parameter.ICMP_ID;
		ICMPv6_Header->Sequence = Parameter.ICMP_Sequence;
		memcpy_s(SendBuffer.get() + sizeof(icmpv6_hdr), Parameter.ICMP_PaddingLength, Parameter.ICMP_PaddingData, Parameter.ICMP_PaddingLength);
	#if defined(PLATFORM_LINUX)
		ICMPv6_Header->Timestamp = static_cast<uint64_t>(Timestamp);
	#if defined(ENABLE_LIBSODIUM)
		ICMPv6_Header->Nonce = randombytes_random();
	#else
		ICMPv6_Header->Nonce = RandomDistribution(*GlobalRunningStatus.RandomEngine);
	#endif
	#elif defined(PLATFORM_MACOS)
		ICMPv6_Header->Timestamp = static_cast<uint64_t>(Timestamp);
	#endif

	//Socket initialization
	//Windows: Use SOCK_RAW type with IPPROTO_ICMPV6.
	//Linux: Use SOCK_RAW type with IPPROTO_ICMPV6, also support SOCK_DGRAM type but default disabled.
	//macOS: Use SOCK_DGRAM type with IPPROTO_ICMPV6.
		SOCKET_DATA SocketDataTemp;
		memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
		SocketDataTemp.Socket = INVALID_SOCKET;
//		int OptionValue = ICMPV6_OFFSET_CHECKSUM;

	//Main
	#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
		SocketDataTemp.Socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	#elif defined(PLATFORM_MACOS)
		SocketDataTemp.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
	#endif
		if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
			!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV6, true, nullptr) // || 
//			!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::CHECKSUM_IPV6, true, &OptionValue) //ICMPv6 protocol checksum will always be calculated by network stack in all platforms.
			)
		{
			SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}
		else {
			SocketDataTemp.SockAddr.ss_family = Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family;
			reinterpret_cast<sockaddr_in6 *>(&SocketDataTemp.SockAddr)->sin6_addr = Parameter.Target_Server_Main_IPv6.AddressData.IPv6.sin6_addr;
			SocketDataTemp.AddrLen = sizeof(sockaddr_in6);
			ICMP_SocketData.push_back(SocketDataTemp);
			memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
			SocketDataTemp.Socket = INVALID_SOCKET;
		}

	//Alternate
		if (Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0)
		{
//			OptionValue = ICMPV6_OFFSET_CHECKSUM;

		#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
			SocketDataTemp.Socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
		#elif defined(PLATFORM_MACOS)
			SocketDataTemp.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
		#endif
			if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
				!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV6, true, nullptr) // || 
//				!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::CHECKSUM_IPV6, true, &OptionValue) //ICMPv6 protocol checksum will always be calculated by network stack in all platforms.
				)
			{
				for (auto &SocketDataIter:ICMP_SocketData)
					SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

				return false;
			}
			else {
				SocketDataTemp.SockAddr.ss_family = Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family;
				reinterpret_cast<sockaddr_in6 *>(&SocketDataTemp.SockAddr)->sin6_addr = Parameter.Target_Server_Alternate_IPv6.AddressData.IPv6.sin6_addr;
				SocketDataTemp.AddrLen = sizeof(sockaddr_in6);
				ICMP_SocketData.push_back(SocketDataTemp);
				memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
				SocketDataTemp.Socket = INVALID_SOCKET;
			}
		}

	//Multiple list(IPv6)
		if (Parameter.Target_Server_IPv6_Multiple != nullptr)
		{
			for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv6_Multiple)
			{
//				OptionValue = ICMPV6_OFFSET_CHECKSUM;

			#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
				SocketDataTemp.Socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
			#elif defined(PLATFORM_MACOS)
				SocketDataTemp.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
			#endif
				if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
					!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV6, true, nullptr) // || 
//					!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::CHECKSUM_IPV6, true, &OptionValue) //ICMPv6 protocol checksum will always be calculated by network stack in all platforms.
					)
				{
					for (auto &SocketDataIter:ICMP_SocketData)
						SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

					return false;
				}
				else {
					SocketDataTemp.SockAddr.ss_family = DNSServerDataIter.AddressData.Storage.ss_family;
					reinterpret_cast<sockaddr_in6 *>(&SocketDataTemp.SockAddr)->sin6_addr = DNSServerDataIter.AddressData.IPv6.sin6_addr;
					SocketDataTemp.AddrLen = sizeof(sockaddr_in6);
					ICMP_SocketData.push_back(SocketDataTemp);
					memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
					SocketDataTemp.Socket = INVALID_SOCKET;
				}
			}
		}
	}
//ICMP
	else if (Protocol == AF_INET)
	{
	//Make a ICMP request echo packet, calculate checksum to make sure that is correct.
	//Windows: It seems that it's not calculating by network stack.
	//Linux: Calculate by network stack.
	//macOS: It seems that it's not calculating by network stack.
		ICMP_Header->Type = ICMP_TYPE_REQUEST;
		ICMP_Header->Code = ICMP_CODE_REQUEST;
		ICMP_Header->ID = Parameter.ICMP_ID;
		ICMP_Header->Sequence = Parameter.ICMP_Sequence;
		memcpy_s(SendBuffer.get() + sizeof(icmp_hdr), Parameter.ICMP_PaddingLength, Parameter.ICMP_PaddingData, Parameter.ICMP_PaddingLength);
	#if defined(PLATFORM_LINUX)
		ICMP_Header->Timestamp = static_cast<uint64_t>(Timestamp);
	#if defined(ENABLE_LIBSODIUM)
		ICMP_Header->Nonce = randombytes_random();
	#else
		ICMP_Header->Nonce = RandomDistribution(*GlobalRunningStatus.RandomEngine);
	#endif
	#elif defined(PLATFORM_MACOS)
		ICMP_Header->Timestamp = static_cast<uint64_t>(Timestamp);
	#endif
		ICMP_Header->Checksum = GetChecksum(reinterpret_cast<uint16_t *>(SendBuffer.get()), Length);

	//Socket initialization
	//Windows: Use SOCK_RAW type with IPPROTO_ICMP.
	//Linux: Use SOCK_RAW type with IPPROTO_ICMP, also support SOCK_DGRAM type but default disabled and need to set <net.ipv4.ping_group_range='0 10'>.
	//macOS: Use SOCK_DGRAM type with IPPROTO_ICMP.
		SOCKET_DATA SocketDataTemp;
		memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
		SocketDataTemp.Socket = INVALID_SOCKET;

	//Main
	#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
		SocketDataTemp.Socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	#elif defined(PLATFORM_MACOS)
		SocketDataTemp.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	#endif
		if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
			!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV4, true, nullptr) || 
			!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::DO_NOT_FRAGMENT, true, nullptr))
		{
			SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}
		else {
			SocketDataTemp.SockAddr.ss_family = Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family;
			reinterpret_cast<sockaddr_in *>(&SocketDataTemp.SockAddr)->sin_addr = Parameter.Target_Server_Main_IPv4.AddressData.IPv4.sin_addr;
			SocketDataTemp.AddrLen = sizeof(sockaddr_in);
			ICMP_SocketData.push_back(SocketDataTemp);
			memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
			SocketDataTemp.Socket = INVALID_SOCKET;
		}

	//Alternate
		if (Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0)
		{
		#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
			SocketDataTemp.Socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		#elif defined(PLATFORM_MACOS)
			SocketDataTemp.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
		#endif
			if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
				!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV4, true, nullptr) || 
				!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::DO_NOT_FRAGMENT, true, nullptr))
			{
				for (auto &SocketDataIter:ICMP_SocketData)
					SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

				return false;
			}
			else {
				SocketDataTemp.SockAddr.ss_family = Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family;
				reinterpret_cast<sockaddr_in *>(&SocketDataTemp.SockAddr)->sin_addr = Parameter.Target_Server_Alternate_IPv4.AddressData.IPv4.sin_addr;
				SocketDataTemp.AddrLen = sizeof(sockaddr_in);
				ICMP_SocketData.push_back(SocketDataTemp);
				memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
				SocketDataTemp.Socket = INVALID_SOCKET;
			}
		}

	//Multiple list(IPv4)
		if (Parameter.Target_Server_IPv4_Multiple != nullptr)
		{
			for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv4_Multiple)
			{
			#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
				SocketDataTemp.Socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
			#elif defined(PLATFORM_MACOS)
				SocketDataTemp.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
			#endif
				if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
					!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV4, true, nullptr) || 
					!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::DO_NOT_FRAGMENT, true, nullptr))
				{
					for (auto &SocketDataIter:ICMP_SocketData)
						SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

					return false;
				}
				else {
					SocketDataTemp.SockAddr.ss_family = DNSServerDataIter.AddressData.Storage.ss_family;
					reinterpret_cast<sockaddr_in *>(&SocketDataTemp.SockAddr)->sin_addr = DNSServerDataIter.AddressData.IPv4.sin_addr;
					SocketDataTemp.AddrLen = sizeof(sockaddr_in);
					ICMP_SocketData.push_back(SocketDataTemp);
					memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
					SocketDataTemp.Socket = INVALID_SOCKET;
				}
			}
		}
	}
	else {
		return false;
	}

//Socket attribute setting(Timeout)
	for (auto &SocketDataIter:ICMP_SocketData)
	{
		if (!SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::TIMEOUT, true, &Parameter.SocketTimeout_Unreliable_Once))
		{
			for (auto &InnerSocketDataIter:ICMP_SocketData)
				SocketSetting(InnerSocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

			return false;
		}
	}

//Send request.
	const auto RecvBuffer = std::make_unique<uint8_t[]>(NORMAL_PACKET_MAXSIZE + PADDING_RESERVED_BYTES);
	SOCKET_DATA InnerSocketData;
	memset(RecvBuffer.get(), 0, NORMAL_PACKET_MAXSIZE + PADDING_RESERVED_BYTES);
	memset(&InnerSocketData, 0, sizeof(InnerSocketData));
	InnerSocketData.Socket = INVALID_SOCKET;
	size_t TotalSleepTime = 0, Times = 0;
	auto FileModifiedTime = GlobalRunningStatus.ConfigFileModifiedTime;
	auto IsAllSend = false;
	for (;;)
	{
	//ICMP Test Disable
		if (Parameter.ICMP_Speed == 0)
		{
			Sleep(Parameter.FileRefreshTime);
			continue;
		}
	//Sleep time controller
		else if (TotalSleepTime > 0)
		{
		//Sleep time controller
			if (FileModifiedTime != GlobalRunningStatus.ConfigFileModifiedTime)
			{
				FileModifiedTime = GlobalRunningStatus.ConfigFileModifiedTime;
				TotalSleepTime = 0;
			}
		//Interval time is not enough.
			else if (TotalSleepTime < Parameter.ICMP_Speed)
			{
				TotalSleepTime += Parameter.FileRefreshTime;
				Sleep(Parameter.FileRefreshTime);

				continue;
			}
		//Interval time is enough, next recheck time.
			else {
				TotalSleepTime = 0;
			}
		}

	//Interval time
		if (Times == SENDING_ONCE_INTERVAL_TIMES)
		{
			Times = 0;

		//Test again check.
			if (Protocol == AF_INET6)
			{
				if (
				//Main
					(Parameter.Target_Server_Main_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Assign == 0 && 
					Parameter.Target_Server_Main_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Mark == 0) || 
				//Alternate
					(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
					Parameter.Target_Server_Alternate_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Assign == 0 && 
					Parameter.Target_Server_Alternate_IPv6.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Mark == 0))
						goto JumpToRetest;

			//Multiple list(IPv6)
				if (Parameter.Target_Server_IPv6_Multiple != nullptr)
				{
					for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv6_Multiple)
					{
						if (DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Assign == 0 && 
							DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv6_HeaderStatus.HopLimit_Mark == 0)
								goto JumpToRetest;
					}
				}
			}
			else if (Protocol == AF_INET)
			{
				if (
				//Main
					(Parameter.Target_Server_Main_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Assign == 0 && 
					Parameter.Target_Server_Main_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Mark == 0) || 
				//Alternate
					(Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
					Parameter.Target_Server_Alternate_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Assign == 0 && 
					Parameter.Target_Server_Alternate_IPv4.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Mark == 0))
						goto JumpToRetest;

			//Multiple list(IPv4)
				if (Parameter.Target_Server_IPv4_Multiple != nullptr)
				{
					for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv4_Multiple)
					{
						if (DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Assign == 0 && DNSServerDataIter.ServerPacketStatus.NetworkLayerStatus.IPv4_HeaderStatus.TTL_Mark == 0)
							goto JumpToRetest;
					}
				}
			}
			else {
				goto JumpToRetest;
			}

		//Wait for testing again.
			TotalSleepTime += Parameter.FileRefreshTime;
			continue;

		//Jump here to restart.
		JumpToRetest:
			Sleep(SENDING_INTERVAL_TIME);
			continue;
		}

	//Send and receive process
		for (auto &SocketDataIter:ICMP_SocketData)
		{
		//Socket check
			if (!SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				continue;
			}

		//Send and receive data.
			IsAllSend = false;
			for (size_t Index = 0;Index < Parameter.MultipleRequestTimes;++Index)
			{
				if (!IsAllSend)
				{
					sendto(SocketDataIter.Socket, reinterpret_cast<const char *>(SendBuffer.get()), static_cast<int>(Length), 0, reinterpret_cast<sockaddr *>(const_cast<sockaddr_storage *>(&SocketDataIter.SockAddr)), SocketDataIter.AddrLen);
					if (Index + 1U == Parameter.MultipleRequestTimes)
					{
						IsAllSend = true;
						Index = 0;
					}
				}
				else {
					memcpy_s(&InnerSocketData, sizeof(InnerSocketData), &SocketDataIter, sizeof(InnerSocketData));
					recvfrom(SocketDataIter.Socket, reinterpret_cast<char *>(RecvBuffer.get()), NORMAL_PACKET_MAXSIZE, 0, reinterpret_cast<sockaddr *>(&InnerSocketData.SockAddr), &InnerSocketData.AddrLen);
					memset(RecvBuffer.get(), 0, NORMAL_PACKET_MAXSIZE + PADDING_RESERVED_BYTES);
					memset(&InnerSocketData, 0, sizeof(InnerSocketData));
					InnerSocketData.Socket = INVALID_SOCKET;
				}
			}

		//Increase sequence.
			if (ntohs(Parameter.ICMP_Sequence) == DEFAULT_SEQUENCE)
			{
			//Get current time.
				Timestamp = time(nullptr);
				if (Timestamp < 0)
					Timestamp = 0;

			//Set header data.
				if (Protocol == AF_INET6)
				{
					if (ICMPv6_Header->Sequence == UINT16_MAX)
						ICMPv6_Header->Sequence = htons(DEFAULT_SEQUENCE);
					else 
						ICMPv6_Header->Sequence = htons(ntohs(ICMPv6_Header->Sequence) + 1U);
				#if defined(PLATFORM_LINUX)
					ICMPv6_Header->Timestamp = static_cast<uint64_t>(Timestamp);
				#if defined(ENABLE_LIBSODIUM)
					ICMPv6_Header->Nonce = randombytes_random();
				#else
					ICMPv6_Header->Nonce = RandomDistribution(*GlobalRunningStatus.RandomEngine);
				#endif
				#elif defined(PLATFORM_MACOS)
					ICMPv6_Header->Timestamp = static_cast<uint64_t>(Timestamp);
				#endif
				}
				else if (Protocol == AF_INET)
				{
					if (ICMP_Header->Sequence == UINT16_MAX)
						ICMP_Header->Sequence = htons(DEFAULT_SEQUENCE);
					else 
						ICMP_Header->Sequence = htons(ntohs(ICMP_Header->Sequence) + 1U);
				#if defined(PLATFORM_LINUX)
					ICMP_Header->Timestamp = static_cast<uint64_t>(Timestamp);
				#if defined(ENABLE_LIBSODIUM)
					ICMP_Header->Nonce = randombytes_random();
				#else
					ICMP_Header->Nonce = RandomDistribution(*GlobalRunningStatus.RandomEngine);
				#endif
				#elif defined(PLATFORM_MACOS)
					ICMP_Header->Timestamp = static_cast<uint64_t>(Timestamp);
				#endif

				//Checksum calculating
					ICMP_Header->Checksum = 0;
					ICMP_Header->Checksum = GetChecksum(reinterpret_cast<uint16_t *>(SendBuffer.get()), Length);
				}
			}
		}

	//Repeat.
		Sleep(SENDING_INTERVAL_TIME);
		++Times;
	}

//Monitor terminated
	for (auto &SocketDataIter:ICMP_SocketData)
		SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"ICMP Test module Monitor terminated", 0, nullptr, 0);

	return true;
}
#endif

//Transmission and reception of TCP protocol
size_t TCP_RequestSingle(
	const REQUEST_PROCESS_TYPE RequestType, 
	const uint8_t * const OriginalSend, 
	const size_t SendSize, 
	uint8_t * const OriginalRecv, 
	const size_t RecvSize, 
	const ADDRESS_UNION_DATA * const SpecifieTargetData, 
	const SOCKET_DATA * const LocalSocketData)
{
//Initialization
	std::vector<SOCKET_DATA> TCPSocketDataList(1U);
	memset(&TCPSocketDataList.front(), 0, sizeof(TCPSocketDataList.front()));
	TCPSocketDataList.front().Socket = INVALID_SOCKET;
	memset(OriginalRecv, 0, RecvSize);
	const auto SendBuffer = OriginalRecv;
	memcpy_s(SendBuffer, RecvSize, OriginalSend, SendSize);

//Socket initialization
	bool *IsAlternate = nullptr;
	size_t *AlternateTimeoutTimes = nullptr;
	if (SelectTargetSocketSingle(RequestType, IPPROTO_TCP, &TCPSocketDataList.front(), &IsAlternate, &AlternateTimeoutTimes, SpecifieTargetData, nullptr, nullptr) == EXIT_FAILURE)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP socket initialization error", 0, nullptr, 0);
		SocketSetting(TCPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

		return EXIT_FAILURE;
	}

//Socket attribute setting(Non-blocking mode)
	if (!SocketSetting(TCPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr))
	{
		SocketSetting(TCPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return EXIT_FAILURE;
	}

//Add length of request packet.
	const auto DataLength = AddLengthDataToHeader(SendBuffer, SendSize, RecvSize);
	if (DataLength == EXIT_FAILURE)
	{
		SocketSetting(TCPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return EXIT_FAILURE;
	}

//Socket selecting
	ssize_t ErrorCode = 0;
	const auto RecvLen = SocketSelectingOnce(RequestType, IPPROTO_TCP, TCPSocketDataList, nullptr, SendBuffer, DataLength, OriginalRecv, RecvSize, &ErrorCode, LocalSocketData);
	if (ErrorCode == WSAETIMEDOUT && IsAlternate != nullptr && !*IsAlternate && //Mark timeout.
		(!Parameter.AlternateMultipleRequest || RequestType == REQUEST_PROCESS_TYPE::LOCAL_NORMAL || RequestType == REQUEST_PROCESS_TYPE::LOCAL_IN_WHITE))
			++(*AlternateTimeoutTimes);

//Close all sockets.
	if (SocketSetting(TCPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
		SocketSetting(TCPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

	return RecvLen;
}

//Transmission and reception of TCP protocol(Multiple threading)
size_t TCP_RequestMultiple(
	const REQUEST_PROCESS_TYPE RequestType, 
	const uint8_t * const OriginalSend, 
	const size_t SendSize, 
	uint8_t * const OriginalRecv, 
	const size_t RecvSize, 
	const SOCKET_DATA * const LocalSocketData)
{
//Initialization
	memset(OriginalRecv, 0, RecvSize);
	const auto SendBuffer = OriginalRecv;
	memcpy_s(SendBuffer, RecvSize, OriginalSend, SendSize);

//Socket initialization
	std::vector<SOCKET_DATA> TCPSocketDataList;
	if (!SelectTargetSocketMultiple(IPPROTO_TCP, TCPSocketDataList))
		return EXIT_FAILURE;

//Add length of request packet.
	const auto DataLength = AddLengthDataToHeader(SendBuffer, SendSize, RecvSize);
	if (DataLength == EXIT_FAILURE)
		return EXIT_FAILURE;

//Socket selecting
	ssize_t ErrorCode = 0;
	const auto RecvLen = SocketSelectingOnce(RequestType, IPPROTO_TCP, TCPSocketDataList, nullptr, SendBuffer, DataLength, OriginalRecv, RecvSize, &ErrorCode, LocalSocketData);
	if (ErrorCode == WSAETIMEDOUT && !Parameter.AlternateMultipleRequest) //Mark timeout.
	{
		if (TCPSocketDataList.front().AddrLen == sizeof(sockaddr_in6)) //IPv6
			++AlternateSwapList.TimeoutTimes[ALTERNATE_SWAP_TYPE_MAIN_TCP_IPV6];
		else if (TCPSocketDataList.front().AddrLen == sizeof(sockaddr_in)) //IPv4
			++AlternateSwapList.TimeoutTimes[ALTERNATE_SWAP_TYPE_MAIN_TCP_IPV4];
	}

//Close all sockets.
	for (auto &SocketIter:TCPSocketDataList)
	{
		if (SocketSetting(SocketIter.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
			SocketSetting(SocketIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	}

	return RecvLen;
}

//Transmission of UDP protocol
#if defined(ENABLE_PCAP)
size_t UDP_RequestSingle(
	const REQUEST_PROCESS_TYPE RequestType, 
	const uint16_t Protocol, 
	const uint8_t * const OriginalSend, 
	const size_t SendSize, 
	const SOCKET_DATA * const LocalSocketData)
{
//Initialization
	std::vector<SOCKET_DATA> UDPSocketDataList(1U);
	memset(&UDPSocketDataList.front(), 0, sizeof(UDPSocketDataList.front()));
	UDPSocketDataList.front().Socket = INVALID_SOCKET;
	bool *IsAlternate = nullptr;
	size_t *AlternateTimeoutTimes = nullptr;

//Socket initialization
	if (SelectTargetSocketSingle(RequestType, IPPROTO_UDP, &UDPSocketDataList.front(), &IsAlternate, &AlternateTimeoutTimes, nullptr, nullptr, nullptr) == EXIT_FAILURE)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"UDP socket initialization error", 0, nullptr, 0);
		SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

		return EXIT_FAILURE;
	}

//Socket attribute setting(Non-blocking mode)
	if (!SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr))
	{
		SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return EXIT_FAILURE;
	}

//Socket selecting
	const auto RecvLen = SocketSelectingOnce(RequestType, IPPROTO_UDP, UDPSocketDataList, nullptr, OriginalSend, SendSize, nullptr, 0, nullptr, LocalSocketData);
	if (RecvLen != EXIT_SUCCESS)
	{
		for (auto &SocketDataIter:UDPSocketDataList)
			SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

		return EXIT_FAILURE;
	}

//Mark port to list.
	MarkPortToList(Protocol, LocalSocketData, UDPSocketDataList);
	return EXIT_SUCCESS;
}

//Transmission of UDP protocol(Multiple threading)
size_t UDP_RequestMultiple(
	const REQUEST_PROCESS_TYPE RequestType, 
	const uint16_t Protocol, 
	const uint8_t * const OriginalSend, 
	const size_t SendSize, 
	const SOCKET_DATA * const LocalSocketData)
{
//Socket initialization
	std::vector<SOCKET_DATA> UDPSocketDataList;
	if (!SelectTargetSocketMultiple(IPPROTO_UDP, UDPSocketDataList))
		return EXIT_FAILURE;

//Socket selecting
	const auto RecvLen = SocketSelectingOnce(RequestType, IPPROTO_UDP, UDPSocketDataList, nullptr, OriginalSend, SendSize, nullptr, 0, nullptr, LocalSocketData);
	if (RecvLen != EXIT_SUCCESS)
	{
		for (auto &SocketDataIter:UDPSocketDataList)
			SocketSetting(SocketDataIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

		return EXIT_FAILURE;
	}

//Mark port to list.
	MarkPortToList(Protocol, LocalSocketData, UDPSocketDataList);
	return EXIT_SUCCESS;
}
#endif

//Complete transmission of UDP protocol
size_t UDP_CompleteRequestSingle(
	const REQUEST_PROCESS_TYPE RequestType, 
	const uint8_t * const OriginalSend, 
	const size_t SendSize, 
	uint8_t * const OriginalRecv, 
	const size_t RecvSize, 
	const ADDRESS_UNION_DATA * const SpecifieTargetData, 
	const SOCKET_DATA * const LocalSocketData)
{
//Initialization
	std::vector<SOCKET_DATA> UDPSocketDataList(1U);
	memset(&UDPSocketDataList.front(), 0, sizeof(UDPSocketDataList.front()));
	UDPSocketDataList.front().Socket = INVALID_SOCKET;
	memset(OriginalRecv, 0, RecvSize);

//Socket initialization
	bool *IsAlternate = nullptr;
	size_t *AlternateTimeoutTimes = nullptr;
	if (SelectTargetSocketSingle(RequestType, IPPROTO_UDP, &UDPSocketDataList.front(), &IsAlternate, &AlternateTimeoutTimes, SpecifieTargetData, nullptr, nullptr) == EXIT_FAILURE)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"Complete UDP socket initialization error", 0, nullptr, 0);
		SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

		return EXIT_FAILURE;
	}

//Socket attribute setting(Non-blocking mode)
	if (!SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr))
	{
		SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return EXIT_FAILURE;
	}

//Socket selecting
	ssize_t ErrorCode = 0;
	const auto RecvLen = SocketSelectingOnce(RequestType, IPPROTO_UDP, UDPSocketDataList, nullptr, OriginalSend, SendSize, OriginalRecv, RecvSize, &ErrorCode, LocalSocketData);
	if (ErrorCode == WSAETIMEDOUT && IsAlternate != nullptr && !*IsAlternate && //Mark timeout.
		(!Parameter.AlternateMultipleRequest || RequestType == REQUEST_PROCESS_TYPE::LOCAL_NORMAL || RequestType == REQUEST_PROCESS_TYPE::LOCAL_IN_WHITE))
			++(*AlternateTimeoutTimes);

//Close all sockets.
	if (SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
		SocketSetting(UDPSocketDataList.front().Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

	return RecvLen;
}

//Complete transmission of UDP protocol(Multiple threading)
size_t UDP_CompleteRequestMultiple(
	const REQUEST_PROCESS_TYPE RequestType, 
	const uint8_t * const OriginalSend, 
	const size_t SendSize, 
	uint8_t * const OriginalRecv, 
	const size_t RecvSize, 
	const SOCKET_DATA * const LocalSocketData)
{
//Initialization
	memset(OriginalRecv, 0, RecvSize);

//Socket initialization
	std::vector<SOCKET_DATA> UDPSocketDataList;
	if (!SelectTargetSocketMultiple(IPPROTO_UDP, UDPSocketDataList))
		return EXIT_FAILURE;

//Socket selecting
	ssize_t ErrorCode = 0;
	const auto RecvLen = SocketSelectingOnce(RequestType, IPPROTO_UDP, UDPSocketDataList, nullptr, OriginalSend, SendSize, OriginalRecv, RecvSize, &ErrorCode, LocalSocketData);
	if (ErrorCode == WSAETIMEDOUT && !Parameter.AlternateMultipleRequest) //Mark timeout.
	{
		if (UDPSocketDataList.front().AddrLen == sizeof(sockaddr_in6)) //IPv6
			++AlternateSwapList.TimeoutTimes[ALTERNATE_SWAP_TYPE_MAIN_TCP_IPV6];
		else if (UDPSocketDataList.front().AddrLen == sizeof(sockaddr_in)) //IPv4
			++AlternateSwapList.TimeoutTimes[ALTERNATE_SWAP_TYPE_MAIN_TCP_IPV4];
	}

//Close all sockets.
	for (auto &SocketIter:UDPSocketDataList)
	{
		if (SocketSetting(SocketIter.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
			SocketSetting(SocketIter.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	}

	return RecvLen;
}
