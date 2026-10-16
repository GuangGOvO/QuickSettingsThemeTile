#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <winternl.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace compatibility {
struct Profile {
    wchar_t const* id;
    wchar_t const* displayName;
    WORD machine;
    DWORD timestamp;
    DWORD imageSize;
    std::array<BYTE,32> sha256;
    GUID pdbGuid;
    DWORD pdbAge;
    bool requirePdbIdentity;
    DWORD initializeRva;
};

inline constexpr std::array<Profile,2> Profiles{{
    {L"win11-x64-26300.9457",L"Windows 11 x64 26300.9457",IMAGE_FILE_MACHINE_AMD64,0xf0b032a1,0x3d3000,
     {0x3e,0x9a,0x30,0xbd,0x1e,0x92,0x22,0x0b,0xae,0xeb,0xa9,0x0c,0xd0,0xac,0x40,0x9a,0xe5,0x41,0xc6,0x50,0x01,0x9a,0xd7,0xc8,0x0a,0xec,0x97,0xda,0xf3,0x9c,0x32,0x61},
     GUID{},0,false,0xac6a0},
    {L"win11-x64-26300.9711",L"Windows 11 x64 26300.9711",IMAGE_FILE_MACHINE_AMD64,0x7162bcec,0x3ea000,
     {0x0a,0x9a,0x84,0xb0,0xb7,0xed,0x61,0x10,0xa4,0x10,0xbd,0x76,0x15,0x0c,0xe0,0xda,0x04,0xdc,0x29,0xb9,0x01,0x83,0x81,0x55,0xab,0x7c,0xe2,0xd8,0xbf,0x2f,0xfb,0x51},
     GUID{0x69549732,0xa6cf,0x1c43,{0xfd,0xf7,0xed,0x1f,0xd2,0x22,0x7e,0x42}},1,true,0xac420}
}};

struct ImageFacts {
    WORD machine{};
    DWORD timestamp{};
    DWORD imageSize{};
    std::array<BYTE,32> sha256{};
    GUID pdbGuid{};
    DWORD pdbAge{};
    bool hasPdbIdentity{};
};

enum class Status { Supported, UnknownBinary, MetadataMismatch, InvalidImage, ReadError };
struct Result {
    Status status=Status::ReadError;
    Profile const* profile=nullptr;
    ImageFacts facts{};
    DWORD error=ERROR_SUCCESS;
    constexpr bool supported() const noexcept { return status==Status::Supported && profile!=nullptr; }
};
struct HostVersion {
    DWORD major{};
    DWORD minor{};
    DWORD build{};
    DWORD ubr{};
    bool valid{};
    bool hasUbr{};
};

inline HostVersion ReadHostVersion() noexcept {
    HostVersion result;
    auto ntdll=GetModuleHandleW(L"ntdll.dll");
    auto rtl=ntdll?reinterpret_cast<LONG (WINAPI*)(PRTL_OSVERSIONINFOW)>(GetProcAddress(ntdll,"RtlGetVersion")):nullptr;
    if (!rtl) return result;
    RTL_OSVERSIONINFOW version{sizeof(version)};
    if (rtl(&version)<0) return result;
    result.major=version.dwMajorVersion; result.minor=version.dwMinorVersion; result.build=version.dwBuildNumber; result.valid=true;
    DWORD size=sizeof(result.ubr);
    result.hasUbr=RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"UBR",RRF_RT_REG_DWORD,nullptr,&result.ubr,&size)==ERROR_SUCCESS;
    return result;
}

inline bool Contains(std::span<BYTE const> bytes,size_t offset,size_t length) noexcept {
    return offset<=bytes.size() && length<=bytes.size()-offset;
}
template<class T> inline bool Read(std::span<BYTE const> bytes,size_t offset,T& value) noexcept {
    if (!Contains(bytes,offset,sizeof(T))) return false;
    std::memcpy(&value,bytes.data()+offset,sizeof(T)); return true;
}

inline bool RvaToOffset(std::span<BYTE const> bytes,IMAGE_FILE_HEADER const& file,size_t fileOffset,IMAGE_OPTIONAL_HEADER64 const& optional,DWORD rva,size_t length,size_t& offset) noexcept {
    if (rva<optional.SizeOfHeaders && Contains(bytes,rva,length)) { offset=rva; return true; }
    auto sectionOffset=fileOffset+sizeof(file)+file.SizeOfOptionalHeader;
    if (file.NumberOfSections>96 || !Contains(bytes,sectionOffset,static_cast<size_t>(file.NumberOfSections)*sizeof(IMAGE_SECTION_HEADER))) return false;
    for (WORD i=0;i<file.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER section{};
        if (!Read(bytes,sectionOffset+static_cast<size_t>(i)*sizeof(section),section)) return false;
        auto spanSize=std::max(section.Misc.VirtualSize,section.SizeOfRawData);
        if (rva<section.VirtualAddress || rva-section.VirtualAddress>=spanSize) continue;
        auto delta=static_cast<size_t>(rva-section.VirtualAddress);
        if (delta>section.SizeOfRawData || length>section.SizeOfRawData-delta) return false;
        offset=static_cast<size_t>(section.PointerToRawData)+delta;
        return Contains(bytes,offset,length);
    }
    return false;
}

inline bool ParseImage(std::span<BYTE const> bytes,ImageFacts& facts) noexcept {
    IMAGE_DOS_HEADER dos{};
    if (!Read(bytes,0,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0) return false;
    auto ntOffset=static_cast<size_t>(dos.e_lfanew); DWORD signature=0;
    if (!Read(bytes,ntOffset,signature) || signature!=IMAGE_NT_SIGNATURE) return false;
    IMAGE_FILE_HEADER file{};
    auto fileOffset=ntOffset+sizeof(signature);
    if (!Read(bytes,fileOffset,file) || file.SizeOfOptionalHeader<sizeof(WORD)) return false;
    auto optionalOffset=fileOffset+sizeof(file); WORD magic=0;
    if (!Read(bytes,optionalOffset,magic)) return false;
    IMAGE_OPTIONAL_HEADER64 optional{};
    IMAGE_DATA_DIRECTORY debugDirectory{};
    if (magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        constexpr auto required=offsetof(IMAGE_OPTIONAL_HEADER64,DataDirectory)+(IMAGE_DIRECTORY_ENTRY_DEBUG+1)*sizeof(IMAGE_DATA_DIRECTORY);
        if (file.SizeOfOptionalHeader<required || file.SizeOfOptionalHeader<sizeof(optional) || !Read(bytes,optionalOffset,optional)) return false;
        if (optional.NumberOfRvaAndSizes>IMAGE_DIRECTORY_ENTRY_DEBUG) debugDirectory=optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    } else if (magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 optional32{};
        constexpr auto required=offsetof(IMAGE_OPTIONAL_HEADER32,DataDirectory)+(IMAGE_DIRECTORY_ENTRY_DEBUG+1)*sizeof(IMAGE_DATA_DIRECTORY);
        if (file.SizeOfOptionalHeader<required || file.SizeOfOptionalHeader<sizeof(optional32) || !Read(bytes,optionalOffset,optional32)) return false;
        optional.SizeOfHeaders=optional32.SizeOfHeaders;
        optional.SizeOfImage=optional32.SizeOfImage;
        optional.NumberOfRvaAndSizes=optional32.NumberOfRvaAndSizes;
        if (optional32.NumberOfRvaAndSizes>IMAGE_DIRECTORY_ENTRY_DEBUG) debugDirectory=optional32.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    } else return false;
    if (!optional.SizeOfImage) return false;
    facts.machine=file.Machine; facts.timestamp=file.TimeDateStamp; facts.imageSize=optional.SizeOfImage;
    if (!debugDirectory.VirtualAddress || debugDirectory.Size<sizeof(IMAGE_DEBUG_DIRECTORY)) return true;
    size_t debugOffset=0;
    if (!RvaToOffset(bytes,file,fileOffset,optional,debugDirectory.VirtualAddress,debugDirectory.Size,debugOffset)) return false;
    auto count=debugDirectory.Size/sizeof(IMAGE_DEBUG_DIRECTORY);
    for (DWORD i=0;i<count;++i) {
        IMAGE_DEBUG_DIRECTORY entry{};
        if (!Read(bytes,debugOffset+static_cast<size_t>(i)*sizeof(entry),entry)) return false;
        if (entry.Type!=IMAGE_DEBUG_TYPE_CODEVIEW || entry.SizeOfData<24) continue;
        size_t codeViewOffset=entry.PointerToRawData;
        if (!Contains(bytes,codeViewOffset,24)) continue;
        DWORD codeViewSignature=0;
        if (!Read(bytes,codeViewOffset,codeViewSignature) || codeViewSignature!=0x53445352) continue; // RSDS
        if (!Read(bytes,codeViewOffset+4,facts.pdbGuid) || !Read(bytes,codeViewOffset+20,facts.pdbAge)) return false;
        facts.hasPdbIdentity=true; break;
    }
    return true;
}

inline Profile const* FindByMetadata(WORD machine,DWORD timestamp,DWORD imageSize) noexcept {
    for (auto const& profile : Profiles)
        if (profile.machine==machine && profile.timestamp==timestamp && profile.imageSize==imageSize) return &profile;
    return nullptr;
}

inline Profile const* FindById(wchar_t const* id) noexcept {
    if (!id) return nullptr;
    for (auto const& profile : Profiles) if (_wcsicmp(profile.id,id)==0) return &profile;
    return nullptr;
}

inline Result Match(ImageFacts const& facts) noexcept {
    Result result; result.facts=facts;
    for (auto const& profile : Profiles) {
        if (profile.sha256!=facts.sha256) continue;
        result.profile=&profile;
        if (facts.machine!=profile.machine || facts.timestamp!=profile.timestamp || facts.imageSize!=profile.imageSize) {
            result.status=Status::MetadataMismatch; return result;
        }
        if (profile.requirePdbIdentity && (!facts.hasPdbIdentity || std::memcmp(&facts.pdbGuid,&profile.pdbGuid,sizeof(GUID))!=0 || facts.pdbAge!=profile.pdbAge)) {
            result.status=Status::MetadataMismatch; return result;
        }
        result.status=Status::Supported; return result;
    }
    result.status=Status::UnknownBinary; return result;
}

inline bool Hash(std::span<BYTE const> bytes,std::array<BYTE,32>& digest) noexcept {
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr; DWORD objectLength=0,returned=0;
    auto status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    if (status<0) return false;
    status=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<BYTE*>(&objectLength),sizeof(objectLength),&returned,0);
    std::vector<BYTE> object(objectLength);
    if (status>=0) status=BCryptCreateHash(algorithm,&hash,object.data(),objectLength,nullptr,0,0);
    if (status>=0 && !bytes.empty()) status=BCryptHashData(hash,const_cast<BYTE*>(bytes.data()),static_cast<ULONG>(bytes.size()),0);
    if (status>=0) status=BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0);
    if (hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm,0);
    return status>=0;
}

inline Result InspectPath(std::filesystem::path const& path) {
    Result result;
    auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) { result.error=GetLastError(); return result; }
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file,&length) || length.QuadPart<sizeof(IMAGE_DOS_HEADER) || length.QuadPart>256ll*1024*1024) {
        result.error=GetLastError(); if (!result.error) result.error=ERROR_FILE_INVALID; CloseHandle(file); return result;
    }
    std::vector<BYTE> bytes(static_cast<size_t>(length.QuadPart)); DWORD total=0;
    while (total<bytes.size()) {
        DWORD read=0; auto chunk=static_cast<DWORD>(std::min<size_t>(bytes.size()-total,MAXDWORD));
        if (!ReadFile(file,bytes.data()+total,chunk,&read,nullptr) || !read) { result.error=GetLastError(); if (!result.error) result.error=ERROR_READ_FAULT; CloseHandle(file); return result; }
        total+=read;
    }
    CloseHandle(file);
    if (!ParseImage(bytes,result.facts)) { result.status=Status::InvalidImage; result.error=ERROR_BAD_EXE_FORMAT; return result; }
    if (!Hash(bytes,result.facts.sha256)) { result.error=ERROR_GEN_FAILURE; return result; }
    result=Match(result.facts); return result;
}

inline Result InspectSystemDll() {
    wchar_t system[MAX_PATH]{}; auto length=GetSystemDirectoryW(system,MAX_PATH);
    if (!length || length>=MAX_PATH) { Result result; result.error=GetLastError(); return result; }
    return InspectPath(std::filesystem::path(system)/L"ControlCenter.dll");
}

inline wchar_t const* StatusName(Status status) noexcept {
    switch(status) {
    case Status::Supported: return L"supported";
    case Status::UnknownBinary: return L"unknown_binary";
    case Status::MetadataMismatch: return L"metadata_mismatch";
    case Status::InvalidImage: return L"invalid_image";
    default: return L"read_error";
    }
}

inline std::wstring Hex(std::span<BYTE const> bytes) {
    constexpr wchar_t digits[]=L"0123456789ABCDEF"; std::wstring result; result.reserve(bytes.size()*2);
    for (auto byte : bytes) { result.push_back(digits[byte>>4]); result.push_back(digits[byte&0xf]); }
    return result;
}

inline std::wstring HexValue(uint64_t value) {
    constexpr wchar_t digits[]=L"0123456789ABCDEF"; wchar_t buffer[19]{}; size_t position=ARRAYSIZE(buffer)-1;
    do { buffer[--position]=digits[value&0xf]; value>>=4; } while (value);
    buffer[--position]=L'x'; buffer[--position]=L'0'; return buffer+position;
}

inline std::wstring PdbGuidText(GUID const& guid) {
    wchar_t text[40]{}; StringFromGUID2(guid,text,ARRAYSIZE(text)); return text;
}
}
