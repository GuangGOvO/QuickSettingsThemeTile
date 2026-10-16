#include "compatibility.h"
#include <iostream>

int wmain(int argc,wchar_t** argv) {
    auto result=argc>1?compatibility::InspectPath(std::filesystem::path(argv[1])):compatibility::InspectSystemDll();
    auto host=compatibility::ReadHostVersion();
    std::wcout << L"{\"status\":\"" << compatibility::StatusName(result.status)
        << L"\",\"supported\":" << (result.supported()?L"true":L"false")
        << L",\"profileId\":\"" << (result.profile?result.profile->id:L"")
        << L"\",\"scannerHostBuild\":" << host.build << L",\"scannerHostUbr\":" << host.ubr
        << L",\"windowsVersionAvailable\":" << (host.valid?L"true":L"false")
        << L",\"windowsUbrAvailable\":" << (host.hasUbr?L"true":L"false")
        << L",\"sha256\":\"" << compatibility::Hex(result.facts.sha256)
        << L"\",\"machine\":\"" << compatibility::HexValue(result.facts.machine)
        << L"\",\"peTimestamp\":\"" << compatibility::HexValue(result.facts.timestamp)
        << L"\",\"imageSize\":\"" << compatibility::HexValue(result.facts.imageSize)
        << L"\",\"pdbGuid\":\"" << (result.facts.hasPdbIdentity?compatibility::PdbGuidText(result.facts.pdbGuid):L"")
        << L"\",\"pdbAge\":" << result.facts.pdbAge
        << L",\"error\":" << result.error << L"}\n";
    return result.supported()?0:2;
}
