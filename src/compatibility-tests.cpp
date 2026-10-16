#include "compatibility.h"
#include <iostream>

int main() {
    unsigned checks=0;
    auto require=[&](bool condition,char const* label) {
        ++checks;
        if (!condition) { std::cerr << "FAIL: " << label << '\n'; ExitProcess(1); }
    };
    for (auto const& profile : compatibility::Profiles) {
        compatibility::ImageFacts facts{};
        facts.machine=profile.machine; facts.timestamp=profile.timestamp; facts.imageSize=profile.imageSize;
        facts.sha256=profile.sha256; facts.pdbGuid=profile.pdbGuid; facts.pdbAge=profile.pdbAge;
        facts.hasPdbIdentity=profile.requirePdbIdentity;
        auto result=compatibility::Match(facts);
        require(result.supported() && result.profile==&profile,"known profile exact match");
        require(compatibility::FindByMetadata(facts.machine,facts.timestamp,facts.imageSize)==&profile,"loaded image profile selection");
        facts.sha256[0]^=0xff;
        require(compatibility::Match(facts).status==compatibility::Status::UnknownBinary,"unknown hash rejected");
        facts.sha256=profile.sha256; facts.timestamp^=1;
        require(compatibility::Match(facts).status==compatibility::Status::MetadataMismatch,"metadata mismatch rejected");
        facts.timestamp=profile.timestamp;
        if (profile.requirePdbIdentity) {
            facts.pdbGuid.Data1^=1;
            require(compatibility::Match(facts).status==compatibility::Status::MetadataMismatch,"PDB identity mismatch rejected");
            facts.pdbGuid=profile.pdbGuid; facts.hasPdbIdentity=false;
            require(compatibility::Match(facts).status==compatibility::Status::MetadataMismatch,"missing required PDB identity rejected");
        }
    }
    std::array<BYTE,0x400> image{};
    IMAGE_DOS_HEADER dos{}; dos.e_magic=IMAGE_DOS_SIGNATURE; dos.e_lfanew=0x80;
    std::memcpy(image.data(),&dos,sizeof(dos));
    DWORD signature=IMAGE_NT_SIGNATURE; std::memcpy(image.data()+dos.e_lfanew,&signature,sizeof(signature));
    IMAGE_FILE_HEADER file{}; file.Machine=IMAGE_FILE_MACHINE_AMD64; file.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);
    auto fileOffset=static_cast<size_t>(dos.e_lfanew)+sizeof(signature);
    std::memcpy(image.data()+fileOffset,&file,sizeof(file));
    IMAGE_OPTIONAL_HEADER64 optional{}; optional.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC; optional.SizeOfImage=0x2000;
    optional.SizeOfHeaders=0x200; optional.NumberOfRvaAndSizes=IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    std::memcpy(image.data()+fileOffset+sizeof(file),&optional,sizeof(optional));
    compatibility::ImageFacts parsed{};
    require(compatibility::ParseImage(image,parsed),"valid PE64 parses");
    require(parsed.machine==IMAGE_FILE_MACHINE_AMD64 && parsed.imageSize==0x2000 && !parsed.hasPdbIdentity,"PE facts extracted");
    auto shortOptional=image; IMAGE_FILE_HEADER shortFile=file; shortFile.SizeOfOptionalHeader=sizeof(WORD);
    std::memcpy(shortOptional.data()+fileOffset,&shortFile,sizeof(shortFile));
    require(!compatibility::ParseImage(shortOptional,parsed),"short optional header rejected");
    require(!compatibility::ParseImage(std::span<BYTE const>{image.data(),64},parsed),"truncated PE rejected");
    image[dos.e_lfanew]=0;
    require(!compatibility::ParseImage(image,parsed),"invalid PE signature rejected");
    require(compatibility::HexValue(0)==L"0x0","zero hex formatting");
    require(compatibility::HexValue(0xffffffffffffffffull)==L"0xFFFFFFFFFFFFFFFF","64-bit hex formatting");
    std::cout << "PASS: " << checks << " compatibility profile and PE parser checks\n";
}
