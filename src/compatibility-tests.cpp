#include "compatibility.h"
#include <iostream>
#include <utility>

int main() {
    unsigned checks=0;
    auto require=[&](bool condition,char const* label) {
        ++checks;
        if (!condition) { std::cerr << "FAIL: " << label << '\n'; ExitProcess(1); }
    };
    require(compatibility::Profiles.size()==23,"expanded x64 GA fingerprint table has the reviewed profile count");
    std::array<std::pair<wchar_t const*,wchar_t const*>,19> expandedCandidates{{
        {L"win11-x64-26100.1742",L"EEFA8B94AA00F2DE94FD086963DF42ED63429F6CBD257A0A50B074475B487202"},
        {L"win11-x64-26100.2033",L"077FA4E4B2ABBE4D3B7057A9B80F16392578AB16A22AE8C9EE7B5F65217C5933"},
        {L"win11-x64-26100.2161",L"9D5074F1B2C49E559ECB122135A86C3ACD705D0CDFB13CF2E8FC277A5DDACDE1"},
        {L"win11-x64-26100.2454",L"F33F0C2E199C822D70A5A4E6FD1570F502EA0E4A7B66ADCDEA61D84DACB6084A"},
        {L"win11-x64-26100.3037",L"37B06332DEA944005CF4E30B15F62267B192B89616ACB17A170CF77C27ACEB0D"},
        {L"win11-x64-26100.3323",L"959AF64A0BA147631AE2FDCB1ECA1DA59832D1044006C176046E699C1C37486B"},
        {L"win11-x64-26100.3624",L"D6850CFF93FF2DA50BF52F1A25A3ECDFF52D6D0A9F12C40BE2823CE95E8A88C8"},
        {L"win11-x64-26100.3915",L"ACD252B8D62F7725629F29151DB69E1C3E8F0DC998D1E12417A1A116CE183BF3"},
        {L"win11-x64-26100.4484",L"C961FD54FF12ED94DF5EC9FD107E043FED8F67B6C138DC5A647C9E908FDF2B9C"},
        {L"win11-x64-26100.5074",L"F1684659B6E4AB964FF7A55180B06831EB1295EA80E93853154D466D5A1152B3"},
        {L"win11-x64-26200.6899",L"27DEBB0B47658A410CDAF7343576EC5B46802F57100FEBD76CFDBE1496AEECBF"},
        {L"win11-x64-26200.7019",L"AE5903681072FBA180FE12488D8C27E30E239278A65A1287D2F3B16C989C3AAA"},
        {L"win11-x64-26200.7309",L"5A832B5B673FB4DD22693FED195D49E94F300ADCDBDA9CB9BFFDDC7CBF0A63CD"},
        {L"win11-x64-26200.7705",L"46E26CF66CB8F90B21792A198B7C28663CC782C9B2F3EC3D5D0D8D4E43120448"},
        {L"win11-x64-26200.7922",L"2645119925A66E302D7476B427537586628E3A87585D8E0FC4EF31A76B7E3491"},
        {L"win11-x64-26200.8116",L"B6E6BC510A2DCE193D90C6105212C254661878F0DAA5469957F5D76B1D2AC7B0"},
        {L"win11-x64-26200.8328",L"AABE326BD7F853981BC3E1FF2948F7ACB76FE9DDE52A1F87FC1BAFBCF50EA3B9"},
        {L"win11-x64-26200.8524",L"998CDBED64F695F15581167A821B6F3D76EF0B6FD665A54A51C96B0A14B43098"},
        {L"win11-x64-26200.8737",L"4FCF9EE69AA2B7B8C0D49EA59D95B808B4AD902B320A18D9F21E82B355824E80"}
    }};
    for (auto const& [id,hash] : expandedCandidates) {
        auto profile=compatibility::FindById(id);
        require(profile!=nullptr,"reviewed GA candidate profile exists");
        require(compatibility::Hex(profile->sha256)==hash,"reviewed GA candidate hash is exact");
        require(profile->requirePdbIdentity && profile->pdbAge==1,"reviewed GA candidate requires its exact PDB identity");
    }
    require(compatibility::FindById(L"win11-x64-26100.4202")==nullptr,"inconclusive PDB candidate remains unsupported");
    for (auto const& profile : compatibility::Profiles) {
        GUID emptyIid{};
        require(std::memcmp(&profile.initializeIid,&emptyIid,sizeof(GUID))!=0,"profile has a verified Initialize IID");
        auto metadataMatches=std::count_if(compatibility::Profiles.begin(),compatibility::Profiles.end(),[&](auto const& other) {
            return other.machine==profile.machine && other.timestamp==profile.timestamp && other.imageSize==profile.imageSize;
        });
        require(metadataMatches==1,"PE metadata uniquely selects a profile");
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
    GUID currentInitializeIid{0xca79615d,0xbfba,0x58dc,{0x8d,0x55,0x74,0x19,0x63,0x98,0x5c,0x48}};
    require(std::memcmp(&candidate->initializeIid,&currentInitializeIid,sizeof(GUID))==0,"26100.4946 profile-specific Initialize IID");
    require(candidate->initializeRva==0xacfb0,"26100.4946 Initialize slot target RVA");
    auto gaCandidate=compatibility::FindById(L"win11-x64-26200.9168");
    require(gaCandidate!=nullptr,"26200.9168 beta candidate is present");
    require(compatibility::Hex(gaCandidate->sha256)==L"E35DD0B8940BC97DA379C6B3E6A5C5F6D1D6CCBDEE2D062D660148C30FCF37AF","26200.9168 exact candidate hash");
    require(gaCandidate->machine==IMAGE_FILE_MACHINE_AMD64 && gaCandidate->timestamp==0x381c21e7 && gaCandidate->imageSize==0x3d3000,"26200.9168 PE fingerprint");
    GUID gaCandidatePdb{0x37209f4e,0x83c8,0x8201,{0x16,0x5d,0x6c,0x7e,0x22,0x01,0xb6,0x71}};
    require(gaCandidate->requirePdbIdentity && gaCandidate->pdbAge==1 && std::memcmp(&gaCandidate->pdbGuid,&gaCandidatePdb,sizeof(GUID))==0,"26200.9168 matching PDB identity");
    require(std::memcmp(&gaCandidate->initializeIid,&currentInitializeIid,sizeof(GUID))==0,"26200.9168 profile-specific Initialize IID");
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
