#pragma once
#include <string_view>
namespace tt {
// Exact local executable hashes. Address provenance: TarnishedTool Offsets.cs,
// 8df23f1b19c62997ba9e3822495c1d2bd001ab88 explicitly shares the used RVAs.
inline constexpr std::string_view supportedVersion(std::string_view sha){
    if(sha=="d1a84083c6c7c7902162ff098f7d86812839aa6b3575959398857e539c488134")return "2.7.0.0";
    if(sha=="1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891")return "2.7.1.0";
    return {};
}
}
