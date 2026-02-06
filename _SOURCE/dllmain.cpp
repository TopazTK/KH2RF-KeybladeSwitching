#define _CRT_SECURE_NO_WARNINGS

#include "Windows.h"
#include <cstdint>
#include <Psapi.h>
#include <vector>
#include <string>

HMODULE MAIN_HANDLE;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

using namespace std;

struct ModuleInfo
{
    const char* startAddr;
    const char* endAddr;

    ModuleInfo()
    {
        HMODULE hModule = GetModuleHandle(NULL);
        startAddr = reinterpret_cast<const char*>(hModule);

        MODULEINFO info = {};
        GetModuleInformation(GetCurrentProcess(), hModule, &info, sizeof(info));
        endAddr = startAddr + info.SizeOfImage;
    }
};

static const ModuleInfo moduleInfo;

template <typename T>
T SignatureScan(const char* pattern, const char* mask)
{
    size_t patLen = std::strlen(mask);

    for (const char* addr = moduleInfo.startAddr; addr < moduleInfo.endAddr - patLen; ++addr)
    {
        size_t i = 0;
        for (; i < patLen; ++i)
        {
            if (mask[i] != '?' && pattern[i] != addr[i])
                break;
        }

        if (i == patLen)
            return reinterpret_cast<T>(const_cast<char*>(addr));
    }

    return nullptr;
}

inline vector<char*> RangedMultiScan(const char* pattern, const char* mask, char* startAddr, size_t length)
{
    vector<char*> _returnList;
    size_t patLen = std::strlen(mask);

    for (const char* addr = startAddr; addr < (startAddr + length) - patLen; ++addr)
    {
        size_t i = 0;
        for (; i < patLen; ++i)
        {
            if (mask[i] != '?' && pattern[i] != addr[i])
                break;
        }

        if (i == patLen)
        {
            _returnList.push_back(const_cast<char*>(addr));
            continue;
        }
    }

    return _returnList;
}

inline uintptr_t ResolveCallRelative(const void* callInstr)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(callInstr);

    if (bytes[0] != 0xE8 && bytes[0] != 0xE9)
        return 0;

    int32_t relOffset;
    std::memcpy(&relOffset, bytes + 1, sizeof(relOffset));

    return reinterpret_cast<uintptr_t>(bytes + 5 + relOffset);
}

template <typename T>
T ResolveFunctionFromCall(const char* pattern, const char* mask, size_t callOffset = 0)
{
    uint8_t* match = SignatureScan<uint8_t*>(pattern, mask);
    if (!match)
        return nullptr;

    uintptr_t target = ResolveCallRelative(match + callOffset);
    return reinterpret_cast<T>(target);
}

template <typename T>
T ResolveRelativeAddress(const char* pattern, const char* mask, size_t callOffset = 0)
{
    size_t patLen = std::strlen(mask);
    size_t currOffset = 0;

    for (const char* addr = moduleInfo.startAddr; addr < moduleInfo.endAddr - patLen; ++addr)
    {
        size_t i = 0;

        for (; i < patLen; ++i)
        {
            if (mask[i] != '?' && pattern[i] != addr[i])
                break;
        }

        if (i == patLen)
        {
            uint32_t _fetchValue;
            std::memcpy(&_fetchValue, addr + callOffset, sizeof(int));
            return reinterpret_cast<T>(const_cast<char*>(_fetchValue + addr + callOffset + 4));
        }

        currOffset++;
    }

    return 0x00;
}

template <typename T>
T ResolveRelativeAddress(const char* addr, size_t callOffset = 0)
{
    int32_t relOffset;
    std::memcpy(&relOffset, addr + callOffset, sizeof(relOffset));

    return reinterpret_cast<T>(const_cast<char*>(addr) + relOffset + callOffset + 0x04);
}

bool COLOR_SWAPPED = false;
bool IS_KEYBLADE_MENU = false;
bool KEYBLADE_DEBOUNCE = false;

int TARGET_KEY = 0x00;

char* MENU_2DD_POINTERS = const_cast<char*>(moduleInfo.startAddr) + *reinterpret_cast<uint32_t*>(SignatureScan<char*>("\x40\x53\x48\x81\xEC\xB0\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x84\x24\xA0\x00\x00\x00\x80\x3D", "xxxxxxxxxxxx????xxxxxxxxxxxxx") + 0xC4);
bool RETRY_ACCOMMODATED = false;

char* CURRENT_SUBMENU = ResolveRelativeAddress<char*>("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x20\x48\x8B\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????x????", 0xF0);

vector<char*> MENU_MAIN_LABEL;
vector<char*> MENU_INSIDE_LABELS;
vector<char*> MENU_FORM_LABELS;
vector<char*> MENU_FORM_TEXTS;

char* MENU_SEQUENCE_ARRAY = SignatureScan<char*>("\xCF\x00\xD1\x00\xD0\x00\xD9\x00\xDB\x00\xDA\x00\xAB\x00\xAD\x00", "xxxxxxxxxxxxxxxx");
char** MENU_ITEMS = ResolveRelativeAddress<char**>("\x40\x53\x55\x56\x57\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x20\xE8\x00\x00\x00\x00\x48\x8B\x0D\x00\x00\x00\x00\x4C\x8B\xF8", "xxxxxxxxxxxxxxxx????xxx????xxx", 0x26);

bool* BTLEND_FLAG = ResolveRelativeAddress<bool*>("\x40\x53\x48\x83\xEC\x20\x80\x79\x18\x00\x48\x8B\xD9\x75\x7C", "xxxxxxxxxxxxxxx", 0x82) + 0x01;

// I do NOT know what there are, and am too lazy to figure out.
void(*MENU_COMMIT_FIRST)(char*, int, int) = SignatureScan<void(*)(char*, int, int)>("\x40\x53\x56\x57\x41\x54\x41\x56\x48\x83\xEC\x20\x4C\x8D\x71\x04", "xxxxxxxxxxxxxxxx");
void(*MENU_COMMIT_SECOND)(char*, int, int) = SignatureScan<void(*)(char*, int, int)>("\x40\x53\x55\x56\x57\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x20\x48", "xxxxxxxxxxxxxxxx");

void(*ITEM_COMMIT)() = nullptr;

extern "C"
{
    __declspec(dllexport) char* RF_ExcludeFunctions()
    {
        char* _allocChar = (char*)malloc(0x20);

        fill(_allocChar, _allocChar + 0x20, 0x00);
        memcpy(_allocChar, "PROCESS_FORM_KEYBLADES", 22);

        return _allocChar;
    }

	__declspec(dllexport) void RF_ModuleInit(const wchar_t* mod_path)
	{
        wchar_t filepath[MAX_PATH];

        wcscpy(filepath, mod_path);
        wcscat(filepath, L"\\dll\\ReFined.KH2.dll");

        MAIN_HANDLE = LoadLibraryW(filepath);
	}

	__declspec(dllexport) void RF_ModuleExecute()
	{
        bool* _isMenu = *(bool**)GetProcAddress(MAIN_HANDLE, "?IsMenu@MENU@YS@@2PEA_NEA");
        bool* _isTitle = *(bool**)GetProcAddress(MAIN_HANDLE, "?IsTitle@TITLE@YS@@2PEA_NEA");
        bool* _isInMap = *(bool**)GetProcAddress(MAIN_HANDLE, "?IsInMap@AREA@YS@@2PEA_NEA");
        char* _menuType = *(char**)GetProcAddress(MAIN_HANDLE, "?MenuType@MENU@YS@@2PEADEA");
        char* _subMenuType = *(char**)GetProcAddress(MAIN_HANDLE, "?SubMenuType@MENU@YS@@2PEADEA");
        char* _saveData = *(char**)GetProcAddress(MAIN_HANDLE, "?SaveData@AREA@YS@@2PEADEA");
        
        uint8_t* _campOptions = *(uint8_t**)GetProcAddress(MAIN_HANDLE, "?CampOptions@MENU@YS@@2PEAEEA");

        uint16_t* _hardpadInput = *(uint16_t**)GetProcAddress(MAIN_HANDLE, "?Input@HARDPAD@YS@@2PEAGEA");
        uint16_t* _loadedItempic = *(uint16_t**)GetProcAddress(MAIN_HANDLE, "?LoadedId@ITEMPIC@YS@@2PEAGEA");

        uint64_t _subOptionSelectPtr = *(uint64_t*)GetProcAddress(MAIN_HANDLE, "?pint_suboptionselect@MENU@YS@@2_KA");
        uint64_t _eventInfoPtr = *(uint64_t*)GetProcAddress(MAIN_HANDLE, "?pint_eventinfo@EVENT@YS@@2_KA");

        uint64_t* _regionDefaultPtr = *(uint64_t**)GetProcAddress(MAIN_HANDLE, "?pint_region_default@REGION@YS@@2PEA_KEA");
        uint64_t* _regionCurrentPtr = *(uint64_t**)GetProcAddress(MAIN_HANDLE, "?pint_region@REGION@YS@@2PEA_KEA");

        uint64_t* _fetchSoraPtr = *(uint64_t**)GetProcAddress(MAIN_HANDLE, "?pint_sora@SORA@YS@@2_KA");

        using PlaySFX_t = void(*)(uint32_t);
        using GetRegion_t = uint32_t(*)();
        using GetData_t = char* (*)(int);
        using GetMessageSize_t = const size_t(*)(const char*);
        using GetFileSize_t = size_t(*)(const char*);
        using GetNumBackyard_t = uint64_t(*)(uint64_t);
        using ReduceBackyard_t = void(*)(uint16_t, int);

        PlaySFX_t _playSFX = *(PlaySFX_t*)GetProcAddress(MAIN_HANDLE, "?PlaySFX@SOUND@YS@@2P6AXI@ZEA");
        GetRegion_t _getRegion = *(GetRegion_t*)GetProcAddress(MAIN_HANDLE, "?Get@REGION@YS@@2P6AIXZEA");
        GetData_t _getData = *(GetData_t*)GetProcAddress(MAIN_HANDLE, "?GetData@MESSAGE@YS@@2P6APEADH@ZEA");
        GetMessageSize_t _getMessageSize = *(GetMessageSize_t*)GetProcAddress(MAIN_HANDLE, "?GetSize@MESSAGE@YS@@2P6A?B_KPEBD@ZEA");
        GetFileSize_t _getFileSize = *(GetFileSize_t*)GetProcAddress(MAIN_HANDLE, "?GetSize@FILE@YS@@2P6A_KPEBD@ZEA");
        GetNumBackyard_t _getNumBackyard = *(GetNumBackyard_t*)GetProcAddress(MAIN_HANDLE, "?GetNumBackyard@ITEM@YS@@2P6A_K_K@ZEA");
        ReduceBackyard_t _reduceBackyard = *(ReduceBackyard_t*)GetProcAddress(MAIN_HANDLE, "?ReduceBackyard@ITEM@YS@@2P6AXGH@ZEA");

        using PanaceaGet_t = char* (*)(string);
        using PanaceaFree_t = void (*)(string);
        using PanaceaAllocate_t = void (*)(string, size_t);
        using ChangeWeapon_t = void(*)(char*, int, bool, int, bool);

        PanaceaGet_t _getPanacea = (PanaceaGet_t)GetProcAddress(MAIN_HANDLE, "?Get@PANACEA_ALLOC@YS@@SAPEADV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
        PanaceaFree_t _freePanacea = (PanaceaFree_t)GetProcAddress(MAIN_HANDLE, "?Free@PANACEA_ALLOC@YS@@SAXV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
        PanaceaAllocate_t _allocPanacea = (PanaceaAllocate_t)GetProcAddress(MAIN_HANDLE, "?Allocate@PANACEA_ALLOC@YS@@SAXV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_K@Z");
        ChangeWeapon_t _changeWeapon = (ChangeWeapon_t)GetProcAddress(MAIN_HANDLE, "?ChangeWeapon@PARTY@YS@@SAXPEADH_NH1@Z");

        // Trying to initialize this in OnInit causes moduleInfo to get corrupt. I have no fucking idea why.
        if (!ITEM_COMMIT)
            ITEM_COMMIT = SignatureScan<void(*)()>("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x40\x45\x32", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

        if (!_getPanacea("KEYBLADE_SWITCH"))
        {
            _allocPanacea("KEYBLADE_SWITCH", 0x10);
            _allocPanacea("KEYBLADE_DRIVES", 0x10);
        }

        // If we ain't in the title screen and the Keyblade Switch memory is just initialized:
        if (!*_isTitle && *_getPanacea("KEYBLADE_SWITCH") == 0x00)
        {
            // For the 5 forms the game has:
            for (int i = 0; i < 0x05; i++)
            {
                // Fetch the value from the save data.
                auto _keybladeValue = *reinterpret_cast<uint16_t*>(_saveData + 0xE400 + (0x02 * i));

                // If it ain't zero, add it to the switch memory.
                if (_keybladeValue != 0x00)
                    memcpy(_getPanacea("KEYBLADE_SWITCH") + 0x02 * i, &_keybladeValue, 0x02);

                // If it is, add the "No Keyblade" item to the switch memory.
                else
                    *reinterpret_cast<uint16_t*>(_getPanacea("KEYBLADE_SWITCH") + 0x02 * i) = 0x0310;
            }
        }

        // If we ARE in the title, reset Keyblade Switch memory.
        else if (*_isTitle && *_getPanacea("KEYBLADE_SWITCH") != 0x00)
            fill(_getPanacea("KEYBLADE_SWITCH"), _getPanacea("KEYBLADE_SWITCH") + 0x10, 0x00);


        // Fetch the Camp Menu pointer and check if we are in the correct sub-menus.
        auto _fetchCamp = *reinterpret_cast<char**>(MENU_2DD_POINTERS + 0x30);

        // If the camp options are this way, assume Prepare and Retry was requested.
        // In which case, sync back the arrays.
        if (*_campOptions == 0x0F && *_isMenu && !RETRY_ACCOMMODATED)
        {
            memcpy(_getPanacea("KEYBLADE_SWITCH"), _saveData + 0xE400, 0x10);

            for (int i = 0; i < 5; i++)
                memcpy(_getPanacea("KEYBLADE_DRIVES") + (0x02 * i), _saveData + 0x32BC + 0x38 * (i + 1), 0x02);

            RETRY_ACCOMMODATED = true;
        }

        else if (!*_isMenu)
            RETRY_ACCOMMODATED = false;

        if (_fetchCamp && MENU_MAIN_LABEL.size() == 0x00)
        {
            char _filePath[0x24];

            auto _fetchConfig = *reinterpret_cast<const uint16_t*>(_saveData + 0x41A6);
            auto _regionPointer = (!_getRegion() || _getRegion() == 7) ? reinterpret_cast<char*>(*_regionDefaultPtr) : reinterpret_cast<char*>(*_regionCurrentPtr);

            string _constructMenu = _fetchConfig & 0x0200 ? "menu_2nd/%s/%s" : (_fetchConfig & 0x0400 ? "menu_3rd/%s/%s" : "menu/%s/%s");

            auto _checkPhoto = strstr("camp.2ld", "jm_photo/");
            sprintf(_filePath, _constructMenu.c_str(), _regionPointer, "camp.2ld");

            if (!_getFileSize(_filePath))
            {
                if (_checkPhoto)
                    goto SKIP_PHOTO;

                _constructMenu.resize(_constructMenu.size() - 0x03);
                sprintf(_filePath, _constructMenu.c_str(), "camp.2ld");

                if (!_getFileSize(_filePath))
                {
                SKIP_PHOTO:
                    sprintf(_filePath, "menu/%s/%s", _regionPointer, "camp.2ld");

                    if (!_getFileSize(_filePath) && !_checkPhoto)
                        sprintf(_filePath, "menu/%s", "camp.2ld");
                }
            }

            auto _fetchSize = _getFileSize(_filePath);

            MENU_MAIN_LABEL = RangedMultiScan("\x00\x40\x50\x80\x00\x40\x50\x80", "xxxxxxxx", _fetchCamp, _fetchSize);
            MENU_INSIDE_LABELS = RangedMultiScan("\x00\x32\x32\x80\x00\x32\x32\x80", "xxxxxxxx", _fetchCamp, _fetchSize);
            MENU_FORM_TEXTS = RangedMultiScan("\x00\x9B\x9B\x80\x00\x9B\x9B\x80", "xxxxxxxx", _fetchCamp, _fetchSize);
            MENU_FORM_LABELS = RangedMultiScan("\x00\x3C\x3C\x50\x00\x3C\x3C\x50", "xxxxxxxx", _fetchCamp, _fetchSize);

            for (int i = 0; i < MENU_MAIN_LABEL.size(); i++)
                MENU_MAIN_LABEL[i] = reinterpret_cast<char*>(MENU_MAIN_LABEL[i] - _fetchCamp);

            for (int i = 0; i < MENU_INSIDE_LABELS.size(); i++)
                MENU_INSIDE_LABELS[i] = reinterpret_cast<char*>(MENU_INSIDE_LABELS[i] - _fetchCamp);

            for (int i = 0; i < MENU_FORM_LABELS.size(); i++)
                MENU_FORM_LABELS[i] = reinterpret_cast<char*>(MENU_FORM_LABELS[i] - _fetchCamp);

            for (int i = 0; i < MENU_FORM_TEXTS.size(); i++)
                MENU_FORM_TEXTS[i] = reinterpret_cast<char*>(MENU_FORM_TEXTS[i] - _fetchCamp);
        }

        // If the Camp menu exists and we are in a menu:
        if (_fetchCamp && *_isMenu)
        {
            // Get the sub-menu selection pointer.
            auto _fetchSelect = *reinterpret_cast<uint8_t**>(_subOptionSelectPtr);

            // Calculate the maximum selection we can make.
            auto _calculateForms = _getNumBackyard(0x001A) + _getNumBackyard(0x001D) + _getNumBackyard(0x001F);

            for (int i = 0; i < 5; i++)
            {
                auto _fetchTarget = *reinterpret_cast<uint16_t*>(_getPanacea("KEYBLADE_SWITCH") + (0x02 * i));

                if (_fetchTarget == 0x0310 || _fetchTarget == 0x0000)
                    continue;

                auto _fetchKeyCount = _getNumBackyard(_fetchTarget);

                if (_fetchKeyCount != 0x00)
                    _reduceBackyard(_fetchTarget, _fetchKeyCount);

                auto _fetchFormKey = *reinterpret_cast<uint16_t*>(_saveData + 0x32BC + 0x38 * (i + 1));

                if (_fetchFormKey == 0x00)
                    continue;

                auto _fetchFKeyCount = _getNumBackyard(_fetchFormKey);

                if (_fetchFKeyCount != 0x00)
                    _reduceBackyard(_fetchFormKey, _fetchFKeyCount);
            }

            auto _actualKey = *reinterpret_cast<uint16_t*>(_saveData + 0x24F0);
            auto _fetchActualKeyCount = _getNumBackyard(_actualKey);

            if (_fetchActualKeyCount != 0x00)
                _reduceBackyard(_actualKey, _fetchActualKeyCount);

            // If the pointer exists:
            if (_fetchSelect)
            {
                // If the selection is in one of the weapons, we are in the correct sub-menus, and TRIANGLE is pressed and the colors did not swap yet:
                if (((*_subMenuType == 0x02 && *_fetchSelect <= _calculateForms) || (*_subMenuType == 0x01 && *_fetchSelect == 0x00)) && (*_hardpadInput & 0x0004) && !COLOR_SWAPPED && *CURRENT_SUBMENU == 0x00)
                {
                    // For every form that exists in game:
                    for (int i = 0; i < 5; i++)
                    {
                        // Fetch the form's current keyblade and the target keyblade according to the menu mode.
                        auto _fetchKey = reinterpret_cast<uint16_t*>(_saveData + 0x32BC + 0x38 * (i + 1));
                        auto _fetchTarget = *reinterpret_cast<uint16_t*>(_getPanacea(IS_KEYBLADE_MENU ? "KEYBLADE_DRIVES" : "KEYBLADE_SWITCH") + (0x02 * i));

                        // If the form keyblade is 0x00, the form does not exist or doesn't have a keyblade. Continue.
                        if (*_fetchKey == 0x00)
                            continue;

                        // Get the sub-memory we will put the current keyblade into and commit it.
                        auto _writeTarget = reinterpret_cast<uint16_t*>(_getPanacea(IS_KEYBLADE_MENU ? "KEYBLADE_SWITCH" : "KEYBLADE_DRIVES") + (0x02 * i));
                        *_writeTarget = *_fetchKey;

                        // If the menu is the Keyblade Menu, assume we are committing all Keyblades.
                        if (IS_KEYBLADE_MENU)
                            memcpy(_saveData + 0xE400 + (i * 0x02), _fetchKey, 0x02);

                        // Commit the switch.
                        *_fetchKey = _fetchTarget;

                        auto _currentForm = *(_saveData + 0x3524);

                        // If Sora is in a form and it is the form we currently are handling: Switch that Keyblade.
                        // This to prevent any scenario that the form keyblade will be affected by the switch keyblade memory.
                        if (IS_KEYBLADE_MENU && _currentForm != 0x00 && _currentForm == i + i)
                            _changeWeapon(nullptr, 0x01, true, *_fetchKey, false);
                    }

                    // Declare the colors we will be using.
                    uint64_t _colorMainLabel = !IS_KEYBLADE_MENU ? 0x8000005080000050 : 0x8050400080504000;
                    uint64_t _colorInsideLabel = !IS_KEYBLADE_MENU ? 0x8000003C8000003C : 0x8032320080323200;
                    uint64_t _colorFormLabel = !IS_KEYBLADE_MENU ? 0x0000000000000000 : 0x503C3C00503C3C00;
                    uint64_t _colorFormText = !IS_KEYBLADE_MENU ? 0x0000000000000000 : 0x809B9B00809B9B00;

                    // Commit the colors to all the elements that will be used.

                    for (char* _fetchMenu : MENU_MAIN_LABEL)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchMenu)) = _colorMainLabel;

                    for (auto _fetchRegular : MENU_INSIDE_LABELS)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchRegular)) = _colorInsideLabel;

                    for (auto _fetchLabel : MENU_FORM_LABELS)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchLabel)) = _colorFormLabel;

                    for (auto _fetchText : MENU_FORM_TEXTS)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchText)) = _colorFormText;


                    // Change out what SEQD elements will be used for the in-menu.
                    *reinterpret_cast<uint64_t*>(MENU_SEQUENCE_ARRAY + 0x06) = !IS_KEYBLADE_MENU ? 0xAB00D000D100CF : 0xAB00DA00DB00D9;
                    *reinterpret_cast<uint64_t*>(MENU_SEQUENCE_ARRAY + 0x36) = !IS_KEYBLADE_MENU ? 0xB000D600D700D5 : 0xB000E000E100DF;

                    // Set the bools as necessary.
                    COLOR_SWAPPED = true;
                    IS_KEYBLADE_MENU = !IS_KEYBLADE_MENU;

                    // Commit the changes made to the menu.
                    MENU_COMMIT_FIRST(*MENU_ITEMS, 0x05, 0x00);
                    MENU_COMMIT_SECOND(*MENU_ITEMS, 0x00, 0x00);

                    // If the sub-menu is 0x02, and the selection ain't 0x00: Commit the current item for a refresh.
                    if (*_subMenuType == 0x02 && *_fetchSelect != 0x00)
                        ITEM_COMMIT();

                    // Play the SFX.
                    _playSFX(0x02);
                }

                // Otherwise, put the boolean to false.
                else if (!(*_hardpadInput & 0x0004) && COLOR_SWAPPED)
                    COLOR_SWAPPED = false;

                // If we are on sub-menu 0x02:
                if (*_subMenuType == 0x02)
                {
                    // Kill the "No Keyblade" description.
                    *_getData(0x5761) = 0x00;

                    // If our selection is within the keyblades and the loaded picture is "No Keyblade", or the selection is 0x00, or we don't have the Keyblade Menu active: Remove "No Keyblade" from the inventory.
                    if ((*_fetchSelect <= _calculateForms && *_loadedItempic == 425) || *_fetchSelect == 0x00 || !IS_KEYBLADE_MENU)
                        *(_saveData + 0x36BA) = 0x00;

                    // Otherwise, force it to 1.
                    else
                        *(_saveData + 0x36BA) = 0x01;
                }

                // Otherwise, restore the "No Keyblade" description.
                else
                    *_getData(0x5761) = 0x03;
            }
        }

        // If we have the Keyblade Menu active but out of the menu:
        else if (IS_KEYBLADE_MENU && !*_isMenu)
        {
            // For all forms the game has:
            for (int i = 0; i < 5; i++)
            {
                // Fetch the current form key as well as the target key.
                auto _fetchKey = reinterpret_cast<uint16_t*>(_saveData + 0x32BC + 0x38 * (i + 1));
                auto _fetchTarget = *reinterpret_cast<uint16_t*>(_getPanacea("KEYBLADE_DRIVES") + (0x02 * i));

                // If the form key is 0x00, it doesn't exist or doesn't have a key. Continue.
                if (*_fetchKey == 0x00)
                    continue;

                // Get the memory we will write the current keyblade to.
                auto _writeTarget = reinterpret_cast<uint16_t*>(_getPanacea("KEYBLADE_SWITCH") + (0x02 * i));

                // Write the current keyblade to both the switch memory and the save file, committing it.
                *_writeTarget = *_fetchKey;
                memcpy(_saveData + 0xE400 + (i * 0x02), _fetchKey, 0x02);

                // Overwrite the form keyblade to what it is supposed to be.
                *_fetchKey = _fetchTarget;

                auto _currentForm = *(_saveData + 0x3524);

                // If Sora is in a form and it is the form we currently are handling: Switch that Keyblade.
                // This to prevent any scenario that the form keyblade will be affected by the switch keyblade memory.
                if (_currentForm != 0x00 && _currentForm == i + 1)
                    _changeWeapon(nullptr, 0x01, true, *_fetchKey, false);

                // If the camp menu pointer exists, revert all the colors and SEQD elements changed.
                if (_fetchCamp)
                {
                    for (char* _fetchMenu : MENU_MAIN_LABEL)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchMenu)) = 0x8050400080504000;

                    for (auto _fetchRegular : MENU_INSIDE_LABELS)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchRegular)) = 0x8032320080323200;

                    for (auto _fetchLabel : MENU_FORM_LABELS)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchLabel)) = 0x503C3C00503C3C00;

                    for (auto _fetchText : MENU_FORM_TEXTS)
                        *reinterpret_cast<uint64_t*>(_fetchCamp + reinterpret_cast<int>(_fetchText)) = 0x809B9B00809B9B00;

                    *reinterpret_cast<uint64_t*>(MENU_SEQUENCE_ARRAY + 0x06) = 0xAB00DA00DB00D9;
                    *reinterpret_cast<uint64_t*>(MENU_SEQUENCE_ARRAY + 0x36) = 0xB000E000E100DF;
                }

                // Set the booleans.
                COLOR_SWAPPED = false;
                IS_KEYBLADE_MENU = false;
            }
        }

        // See if there is specifically a Cutscene playing.
        auto _eventPointer = *reinterpret_cast<const char**>(_eventInfoPtr);

        bool _isCutscene = _eventPointer != 0x00 && *reinterpret_cast<const uint32_t*>(_eventPointer + 0x04) != 0xCAFEEFAC &&
                                                    *reinterpret_cast<const uint32_t*>(_eventPointer + 0x04) != 0xEFACCAFE;

        // If we ain't in the title, ain't in a menu, and are in a map:
        if (!*_isTitle && !*_isMenu && *_isInMap && !_isCutscene && *(_saveData + 0x3524) <= 0x05 && !*BTLEND_FLAG)
        {
            auto _fetchSora = *reinterpret_cast<uint64_t*>(_fetchSoraPtr);

            if (_fetchSora)
            { 
                auto _fetchFlag = *reinterpret_cast<uint8_t*>(_fetchSora + 0x127);

                // If the input is L1 + RIGHT and the debounce isn't set:
                if (!KEYBLADE_DEBOUNCE && (_fetchFlag & 0x10) != 0x10)
                {
                    if ((*_hardpadInput & 0x0020) == 0x0020 || (*_hardpadInput & 0x0080) == 0x0080)
                    {
                        bool _secondHand = false;

                        // Get the Keyblade Pointers.
                        uint16_t* _fetchKeyblade = nullptr;
                        uint16_t* _currentKeyblade = reinterpret_cast<uint16_t*>(_saveData + 0x24F0);

                        uint16_t _liveKeyblade = 0x0000;
                        uint16_t _targetKeyblade = 0x0000;

                        int _keyIterator = 0x00;

                        if ((*_hardpadInput & 0x0080) == 0x0080)
                        {
                            if (*(_saveData + 0x3524) != 0x00)
                            {
                                _secondHand = true;

                                auto _fetchForm = *(_saveData + 0x3524) - 0x01;
                                _currentKeyblade = reinterpret_cast<uint16_t*>(_saveData + 0x32F4 + (0x38 * _fetchForm));
                            }

                            else
                                goto END_KEYBLADE;
                        }

                        // Unless broken;
                        while (true)
                        {
                            // Get the target Keyblade from the save.
                            _fetchKeyblade = reinterpret_cast<uint16_t*>(_saveData + 0xE400 + (0x02 * (TARGET_KEY + _keyIterator)));

                            // If it isn't null or no keyblade:
                            if (*_fetchKeyblade == 0x0000 || *_fetchKeyblade == 0x0310)
                            {
                                // If the temporary or the paermanent iterator is equal to or greater than 6:
                                if (_keyIterator >= 0x06 || TARGET_KEY >= 0x06)
                                {
                                    // If the TARGET_KEY isn't reset:
                                    if (TARGET_KEY != 0x00)
                                    {
                                        // Reset everything and restart the loop.
                                        TARGET_KEY = 0x00;
                                        _keyIterator = 0x00;

                                        continue;
                                    }

                                    // If it is, either there is no valid Keyblade or something is horribly wrong! Abort immediately!
                                    else
                                        goto END_KEYBLADE;

                                }

                                // Increase the iterator and restart the loop.
                                _keyIterator++;
                                continue;
                            }

                            // Get the target Keyblades.
                            _targetKeyblade = *_fetchKeyblade;
                            _liveKeyblade = *_currentKeyblade;

                            // Increase TARGET_KEY by the temporary iteerator.
                            TARGET_KEY += _keyIterator;

                            // Commit the switch in memory.
                            memcpy(_saveData + 0xE400 + 0x02 * TARGET_KEY, &_liveKeyblade, 0x02);
                            memcpy(_getPanacea("KEYBLADE_SWITCH") + 0x02 * TARGET_KEY, &_liveKeyblade, 0x02);

                            *_currentKeyblade = _targetKeyblade;

                            // Increase the permanent iterator and break out.
                            TARGET_KEY++;
                            break;
                        }

                        // Commit the Keyblade Switch.
                        _changeWeapon(nullptr, 0x01, _secondHand, _targetKeyblade, true);

                        // Set the debounce.
                    END_KEYBLADE:
                        KEYBLADE_DEBOUNCE = true;
                    }
                }

                // Otherwise, unset the debounce.
                else if ((*_hardpadInput & 0x0020) != 0x0020 && (*_hardpadInput & 0x0080) != 0x0080 && KEYBLADE_DEBOUNCE)
                    KEYBLADE_DEBOUNCE = false;
            }
        }
    }
}
