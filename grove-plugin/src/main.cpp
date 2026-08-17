#include <wups.h>
#include <function_patcher/function_patching.h>
#include <kernel/kernel.h>
#include <coreinit/filesystem.h>
#include <utils/logger.h>
#include <cstring>
#include <cstdint>
#include <vector>

WUPS_PLUGIN_NAME("Grove");
WUPS_PLUGIN_DESCRIPTION("Grove local eShop tester");
WUPS_PLUGIN_VERSION("0.1.0");
WUPS_PLUGIN_AUTHOR("RasberryTech");
WUPS_PLUGIN_LICENSE("GPLv3");
WUPS_USE_WUT_DEVOPTAB();

namespace {

constexpr char kOriginalWave[] = "https://ninja.wup.shop.nintendo.net/ninja/wood_index.html?";
constexpr char kGroveWave[] = "http://192.168.1.194:3001/geisha?";

struct EShopAllowlist {
    char scheme[16];
    char domain[128];
    char path[128];
    unsigned char flags[5];
};

constexpr EShopAllowlist kOriginalEntry = {
    "https",
    "samurai.wup.shop.nintendo.net",
    "",
    {1, 1, 1, 1, 0}
};

constexpr EShopAllowlist kGroveEntry = {
    "http",
    "192.168.1.194",
    "",
    {1, 1, 1, 1, 0}
};

std::vector<PatchedFunctionHandle> patches;

bool replaceBytes(uint32_t start, uint32_t size, const void *original, size_t originalSize,
                  const void *replacement, size_t replacementSize) {
    if (replacementSize > originalSize || replacementSize == 0) {
        return false;
    }

    for (uint32_t address = start; address < start + size - originalSize; ++address) {
        if (std::memcmp(reinterpret_cast<const void *>(address), original, originalSize) == 0) {
            KernelCopyData(OSEffectiveToPhysical(address),
                           OSEffectiveToPhysical(reinterpret_cast<uint32_t>(replacement)),
                           replacementSize);
            return true;
        }
    }
    return false;
}

DECL_FUNCTION(int, Grove_FSOpenFile, FSClient *client, FSCmdBlock *block, char *path,
              const char *mode, uint32_t *handle, int error) {
    constexpr char initialOma[] = "vol/content/initial.oma";

    if (std::strcmp(initialOma, path) == 0) {
        const bool urlFound = replaceBytes(
            0x10000000, 0x10000000,
            kOriginalWave, sizeof(kOriginalWave),
            kGroveWave, sizeof(kGroveWave));

        const bool allowlistFound = replaceBytes(
            0x10000000, 0x10000000,
            &kOriginalEntry, sizeof(kOriginalEntry),
            &kGroveEntry, sizeof(kGroveEntry));

        DEBUG_FUNCTION_LINE("Grove eShop hook: URL=%s allowlist=%s",
                            urlFound ? "patched" : "not found",
                            allowlistFound ? "patched" : "not found");
    }

    return real_Grove_FSOpenFile(client, block, path, mode, handle, error);
}

void installPatches() {
    if (FunctionPatcher_InitLibrary() != FUNCTION_PATCHER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE("Grove: FunctionPatcher initialization failed");
        return;
    }

    function_replacement_data_t replacement =
        REPLACE_FUNCTION_FOR_PROCESS(Grove_FSOpenFile, LIBRARY_COREINIT, FSOpenFile, FP_TARGET_PROCESS_ESHOP);

    PatchedFunctionHandle handle = 0;
    if (FunctionPatcher_AddFunctionPatch(&replacement, &handle, nullptr) != FUNCTION_PATCHER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE("Grove: failed to install eShop FSOpenFile hook");
        return;
    }

    patches.push_back(handle);
    DEBUG_FUNCTION_LINE("Grove: eShop tester hook installed");
}

void removePatches() {
    for (const auto handle : patches) {
        FunctionPatcher_RemoveFunctionPatch(handle);
    }
    patches.clear();
    FunctionPatcher_DeInitLibrary();
}

} // namespace

INITIALIZE_PLUGIN() {
    WHBLogCafeInit();
    WHBLogUdpInit();
    installPatches();
}

DEINITIALIZE_PLUGIN() {
    removePatches();
    WHBLogCafeDeinit();
    WHBLogUdpDeinit();
}

ON_APPLICATION_START() {
    DEBUG_FUNCTION_LINE("Grove: application started");
}

ON_APPLICATION_ENDS() {
}
