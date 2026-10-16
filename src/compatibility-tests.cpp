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
    auto candidate=compatibility::FindById(L"win11-x64-26100.4946");
    require(candidate!=nullptr,"26100.4946 beta candidate is present");
    require(compatibility::Hex(candidate->sha256)==L"C1786EC32A32F3815A92F2895E60FDE938033BB24007AD6AF11856C9C985491E","26100.4946 exact candidate hash");
    require(candidate->machine==IMAGE_FILE_MACHINE_AMD64 && candidate->timestamp==0xdcba0b0b && candidate->imageSize==0x3b8000,"26100.4946 PE fingerprint");
    GUID candidatePdb{0xeafc8570,0xd54b,0x672b,{0x78,0x15,0xf7,0x6b,0x54,0xd8,0x40,0x31}};
    require(candidate->requirePdbIdentity && candidate->pdbAge==1 && std::memcmp(&candidate->pdbGuid,&candidatePdb,sizeof(GUID))==0,"26100.4946 matching PDB identity");
    require(candidate->initializeRva==0xacfb0,"26100.4946 Initialize slot target RVA");
    auto gaCandidate=compatibility::FindById(L"win11-x64-26200.9168");
    require(gaCandidate!=nullptr,"26200.9168 beta candidate is present");
    require(compatibility::Hex(gaCandidate->sha256)==L"E35DD0B8940BC97DA379C6B3E6A5C5F6D1D6CCBDEE2D062D660148C30FCF37AF","26200.9168 exact candidate hash");
    require(gaCandidate->machine==IMAGE_FILE_MACHINE_AMD64 && gaCandidate->timestamp==0x381c21e7 && gaCandidate->imageSize==0x3d3000,"26200.9168 PE fingerprint");
    GUID gaCandidatePdb{0x37209f4e,0x83c8,0x8201,{0x16,0x5d,0x6c,0x7e,0x22,0x01,0xb6,0x71}};
    require(gaCandidate->requirePdbIdentity && gaCandidate->pdbAge==1 && std::memcmp(&gaCandidate->pdbGuid,&gaCandidatePdb,sizeof(GUID))==0,"26200.9168 matching PDB identity");
    require(gaCandidate->initializeRva==0xac690,"26200.9168 Initialize slot target RVA");
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
