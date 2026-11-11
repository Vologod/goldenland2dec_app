#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr uint32_t kLevelManagerRva = 0x88dc4;
constexpr uint32_t kItemManagerRva = 0x88df8;
constexpr uint32_t kEntityArrayOffset = 0x2564;
constexpr uint32_t kEntityBitmapOffset = 0x2568;
constexpr uint32_t kEntityCountOffset = 0x256c;
constexpr uint32_t kEntityTypeOffset = 0x14;
constexpr uint32_t kEntityNameOffset = 0x4c;
constexpr uint32_t kEntityMainContainerOffset = 0x10;
constexpr uint32_t kNpcSpawnRva = 0x4b1c0;
constexpr uint32_t kRandBeforeDrawRva = 0x6c0a4;
constexpr uint32_t kPriceCallRva = 0x6e8d0;
constexpr uint32_t kLootLevelOffset = 0x26c4;
constexpr uint32_t kNpcSpawnHotStackOffset = 4;
constexpr uint32_t kNpcSpawnMainStackOffset = 8;
constexpr uint32_t kNpcSpawnInventoryStackOffset = 0xc;
constexpr std::size_t kMaxCallTrace = 512;
constexpr std::size_t kMaxDrawTrace = 4096;
constexpr uint32_t kManagerPointersOffset = 0x18;
constexpr uint32_t kManagerCountOffset = 0x1c;
constexpr uint32_t kManagerBitmapOffset = 0x20;
constexpr uint32_t kContainerMapCountOffset = 0x0c;
constexpr uint32_t kContainerMapEntriesOffset = 0x10;
constexpr uint32_t kContainerIdsOffset = 0x0c;
constexpr uint32_t kContainerSlotsOffset = 0x10;
constexpr uint32_t kContainerBitmapOffset = 0x14;
constexpr uint32_t kItemFlagsOffset = 0x0c;
constexpr uint32_t kItemUidOffset = 0xa4;
constexpr uint32_t kItemIdOffset = 0xa8;
constexpr uint32_t kItemCountOffset = 0xb0;
constexpr uint32_t kItemModCountOffset = 0x78;
constexpr uint32_t kItemModArrayOffset = 0x7c;
constexpr uint32_t kModStride = 0x14;

constexpr uint32_t kMaxEntityCount = 1000000;
constexpr uint32_t kMaxManagerCount = 1000000;
constexpr uint32_t kMaxContainerSlots = 65536;
constexpr uint32_t kMaxMods = 4096;
constexpr std::size_t kMaxNameBytes = 192;
constexpr int kDefaultTimeoutSeconds = 120;
constexpr int kScanSettleMs = 15000;
constexpr int kScanPollMs = 100;
constexpr int kScanStablePolls = 5;
constexpr std::size_t kMaxWarnings = 80;

struct Options {
    DWORD pid = 0;
    std::wstring level;
    int loot_level = 0;
    std::wstring out_path;
    int timeout_seconds = kDefaultTimeoutSeconds;
    std::wstring server_dll = L"Server.dll";
    std::wstring client_dll = L"Client.dll";
    bool scan_only = false;
    bool price_cw = false;
    int trace_calls_ms = 0;
};

struct ModuleInfo {
    uint32_t base = 0;
    uint32_t size = 0;
    std::wstring path;
};

struct ModRow {
    int32_t id = 0;
    int32_t value = 0;
    uint32_t flags = 0;
};

struct ItemRow {
    std::string person;
    uint32_t item_id = 0;
    int32_t count = 0;
    uint32_t item_flags = 0;
    std::vector<ModRow> mods;
};

struct ScanResult {
    std::vector<ItemRow> items;
    std::vector<std::string> warnings;
    std::size_t total_mods = 0;
    std::size_t entity_count = 0;
    std::size_t person_count = 0;
    bool level_read = false;
    bool manager_read = false;
    bool complete = false;
    std::optional<uint32_t> loot_level;
};

struct ThreadSlot {
    HANDLE handle = nullptr;
    CONTEXT original{};
    bool have_original = false;
    bool armed = false;
};

struct DrawTrace {
    uint32_t ret = 0;
    uint32_t state = 0;
};

struct CallTrace {
    uint32_t index = 0;
    DWORD tid = 0;
    uint32_t hot = 0;
    uint32_t main = 0;
    std::string inventory;
    uint32_t draws = 0;
    std::vector<DrawTrace> draw_returns;
};

struct CaptureState {
    uint32_t bp0 = 0;
    uint32_t bp1 = 0;
    bool generation_active = false;
    bool state_captured = false;
    bool capture_failed = false;
    bool process_exited = false;
    DWORD generation_tid = 0;
    uint32_t rng_state = 0;
    std::optional<uint16_t> entry_cw;
    std::optional<uint16_t> draw_cw;
    std::vector<CallTrace> calls;
    uint32_t second_rng_state = 0;
    bool second_draw_seen = false;
    bool second_draw_valid = false;
};

std::string to_utf8(std::wstring_view text) {
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::string cp1251_to_utf8(std::string_view text) {
    bool ascii = true;
    for (const char ch : text) {
        if (static_cast<unsigned char>(ch) >= 0x80) {
            ascii = false;
            break;
        }
    }
    if (ascii)
        return std::string(text);
    const int wide_size =
        MultiByteToWideChar(1251, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wide_size <= 0)
        return std::string(text);
    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    MultiByteToWideChar(1251, 0, text.data(), static_cast<int>(text.size()), wide.data(), wide_size);
    return to_utf8(wide);
}

std::string wide_error(DWORD code) {
    wchar_t *buffer = nullptr;
    const DWORD length =
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr)
        return "win32 error " + std::to_string(code);
    std::wstring text(buffer, length);
    LocalFree(buffer);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
        text.pop_back();
    return to_utf8(text) + " (" + std::to_string(code) + ")";
}

void log_error(const std::string &message) {
    std::fprintf(stderr, "gl_live_capture: error: %s\n", message.c_str());
}

void log_warn(const std::string &message) {
    std::fprintf(stderr, "gl_live_capture: warning: %s\n", message.c_str());
}

void log_info(const std::string &message) {
    std::fprintf(stdout, "gl_live_capture: %s\n", message.c_str());
    std::fflush(stdout);
}

std::wstring lower(std::wstring text) {
    for (wchar_t &ch : text)
        ch = static_cast<wchar_t>(towlower(ch));
    return text;
}

std::wstring file_name(const std::wstring &path) {
    const std::size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

bool parse_u32(const std::wstring &text, uint32_t &out) {
    if (text.empty() || text.front() == L'-')
        return false;
    wchar_t *end = nullptr;
    errno = 0;
    const unsigned long long value = std::wcstoull(text.c_str(), &end, 0);
    if (errno != 0 || end == text.c_str() || *end != L'\0' || value > 0xffffffffull)
        return false;
    out = static_cast<uint32_t>(value);
    return true;
}

bool parse_i32(const std::wstring &text, int32_t &out) {
    if (text.empty())
        return false;
    wchar_t *end = nullptr;
    errno = 0;
    const long long value = std::wcstoll(text.c_str(), &end, 0);
    if (errno != 0 || end == text.c_str() || *end != L'\0' || value < -2147483648ll ||
        value > 2147483647ll)
        return false;
    out = static_cast<int32_t>(value);
    return true;
}

void print_usage() {
    std::printf(
        "gl_live_capture - read-only 32-bit live capture for GoldenLand 2 specials\n"
        "\n"
        "Usage:\n"
        "  gl_live_capture.exe --pid <pid> --level <id> --loot-level <n> --out <file>\n"
        "                      [--timeout <sec>] [--server <dll>] [--scan-only]\n"
        "  gl_live_capture.exe --price-cw --pid <pid> --out <file>\n"
        "                      [--client <dll>] [--timeout <sec>]\n"
        "\n"
        "Options:\n"
        "  --pid <pid>          PID of the running 32-bit GoldenLand.exe\n"
        "  --level <id>         Level id written to the capture (for example l1_1)\n"
        "  --loot-level <n>     Positive loot level passed to NPC generation\n"
        "  --out <file>         Output capture file (UTF-8, gl-specials-live-2)\n"
        "  --timeout <sec>      Wait for level load / first NPC draw (default %d)\n"
        "  --server <dll>       Server module name or path (default Server.dll)\n"
        "  --client <dll>       Client module name or path (default Client.dll)\n"
        "  --trace-calls <ms>   keep logging NPC spawn calls for <ms> after attach\n"
        "  --scan-only          Scan an already loaded level without breakpoints\n"
        "  --price-cw           Capture the client price x87 control word only\n"
        "  --help, -h           Show this help\n"
        "\n"
        "The tool attaches with DebugActiveProcess, installs hardware execute\n"
        "breakpoints on the NPC spawn entry and the pre-draw _rand site, then\n"
        "detaches and reads NPC main containers with PROCESS_VM_READ. It never\n"
        "uses WriteProcessMemory, remote threads, code patching, injection or DRM\n"
        "bypasses. Start it before loading a level so the first generation draw can\n"
        "be captured.\n"
        "\n"
        "With --price-cw the tool attaches only to Client.dll+0x6e8d0 (the price\n"
        "call on the trade/inventory path), reads the ambient x87 control word at\n"
        "the first hit and writes 'price=0x....' plus '# source=client-price'. It\n"
        "does not scan containers and does not require --level or --loot-level.\n"
        "The expected value is 0x007f (PC=24, RC=nearest).\n"
        "\n"
        "Exit codes: 0 capture written, 1 usage error, 2 attach/module error,\n"
        "3 timeout or RNG state not captured (partial file), 4 no NPC items found\n"
        "(partial file), 5 write error, 6 process exited.\n",
        kDefaultTimeoutSeconds);
}

bool parse_args(int argc, wchar_t **argv, Options &options, std::string &error) {
    bool have_pid = false;
    bool have_level = false;
    bool have_loot = false;
    bool have_out = false;
    for (int index = 1; index < argc; ++index) {
        std::wstring key = argv[index];
        std::wstring value;
        const std::size_t equals = key.find(L'=');
        if (equals != std::wstring::npos) {
            value = key.substr(equals + 1);
            key.resize(equals);
        }
        auto take_value = [&]() -> bool {
            if (!value.empty())
                return true;
            if (index + 1 >= argc) {
                error = "missing value for " + to_utf8(key);
                return false;
            }
            value = argv[++index];
            return true;
        };
        if (key == L"--help" || key == L"-h") {
            print_usage();
            std::exit(0);
        } else if (key == L"--pid") {
            if (!take_value())
                return false;
            uint32_t pid = 0;
            if (!parse_u32(value, pid) || pid == 0) {
                error = "invalid --pid";
                return false;
            }
            options.pid = static_cast<DWORD>(pid);
            have_pid = true;
        } else if (key == L"--level") {
            if (!take_value())
                return false;
            if (value.empty() || value.find_first_of(L"\r\n|") != std::wstring::npos) {
                error = "invalid --level";
                return false;
            }
            options.level = value;
            have_level = true;
        } else if (key == L"--loot-level") {
            if (!take_value())
                return false;
            if (!parse_i32(value, options.loot_level) || options.loot_level <= 0) {
                error = "invalid --loot-level (must be positive)";
                return false;
            }
            have_loot = true;
        } else if (key == L"--out") {
            if (!take_value())
                return false;
            if (value.empty()) {
                error = "invalid --out";
                return false;
            }
            options.out_path = value;
            have_out = true;
        } else if (key == L"--timeout") {
            if (!take_value())
                return false;
            int32_t seconds = 0;
            if (!parse_i32(value, seconds) || seconds <= 0 || seconds > 86400) {
                error = "invalid --timeout";
                return false;
            }
            options.timeout_seconds = seconds;
        } else if (key == L"--server") {
            if (!take_value())
                return false;
            if (value.empty()) {
                error = "invalid --server";
                return false;
            }
            options.server_dll = value;
        } else if (key == L"--client") {
            if (!take_value())
                return false;
            if (value.empty()) {
                error = "invalid --client";
                return false;
            }
            options.client_dll = value;
        } else if (key == L"--price-cw") {
            options.price_cw = true;
        } else if (key == L"--trace-calls") {
            if (!take_value())
                return false;
            int32_t milliseconds = 0;
            if (!parse_i32(value, milliseconds) || milliseconds < 0 ||
                milliseconds > 600000) {
                error = "invalid --trace-calls";
                return false;
            }
            options.trace_calls_ms = milliseconds;
        } else if (key == L"--scan-only") {
            options.scan_only = true;
        } else {
            error = "unknown argument " + to_utf8(key);
            return false;
        }
    }
    if (!have_pid) {
        error = "--pid is required";
        return false;
    }
    if (options.price_cw) {
        if (!have_out) {
            error = "--out is required with --price-cw";
            return false;
        }
        if (options.scan_only) {
            error = "--price-cw cannot be combined with --scan-only";
            return false;
        }
        return true;
    }
    if (!options.scan_only && (!have_level || !have_loot || !have_out)) {
        error = "--level, --loot-level and --out are required (unless --scan-only)";
        return false;
    }
    return true;
}

bool enable_debug_privilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;
    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &privileges.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    SetLastError(ERROR_SUCCESS);
    const BOOL ok = AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
    const DWORD error = GetLastError();
    CloseHandle(token);
    return ok && error == ERROR_SUCCESS;
}

std::vector<DWORD> list_threads(DWORD pid) {
    std::vector<DWORD> result;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return result;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == pid)
                result.push_back(entry.th32ThreadID);
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

bool find_module(DWORD pid, const std::wstring &wanted, ModuleInfo &out, std::string &error) {
    const std::wstring wanted_name = lower(file_name(wanted));
    const std::wstring wanted_path = lower(wanted);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        error = "cannot enumerate modules: " + wide_error(GetLastError());
        return false;
    }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const std::wstring name = lower(entry.szModule);
            const std::wstring path = lower(entry.szExePath);
            if (name == wanted_name || path == wanted_path ||
                file_name(path) == wanted_name) {
                out.base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry.modBaseAddr));
                out.size = entry.modBaseSize;
                out.path = entry.szExePath;
                found = true;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    if (!found) {
        error = "module '" + to_utf8(wanted) + "' not found in PID " + std::to_string(pid);
        return false;
    }
    if (out.base == 0 || out.size == 0) {
        error = "module '" + to_utf8(wanted) + "' has an empty base/size";
        return false;
    }
    return true;
}

bool module_range_ok(const ModuleInfo &module, uint32_t rva, std::size_t length) {
    const uint64_t start = static_cast<uint64_t>(module.base) + rva;
    const uint64_t end = start + length;
    return start >= module.base && end <= static_cast<uint64_t>(module.base) + module.size;
}

class MemReader {
  public:
    explicit MemReader(HANDLE process) : process_(process) {}

    bool read(uint32_t address, void *buffer, std::size_t size) const {
        if (process_ == nullptr || address == 0 || size == 0)
            return false;
        SIZE_T done = 0;
        const BOOL ok = ReadProcessMemory(process_,
                                          reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)),
                                          buffer, size, &done);
        return ok != FALSE && done == size;
    }

    bool u32(uint32_t address, uint32_t &value) const {
        return read(address, &value, sizeof(value));
    }

    bool i32(uint32_t address, int32_t &value) const {
        return read(address, &value, sizeof(value));
    }

    bool byte(uint32_t address, uint8_t &value) const {
        return read(address, &value, sizeof(value));
    }

    bool cstring(uint32_t address, std::size_t max_size, std::string &out) const {
        std::vector<char> buffer(max_size);
        if (!read(address, buffer.data(), buffer.size()))
            return false;
        const auto end = std::find(buffer.begin(), buffer.end(), '\0');
        out.assign(buffer.begin(), end);
        return end != buffer.end();
    }

  private:
    HANDLE process_ = nullptr;
};

bool bit_set(const MemReader &mem, uint32_t bitmap, uint32_t index) {
    if (bitmap == 0)
        return false;
    uint8_t byte = 0;
    if (!mem.byte(bitmap + (index >> 3), byte))
        return false;
    return (byte & static_cast<uint8_t>(1u << (index & 7))) != 0;
}

std::string sanitize_name(std::string name) {
    for (char &ch : name) {
        if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t')
            ch = '_';
    }
    return name;
}

void add_warning(ScanResult &result, const std::string &message) {
    if (result.warnings.size() < kMaxWarnings)
        result.warnings.push_back(message);
}

class Debugger {
  public:
    Debugger(DWORD pid, uint32_t bp0, uint32_t bp1) : pid_(pid), bp0_(bp0), bp1_(bp1) {}

    ~Debugger() { restore_and_detach(); }

    bool attach(std::string &error) {
        if (!DebugActiveProcess(pid_)) {
            error = "DebugActiveProcess failed: " + wide_error(GetLastError());
            return false;
        }
        attached_ = true;
        if (!DebugSetProcessKillOnExit(FALSE)) {
            error = "DebugSetProcessKillOnExit failed: " + wide_error(GetLastError());
            return false;
        }
        return true;
    }

    bool arm_all_threads(std::string &error) {
        const std::vector<DWORD> threads = list_threads(pid_);
        if (threads.empty()) {
            error = "Toolhelp reported no threads for PID " + std::to_string(pid_);
            return false;
        }
        bool any = false;
        for (const DWORD tid : threads) {
            if (ensure_thread(tid, nullptr))
                any = true;
        }
        if (!any) {
            error = "could not arm any thread with hardware breakpoints";
            return false;
        }
        return true;
    }

    bool ensure_thread(DWORD tid, HANDLE event_handle) {
        const auto existing = threads_.find(tid);
        if (existing != threads_.end()) {
            if (event_handle != nullptr)
                CloseHandle(event_handle);
            return existing->second.armed;
        }
        HANDLE handle = event_handle;
        if (handle == nullptr) {
            handle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME |
                                    THREAD_QUERY_INFORMATION,
                                FALSE, tid);
        }
        if (handle == nullptr) {
            log_warn("cannot open thread " + std::to_string(tid) + ": " + wide_error(GetLastError()));
            return false;
        }
        ThreadSlot slot;
        slot.handle = handle;
        CONTEXT context{};
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(handle, &context)) {
            log_warn("cannot read thread context " + std::to_string(tid) + ": " +
                     wide_error(GetLastError()));
            threads_.emplace(tid, slot);
            return false;
        }
        slot.original = context;
        slot.have_original = true;
        context.Dr0 = bp0_;
        context.Dr1 = 0;
        context.Dr2 = 0;
        context.Dr3 = 0;
        context.Dr7 = (context.Dr7 & ~0x00fc00ffu) | 0x00000001u;
        if (!SetThreadContext(handle, &context)) {
            log_warn("cannot arm thread " + std::to_string(tid) + ": " + wide_error(GetLastError()));
            threads_.emplace(tid, slot);
            return false;
        }
        slot.armed = true;
        threads_.emplace(tid, slot);
        CONTEXT readback{};
        readback.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(handle, &readback)) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "armed tid=%lu dr0=0x%08lx dr7=0x%08lx",
                          static_cast<unsigned long>(tid),
                          static_cast<unsigned long>(readback.Dr0),
                          static_cast<unsigned long>(readback.Dr7));
            log_info(buffer);
        }
        return true;
    }

    bool get_context(DWORD tid, CONTEXT &context) const {
        const auto found = threads_.find(tid);
        if (found == threads_.end() || found->second.handle == nullptr)
            return false;
        context = {};
        context.ContextFlags =
            CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS;
        return GetThreadContext(found->second.handle, &context) != FALSE;
    }

    void arm_dr1(DWORD tid) { modify_context(tid, [this](CONTEXT &context) {
                                  context.Dr1 = bp1_;
                                  context.Dr6 = 0;
                                  context.Dr7 = (context.Dr7 & ~0x00f00000u) | 0x00000004u;
                              }); }

    void set_resume_flag(DWORD tid) {
        const auto found = threads_.find(tid);
        if (found == threads_.end() || found->second.handle == nullptr)
            return;
        CONTEXT context{};
        context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(found->second.handle, &context)) {
            log_warn("cannot read context to set the resume flag on thread " +
                     std::to_string(tid) + ": " + wide_error(GetLastError()));
            return;
        }
        context.EFlags |= 0x10000u;
        if (!SetThreadContext(found->second.handle, &context)) {
            log_warn("cannot set the resume flag on thread " + std::to_string(tid) + ": " +
                     wide_error(GetLastError()));
        }
    }

    void clear_dr0(DWORD tid) { modify_context(tid, [](CONTEXT &context) {
                                   context.Dr0 = 0;
                                   context.Dr6 = 0;
                                   context.Dr7 &= ~0x00000001u;
                                   context.Dr7 &= ~0x000f0000u;
                               }); }

    void clear_dr1(DWORD tid) { modify_context(tid, [](CONTEXT &context) {
                                   context.Dr1 = 0;
                                   context.Dr6 = 0;
                                   context.Dr7 &= ~0x00000004u;
                                   context.Dr7 &= ~0x00f00000u;
                               }); }

    void close_thread(DWORD tid) {
        const auto found = threads_.find(tid);
        if (found == threads_.end())
            return;
        if (found->second.handle != nullptr)
            CloseHandle(found->second.handle);
        threads_.erase(found);
    }

    void restore_and_detach() {
        if (!attached_ && threads_.empty())
            return;
        for (auto &[tid, slot] : threads_) {
            (void)tid;
            if (slot.handle == nullptr)
                continue;
            const DWORD previous = SuspendThread(slot.handle);
            if (slot.have_original) {
                CONTEXT context{};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(slot.handle, &context)) {
                    context.Dr0 = slot.original.Dr0;
                    context.Dr1 = slot.original.Dr1;
                    context.Dr2 = slot.original.Dr2;
                    context.Dr3 = slot.original.Dr3;
                    context.Dr6 = slot.original.Dr6;
                    context.Dr7 = slot.original.Dr7;
                    if (!SetThreadContext(slot.handle, &context)) {
                        log_warn("cannot restore debug registers for thread " +
                                 std::to_string(tid) + ": " + wide_error(GetLastError()));
                    }
                }
            }
            if (previous != static_cast<DWORD>(-1))
                ResumeThread(slot.handle);
        }
        if (attached_) {
            BOOL stopped = DebugActiveProcessStop(pid_);
            if (!stopped) {
                const DWORD first_error = GetLastError();
                Sleep(20);
                stopped = DebugActiveProcessStop(pid_);
                if (!stopped) {
                    log_warn("DebugActiveProcessStop failed: " + wide_error(first_error) +
                             "; the target is detached when this tool exits");
                }
            }
            attached_ = false;
        }
        for (auto &[tid, slot] : threads_) {
            (void)tid;
            if (slot.handle != nullptr)
                CloseHandle(slot.handle);
        }
        threads_.clear();
    }

  private:
    template <typename Fn> void modify_context(DWORD tid, Fn fn) {
        const auto found = threads_.find(tid);
        if (found == threads_.end() || found->second.handle == nullptr)
            return;
        CONTEXT context{};
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(found->second.handle, &context))
            return;
        fn(context);
        if (!SetThreadContext(found->second.handle, &context)) {
            log_warn("cannot update debug registers for thread " + std::to_string(tid) + ": " +
                     wide_error(GetLastError()));
        }
    }

    DWORD pid_ = 0;
    uint32_t bp0_ = 0;
    uint32_t bp1_ = 0;
    bool attached_ = false;
    std::map<DWORD, ThreadSlot> threads_;
};

std::optional<uint16_t> read_thread_control_word(DWORD tid) {
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME,
                               FALSE, tid);
    if (thread == nullptr)
        return std::nullopt;
    std::optional<uint16_t> result;
    const DWORD previous = SuspendThread(thread);
    CONTEXT context{};
    context.ContextFlags = CONTEXT_FLOATING_POINT;
    if (GetThreadContext(thread, &context))
        result = static_cast<uint16_t>(context.FloatSave.ControlWord & 0xffffu);
    if (previous != static_cast<DWORD>(-1))
        ResumeThread(thread);
    CloseHandle(thread);
    return result;
}

std::optional<uint16_t> scan_control_word_for_pid(DWORD pid, DWORD preferred_tid) {
    if (preferred_tid != 0) {
        if (const auto value = read_thread_control_word(preferred_tid))
            return value;
    }
    for (const DWORD tid : list_threads(pid)) {
        if (const auto value = read_thread_control_word(tid))
            return value;
    }
    return std::nullopt;
}

bool resolve_container(const MemReader &mem, uint32_t manager, uint32_t container_id,
                       uint32_t &container, std::string &error) {
    uint32_t map_count = 0;
    uint32_t map_entries = 0;
    if (!mem.u32(manager + kContainerMapCountOffset, map_count) ||
        !mem.u32(manager + kContainerMapEntriesOffset, map_entries) || map_entries == 0) {
        error = "item manager container map is unreadable";
        return false;
    }
    if (map_count > kMaxManagerCount) {
        error = "implausible container map count " + std::to_string(map_count);
        return false;
    }
    for (uint32_t index = 0; index < map_count; ++index) {
        uint32_t key = 0;
        uint32_t value = 0;
        if (!mem.u32(map_entries + index * 8, key) || !mem.u32(map_entries + index * 8 + 4, value))
            continue;
        if (key != container_id)
            continue;
        if (value == 0) {
            error = "null container pointer for id " + std::to_string(container_id);
            return false;
        }
        container = value;
        return true;
    }
    error = "container id " + std::to_string(container_id) + " not found in item manager map";
    return false;
}

bool scan_once(const MemReader &mem, const ModuleInfo &server, ScanResult &result) {
    result = ScanResult{};
    if (!module_range_ok(server, kLevelManagerRva, 4) || !module_range_ok(server, kItemManagerRva, 4)) {
        add_warning(result, "Server.dll is too small for the documented data RVAs");
        return false;
    }
    uint32_t level_ptr = 0;
    if (!mem.u32(server.base + kLevelManagerRva, level_ptr) || level_ptr == 0) {
        add_warning(result, "level manager pointer is null or unreadable");
        return false;
    }
    result.level_read = true;
    uint32_t raw_loot_level = 0;
    if (mem.u32(level_ptr + kLootLevelOffset, raw_loot_level) &&
        raw_loot_level < 1000000u)
        result.loot_level = raw_loot_level + 1;
    uint32_t entities = 0;
    uint32_t entity_bitmap_ptr = 0;
    uint32_t entity_bitmap = 0;
    uint32_t entity_count = 0;
    if (!mem.u32(level_ptr + kEntityArrayOffset, entities) ||
        !mem.u32(level_ptr + kEntityBitmapOffset, entity_bitmap_ptr) ||
        !mem.u32(level_ptr + kEntityCountOffset, entity_count) ||
        !mem.u32(entity_bitmap_ptr, entity_bitmap)) {
        add_warning(result, "cannot read the level entity table");
        return false;
    }
    if (entity_count > kMaxEntityCount) {
        add_warning(result, "implausible entity count " + std::to_string(entity_count));
        return false;
    }
    result.entity_count = entity_count;
    if (entity_count == 0) {
        add_warning(result, "level entity count is zero");
        return false;
    }

    uint32_t manager = 0;
    if (!mem.u32(server.base + kItemManagerRva, manager) || manager == 0) {
        add_warning(result, "item manager pointer is null or unreadable");
        return false;
    }
    uint32_t manager_count = 0;
    uint32_t manager_bitmap = 0;
    if (!mem.u32(manager + kManagerCountOffset, manager_count) ||
        !mem.u32(manager + kManagerBitmapOffset, manager_bitmap)) {
        add_warning(result, "cannot read the item manager header");
        return false;
    }
    if (manager_count > kMaxManagerCount) {
        add_warning(result, "implausible item manager count " + std::to_string(manager_count));
        return false;
    }
    result.manager_read = true;

    std::vector<std::string> seen_names;
    seen_names.reserve(64);
    for (uint32_t index = 0; index < entity_count; ++index) {
        if (!bit_set(mem, entity_bitmap, index))
            continue;
        uint32_t entity = 0;
        if (!mem.u32(entities + index * 4, entity) || entity == 0) {
            add_warning(result, "null entity pointer at slot " + std::to_string(index));
            continue;
        }
        uint32_t type = 0;
        if (!mem.u32(entity + kEntityTypeOffset, type))
            continue;
        if ((type & 0x3eu) != 2u)
            continue;
        std::string raw_name;
        if (!mem.cstring(entity + kEntityNameOffset, kMaxNameBytes, raw_name) || raw_name.empty()) {
            add_warning(result, "invalid EF_PERSON name at entity 0x" + std::to_string(entity));
            continue;
        }
        const std::string name = sanitize_name(cp1251_to_utf8(raw_name));
        if (name.empty()) {
            add_warning(result, "empty EF_PERSON name at entity 0x" + std::to_string(entity));
            continue;
        }
        if (name == "Hero")
            continue;
        if (std::find(seen_names.begin(), seen_names.end(), name) != seen_names.end()) {
            add_warning(result, "duplicate person '" + name + "' skipped (first wins)");
            continue;
        }
        seen_names.push_back(name);
        ++result.person_count;

        uint32_t container_id = 0;
        if (!mem.u32(entity + kEntityMainContainerOffset, container_id)) {
            add_warning(result, "cannot read main container id for '" + name + "'");
            continue;
        }
        uint32_t container = 0;
        std::string error;
        if (!resolve_container(mem, manager, container_id, container, error)) {
            add_warning(result, "'" + name + "': " + error);
            continue;
        }
        uint32_t ids = 0;
        uint32_t slots = 0;
        uint32_t container_bitmap = 0;
        if (!mem.u32(container + kContainerIdsOffset, ids) ||
            !mem.u32(container + kContainerSlotsOffset, slots) ||
            !mem.u32(container + kContainerBitmapOffset, container_bitmap)) {
            add_warning(result, "'" + name + "': cannot read container header");
            continue;
        }
        if (slots > kMaxContainerSlots) {
            add_warning(result, "'" + name + "': implausible slot count " + std::to_string(slots));
            continue;
        }
        uint32_t record_pointers = 0;
        if (!mem.u32(manager + kManagerPointersOffset, record_pointers) || record_pointers == 0) {
            add_warning(result, "'" + name + "': item manager record-pointer array is null");
            continue;
        }
        for (uint32_t slot = 0; slot < slots; ++slot) {
            if (!bit_set(mem, container_bitmap, slot))
                continue;
            uint32_t record_id = 0;
            if (!mem.u32(ids + slot * 4, record_id)) {
                add_warning(result, "'" + name + "': cannot read slot " + std::to_string(slot));
                continue;
            }
            if (record_id >= manager_count) {
                add_warning(result, "'" + name + "': record id " + std::to_string(record_id) +
                                        " >= manager count");
                continue;
            }
            if (!bit_set(mem, manager_bitmap, record_id)) {
                add_warning(result, "'" + name + "': record id " + std::to_string(record_id) +
                                        " is not marked used");
                continue;
            }
            uint32_t record = 0;
            if (!mem.u32(record_pointers + record_id * 4, record) || record == 0) {
                add_warning(result, "'" + name + "': null record for id " +
                                        std::to_string(record_id));
                continue;
            }
            uint32_t uid = 0;
            uint32_t item_id = 0;
            uint32_t item_flags = 0;
            int32_t count = 0;
            uint32_t mod_count = 0;
            uint32_t mod_array = 0;
            if (!mem.u32(record + kItemUidOffset, uid) || !mem.u32(record + kItemIdOffset, item_id) ||
                !mem.u32(record + kItemFlagsOffset, item_flags) ||
                !mem.i32(record + kItemCountOffset, count) ||
                !mem.u32(record + kItemModCountOffset, mod_count) ||
                !mem.u32(record + kItemModArrayOffset, mod_array)) {
                add_warning(result, "'" + name + "': cannot read item record " +
                                        std::to_string(record_id));
                continue;
            }
            if (uid == 0) {
                add_warning(result, "'" + name + "': record " + std::to_string(record_id) +
                                        " has zero uid");
                continue;
            }
            if (item_id == 0) {
                add_warning(result, "'" + name + "': record " + std::to_string(record_id) +
                                        " has zero item id");
                continue;
            }
            if (count <= 0) {
                add_warning(result, "'" + name + "': record " + std::to_string(record_id) +
                                        " has invalid count " + std::to_string(count));
                continue;
            }
            if (mod_count > kMaxMods) {
                add_warning(result, "'" + name + "': record " + std::to_string(record_id) +
                                        " has implausible mod count " +
                                        std::to_string(mod_count));
                continue;
            }
            if (mod_count != 0 && mod_array == 0) {
                add_warning(result, "'" + name + "': record " + std::to_string(record_id) +
                                        " has mods but a null mod array");
                continue;
            }
            ItemRow item;
            item.person = name;
            item.item_id = item_id;
            item.count = count;
            item.item_flags = item_flags;
            item.mods.reserve(mod_count);
            bool mods_ok = true;
            for (uint32_t mod_index = 0; mod_index < mod_count; ++mod_index) {
                const uint32_t mod_address = mod_array + mod_index * kModStride;
                uint32_t raw_id = 0;
                int32_t value = 0;
                uint32_t flags = 0;
                if (!mem.u32(mod_address, raw_id) || !mem.i32(mod_address + 4, value) ||
                    !mem.u32(mod_address + 8, flags)) {
                    add_warning(result, "'" + name + "': cannot read mod " +
                                            std::to_string(mod_index) + " of record " +
                                            std::to_string(record_id));
                    mods_ok = false;
                    break;
                }
                const int32_t mod_id = static_cast<int32_t>(raw_id);
                if (mod_id < 0) {
                    add_warning(result, "'" + name + "': negative mod id in record " +
                                            std::to_string(record_id));
                    mods_ok = false;
                    break;
                }
                item.mods.push_back(ModRow{mod_id, value, flags});
            }
            if (!mods_ok)
                continue;
            result.total_mods += item.mods.size();
            result.items.push_back(std::move(item));
        }
    }
    result.complete = true;
    return true;
}

ScanResult scan_until_stable(const MemReader &mem, const ModuleInfo &server) {
    ScanResult best;
    ScanResult previous;
    int stable = 0;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(kScanSettleMs);
    while (std::chrono::steady_clock::now() < deadline) {
        ScanResult current;
        scan_once(mem, server, current);
        const bool same_shape =
            current.items.size() == previous.items.size() &&
            current.total_mods == previous.total_mods &&
            current.person_count == previous.person_count && current.entity_count == previous.entity_count;
        if (same_shape && !current.items.empty())
            ++stable;
        else
            stable = 0;
        previous = current;
        best = std::move(current);
        if (stable >= kScanStablePolls)
            break;
        Sleep(kScanPollMs);
    }
    if (best.items.empty()) {
        ScanResult final;
        scan_once(mem, server, final);
        if (!final.items.empty() || final.level_read)
            best = std::move(final);
    }
    return best;
}

void write_rows(std::ostream &out, const ScanResult &scan) {
    for (const ItemRow &item : scan.items) {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), "item|%s|%u|%d|0x%x|%u\n", item.person.c_str(),
                      item.item_id, item.count, item.item_flags,
                      static_cast<unsigned>(item.mods.size()));
        out << buffer;
        for (const ModRow &mod : item.mods) {
            std::snprintf(buffer, sizeof(buffer), "mod|%s|%u|%d|0x%x|%d|%d|0x%x\n",
                          item.person.c_str(), item.item_id, item.count, item.item_flags, mod.id,
                          mod.value, mod.flags);
            out << buffer;
        }
    }
}

std::string address_string(uint32_t value);

bool write_capture(const std::filesystem::path &path, bool valid, const Options &options,
                   const ModuleInfo &server, const CaptureState &state,
                   const std::optional<uint16_t> &scan_cw, const ScanResult &scan,
                   std::string &error) {
    std::filesystem::path target = path;
    if (!valid)
        target += L".partial";
    std::ofstream file(target, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot open output file '" + to_utf8(target.wstring()) + "'";
        return false;
    }
    file << "# gl-specials-live-capture v2\n";
    file << "# source=gl_live_capture read-only\n";
    file << "# server_module=" << to_utf8(server.path) << "\n";
    file << "# server_base=0x" << std::hex << server.base << std::dec << "\n";
    file << "# rng_second=0x" << std::hex << state.second_rng_state << std::dec
         << " check=" << (state.second_draw_seen
                              ? (state.second_draw_valid ? "ok" : "fail")
                              : "unavailable")
         << "\n";
    file << "# calls=" << state.calls.size() << "\n";
    for (const CallTrace &call : state.calls) {
        file << "# call=" << call.index << " tid=" << call.tid
             << " hot=" << call.hot << " main=" << call.main
             << " inv=" << call.inventory << " draws=" << call.draws << "\n";
        for (std::size_t k = 0; k < call.draw_returns.size(); ++k) {
            const DrawTrace &draw = call.draw_returns[k];
            char line[64];
            std::snprintf(line, sizeof(line), "# draw=%u %u ret=%s state=%s\n",
                          static_cast<unsigned>(call.index),
                          static_cast<unsigned>(k + 1),
                          address_string(draw.ret).c_str(),
                          address_string(draw.state).c_str());
            file << line;
        }
    }
    if (!valid)
        file << "# incomplete capture: not a valid live oracle\n";
    file << "format=" << (valid ? "gl-specials-live-2" : "gl-specials-live-2-partial") << "\n";
    file << "level=" << to_utf8(options.level) << "\n";
    file << "seed=0\n";
    file << "rng_state=0x" << std::hex << state.rng_state << std::dec << "\n";
    const std::optional<uint32_t> loot_level =
        scan.loot_level.has_value() ? scan.loot_level
                                    : std::optional<uint32_t>(
                                          static_cast<uint32_t>(options.loot_level));
    file << "loot_level=" << *loot_level << "\n";
    file << "# loot_level_source=" << (scan.loot_level.has_value() ? "memory" : "cli") << "\n";
    if (state.entry_cw.has_value()) {
        char line[128];
        std::snprintf(line, sizeof(line), "# fpu_cw=0x%04x source=generation-entry\n",
                      static_cast<unsigned>(*state.entry_cw));
        file << line;
    } else {
        file << "# fpu_cw=unavailable source=generation-entry\n";
    }
    if (state.draw_cw.has_value()) {
        char line[128];
        std::snprintf(line, sizeof(line), "# fpu_cw=0x%04x source=first-draw\n",
                      static_cast<unsigned>(*state.draw_cw));
        file << line;
    } else {
        file << "# fpu_cw=unavailable source=first-draw\n";
    }
    if (scan_cw.has_value()) {
        char line[128];
        std::snprintf(line, sizeof(line), "# fpu_cw=0x%04x source=scan-time\n",
                      static_cast<unsigned>(*scan_cw));
        file << line;
    } else {
        file << "# fpu_cw=unavailable source=scan-time\n";
    }
    write_rows(file, scan);
    file.flush();
    if (!file) {
        error = "cannot write output file '" + to_utf8(target.wstring()) + "'";
        return false;
    }
    return true;
}

std::string address_string(uint32_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
    return buffer;
}

int run_price(const Options &options) {
    if (!enable_debug_privilege())
        log_warn("SeDebugPrivilege is not available; attach/read may still work");

    ModuleInfo client;
    std::string error;
    if (!find_module(options.pid, options.client_dll, client, error)) {
        log_error(error);
        return 2;
    }
    log_info("target PID " + std::to_string(options.pid) + ", " + to_utf8(file_name(client.path)) +
             " base " + address_string(client.base) + " size " + address_string(client.size));
    if (!module_range_ok(client, kPriceCallRva, 1)) {
        log_error("price call RVA " + address_string(kPriceCallRva) +
                  " is outside the resolved " + to_utf8(file_name(client.path)) + " image");
        return 2;
    }

    const uint32_t breakpoint = client.base + kPriceCallRva;
    Debugger debugger(options.pid, breakpoint, 0);
    if (!debugger.attach(error)) {
        log_error(error);
        return 2;
    }
    log_info("attached; DR0=" + address_string(breakpoint));
    if (!debugger.arm_all_threads(error))
        log_warn(error);

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(options.timeout_seconds);
    std::optional<uint16_t> price_cw;
    bool process_exited = false;
    bool debug_failed = false;
    while (!price_cw.has_value() && !process_exited && !debug_failed) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            log_warn("timeout waiting for the client price x87 call");
            break;
        }
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const DWORD wait_ms = static_cast<DWORD>(std::min<long long>(remaining.count(), 1000));
        DEBUG_EVENT event{};
        if (!WaitForDebugEvent(&event, wait_ms)) {
            const DWORD wait_error = GetLastError();
            if (wait_error == ERROR_SEM_TIMEOUT || wait_error == WAIT_TIMEOUT)
                continue;
            log_error("WaitForDebugEvent failed: " + wide_error(wait_error));
            debug_failed = true;
            break;
        }

        DWORD continue_status = DBG_CONTINUE;
        bool should_continue = true;
        switch (event.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT:
            if (event.u.CreateProcessInfo.hFile != nullptr)
                CloseHandle(event.u.CreateProcessInfo.hFile);
            debugger.ensure_thread(event.dwThreadId, event.u.CreateProcessInfo.hThread);
            break;
        case CREATE_THREAD_DEBUG_EVENT:
            debugger.ensure_thread(event.dwThreadId, event.u.CreateThread.hThread);
            break;
        case EXIT_THREAD_DEBUG_EVENT:
            debugger.close_thread(event.dwThreadId);
            break;
        case LOAD_DLL_DEBUG_EVENT:
            if (event.u.LoadDll.hFile != nullptr)
                CloseHandle(event.u.LoadDll.hFile);
            break;
        case EXCEPTION_DEBUG_EVENT: {
            const EXCEPTION_RECORD &record = event.u.Exception.ExceptionRecord;
            if (record.ExceptionCode == EXCEPTION_SINGLE_STEP) {
                const uint32_t address =
                    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record.ExceptionAddress));
                if (address == breakpoint) {
                    CONTEXT context{};
                    if (debugger.get_context(event.dwThreadId, context)) {
                        price_cw = static_cast<uint16_t>(context.FloatSave.ControlWord & 0xffffu);
                        char buffer[96];
                        std::snprintf(buffer, sizeof(buffer),
                                      "captured client price control word 0x%04x on thread %lu",
                                      static_cast<unsigned>(*price_cw),
                                      static_cast<unsigned long>(event.dwThreadId));
                        log_info(buffer);
                    } else {
                        log_error("cannot read the client price thread context");
                        debug_failed = true;
                    }
                    debugger.clear_dr0(event.dwThreadId);
                }
            } else if (record.ExceptionCode != EXCEPTION_BREAKPOINT) {
                continue_status = DBG_EXCEPTION_NOT_HANDLED;
            }
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
            process_exited = true;
            break;
        case RIP_EVENT:
            log_warn("debug RIP event " + std::to_string(event.u.RipInfo.dwError));
            break;
        default:
            break;
        }
        if (!ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continue_status)) {
            log_error("ContinueDebugEvent failed: " + wide_error(GetLastError()));
            should_continue = false;
        }
        if (!should_continue) {
            debug_failed = true;
            break;
        }
    }

    debugger.restore_and_detach();

    if (process_exited) {
        log_error("the target process exited before the price control word was captured");
        return 6;
    }
    if (!price_cw.has_value()) {
        if (!debug_failed)
            log_error("timeout waiting for the client price x87 control word");
        return 3;
    }

    const std::filesystem::path target(options.out_path);
    std::ofstream file(target, std::ios::binary | std::ios::trunc);
    if (!file) {
        log_error("cannot open output file '" + to_utf8(target.wstring()) + "'");
        return 5;
    }
    char line[64];
    std::snprintf(line, sizeof(line), "price=0x%04x\n# source=client-price\n",
                  static_cast<unsigned>(*price_cw));
    file << line;
    file.flush();
    if (!file) {
        log_error("cannot write output file '" + to_utf8(target.wstring()) + "'");
        return 5;
    }
    log_info("wrote " + to_utf8(options.out_path));
    return 0;
}

int run(const Options &options) {
    if (options.price_cw)
        return run_price(options);

    if (!enable_debug_privilege())
        log_warn("SeDebugPrivilege is not available; attach/read may still work");

    ModuleInfo server;
    std::string error;
    if (!find_module(options.pid, options.server_dll, server, error)) {
        log_error(error);
        return 2;
    }
    log_info("target PID " + std::to_string(options.pid) + ", " + to_utf8(file_name(server.path)) +
             " base " + address_string(server.base) + " size " + address_string(server.size));

    if (!options.scan_only) {
        for (const std::pair<const char *, uint32_t> check : {
                 std::pair<const char *, uint32_t>{"NPC spawn entry", kNpcSpawnRva},
                 std::pair<const char *, uint32_t>{"_rand pre-draw", kRandBeforeDrawRva}}) {
            if (!module_range_ok(server, check.second, 1)) {
                log_error(std::string(check.first) + " RVA " + address_string(check.second) +
                          " is outside the resolved " + to_utf8(file_name(server.path)) + " image");
                return 2;
            }
        }
    }

    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, options.pid);
    if (process == nullptr) {
        log_error("OpenProcess(VmRead|Query) failed: " + wide_error(GetLastError()));
        return 2;
    }
    MemReader mem(process);

    if (options.scan_only) {
        const ScanResult scan = scan_until_stable(mem, server);
        log_info("scan-only: entities=" + std::to_string(scan.entity_count) +
                 " persons=" + std::to_string(scan.person_count) +
                 " items=" + std::to_string(scan.items.size()) +
                 " mods=" + std::to_string(scan.total_mods) +
                 (scan.complete ? "" : " (incomplete)"));
        std::size_t shown = 0;
        for (const ItemRow &item : scan.items) {
            if (shown++ >= 20)
                break;
            std::string line = "  " + item.person + " item=" + std::to_string(item.item_id) +
                               " count=" + std::to_string(item.count) + " mods=";
            for (const ModRow &mod : item.mods) {
                line += "[" + std::to_string(mod.id) + ":" + std::to_string(mod.value) + ":0x" +
                        address_string(mod.flags).substr(2) + "]";
            }
            log_info(line);
        }
        for (const std::string &warning : scan.warnings)
            log_warn(warning);
        CloseHandle(process);
        return scan.complete && scan.person_count != 0 ? 0 : 4;
    }

    CaptureState state;
    state.bp0 = server.base + kNpcSpawnRva;
    state.bp1 = server.base + kRandBeforeDrawRva;

    Debugger debugger(options.pid, state.bp0, state.bp1);
    if (!debugger.attach(error)) {
        log_error(error);
        CloseHandle(process);
        return 2;
    }
    log_info("attached; DR0=" + address_string(state.bp0) + " DR1=" + address_string(state.bp1));
    if (!debugger.arm_all_threads(error))
        log_warn(error);

    std::optional<std::chrono::steady_clock::time_point> trace_deadline;
    if (options.trace_calls_ms != 0)
        trace_deadline = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(options.trace_calls_ms);

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(options.timeout_seconds);
    bool stop = false;
    while (!stop) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            log_warn("timeout waiting for the first NPC generation draw");
            break;
        }
        if (trace_deadline.has_value() && now >= *trace_deadline) {
            log_info("trace window elapsed; detaching");
            stop = true;
            break;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const DWORD wait_ms = static_cast<DWORD>(std::min<long long>(remaining.count(), 1000));
        DEBUG_EVENT event{};
        if (!WaitForDebugEvent(&event, wait_ms)) {
            const DWORD wait_error = GetLastError();
            if (wait_error == ERROR_SEM_TIMEOUT || wait_error == WAIT_TIMEOUT)
                continue;
            log_error("WaitForDebugEvent failed: " + wide_error(wait_error));
            break;
        }

        DWORD continue_status = DBG_CONTINUE;
        bool should_continue = true;
        switch (event.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT: {
            if (event.u.CreateProcessInfo.hFile != nullptr)
                CloseHandle(event.u.CreateProcessInfo.hFile);
            debugger.ensure_thread(event.dwThreadId, event.u.CreateProcessInfo.hThread);
            break;
        }
        case CREATE_THREAD_DEBUG_EVENT:
            debugger.ensure_thread(event.dwThreadId, event.u.CreateThread.hThread);
            break;
        case EXIT_THREAD_DEBUG_EVENT:
            debugger.close_thread(event.dwThreadId);
            break;
        case LOAD_DLL_DEBUG_EVENT:
            if (event.u.LoadDll.hFile != nullptr)
                CloseHandle(event.u.LoadDll.hFile);
            break;
        case EXCEPTION_DEBUG_EVENT: {
            const EXCEPTION_RECORD &record = event.u.Exception.ExceptionRecord;
            if (record.ExceptionCode == EXCEPTION_SINGLE_STEP) {
                const uint32_t address =
                    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record.ExceptionAddress));
                if (address == state.bp0) {
                    if (!state.generation_active) {
                        state.generation_active = true;
                        state.generation_tid = event.dwThreadId;
                        CONTEXT context{};
                        if (debugger.get_context(event.dwThreadId, context)) {
                            state.entry_cw =
                                static_cast<uint16_t>(context.FloatSave.ControlWord & 0xffffu);
                        }
                        log_info("NPC generation active (thread " +
                                 std::to_string(event.dwThreadId) + ")");
                    }
                    if (state.calls.size() < kMaxCallTrace) {
                        CallTrace call;
                        call.index = static_cast<uint32_t>(state.calls.size() + 1);
                        call.tid = event.dwThreadId;
                        CONTEXT context{};
                        if (debugger.get_context(event.dwThreadId, context)) {
                            std::uint32_t name_ptr = 0;
                            mem.u32(context.Esp + kNpcSpawnHotStackOffset, call.hot);
                            mem.u32(context.Esp + kNpcSpawnMainStackOffset, call.main);
                            mem.u32(context.Esp + kNpcSpawnInventoryStackOffset, name_ptr);
                            std::string raw;
                            if (name_ptr != 0 && mem.cstring(name_ptr, kMaxNameBytes, raw))
                                call.inventory = sanitize_name(cp1251_to_utf8(raw));
                        }
                        log_info("npc call " + std::to_string(call.index) + " tid=" +
                                 std::to_string(call.tid) + " hot=" + address_string(call.hot) +
                                 " main=" + address_string(call.main) + " inv=" +
                                 call.inventory);
                        state.calls.push_back(std::move(call));
                    }
                    if (!state.second_draw_seen)
                        debugger.arm_dr1(event.dwThreadId);
                    if (options.trace_calls_ms != 0)
                        debugger.set_resume_flag(event.dwThreadId);
                    if (options.trace_calls_ms == 0)
                        debugger.clear_dr0(event.dwThreadId);
                } else if (address == state.bp1) {
                    log_info("single-step at DR1 on thread " + std::to_string(event.dwThreadId));
                    const bool is_generation_thread =
                        state.generation_active && event.dwThreadId == state.generation_tid;
                    if (options.trace_calls_ms != 0) {
                        CONTEXT context{};
                        uint32_t ret = 0;
                        uint32_t rng_state = 0;
                        bool state_read = false;
                        const bool have_context =
                            debugger.get_context(event.dwThreadId, context);
                        if (have_context) {
                            mem.u32(context.Esp, ret);
                            const uint32_t crt = context.Eax;
                            state_read = crt != 0 && mem.u32(crt + 0x14, rng_state);
                        }
                        if (is_generation_thread) {
                            const bool have_state = have_context && state_read;
                            if (!state.state_captured) {
                                if (have_state) {
                                    state.rng_state = rng_state;
                                    state.draw_cw = static_cast<uint16_t>(
                                        context.FloatSave.ControlWord & 0xffffu);
                                    state.state_captured = true;
                                    log_info("captured CRT _rand state " +
                                             address_string(rng_state) +
                                             " before the first NPC draw");
                                } else if (!have_context) {
                                    state.capture_failed = true;
                                    log_error("cannot read the generation thread context at the "
                                              "first NPC draw");
                                } else {
                                    state.capture_failed = true;
                                    log_error("cannot read CRT state from EAX=" +
                                              address_string(context.Eax) +
                                              " at the first NPC draw");
                                }
                            } else if (!state.second_draw_seen) {
                                state.second_draw_seen = true;
                                if (have_state) {
                                    state.second_rng_state = rng_state;
                                    state.second_draw_valid =
                                        rng_state == state.rng_state * 0x343FDu + 0x269EC3u;
                                    log_info("second draw state " + address_string(rng_state) +
                                             (state.second_draw_valid ? " LCG check ok"
                                                                      : " LCG CHECK FAILED"));
                                }
                            }
                            if (!state.calls.empty()) {
                                CallTrace &call = state.calls.back();
                                ++call.draws;
                                if (call.draw_returns.size() < kMaxDrawTrace)
                                    call.draw_returns.push_back(DrawTrace{ret, rng_state});
                            }
                            debugger.set_resume_flag(event.dwThreadId);
                        } else {
                            debugger.clear_dr1(event.dwThreadId);
                        }
                    } else if (state.generation_active) {
                        if (event.dwThreadId != state.generation_tid) {
                            debugger.clear_dr1(event.dwThreadId);
                        } else if (!state.state_captured) {
                            CONTEXT context{};
                            if (debugger.get_context(event.dwThreadId, context)) {
                                const uint32_t crt = context.Eax;
                                uint32_t rng_state = 0;
                                if (crt != 0 && mem.u32(crt + 0x14, rng_state)) {
                                    state.rng_state = rng_state;
                                    state.draw_cw = static_cast<uint16_t>(
                                        context.FloatSave.ControlWord & 0xffffu);
                                    state.state_captured = true;
                                    log_info("captured CRT _rand state " +
                                             address_string(rng_state) +
                                             " before the first NPC draw");
                                } else {
                                    state.capture_failed = true;
                                    log_error("cannot read CRT state from EAX=" +
                                              address_string(crt) + " at the first NPC draw");
                                }
                            } else {
                                state.capture_failed = true;
                                log_error("cannot read the generation thread context at the "
                                          "first NPC draw");
                            }
                            debugger.set_resume_flag(event.dwThreadId);
                        } else if (!state.second_draw_seen) {
                            CONTEXT context{};
                            state.second_draw_seen = true;
                            if (debugger.get_context(event.dwThreadId, context)) {
                                const uint32_t crt = context.Eax;
                                uint32_t rng_state = 0;
                                if (crt != 0 && mem.u32(crt + 0x14, rng_state)) {
                                    state.second_rng_state = rng_state;
                                    state.second_draw_valid =
                                        rng_state == state.rng_state * 0x343FDu + 0x269EC3u;
                                    log_info("second draw state " + address_string(rng_state) +
                                             (state.second_draw_valid ? " LCG check ok"
                                                                      : " LCG CHECK FAILED"));
                                }
                            }
                            debugger.clear_dr1(event.dwThreadId);
                            if (options.trace_calls_ms == 0)
                                stop = true;
                        } else {
                            debugger.clear_dr1(event.dwThreadId);
                        }
                    }
                }
            } else if (record.ExceptionCode != EXCEPTION_BREAKPOINT) {
                continue_status = DBG_EXCEPTION_NOT_HANDLED;
            }
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
            state.process_exited = true;
            stop = true;
            break;
        case RIP_EVENT:
            log_warn("debug RIP event " + std::to_string(event.u.RipInfo.dwError));
            break;
        default:
            break;
        }
        if (!ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continue_status)) {
            log_error("ContinueDebugEvent failed: " + wide_error(GetLastError()));
            should_continue = false;
        }
        if (!should_continue)
            break;
    }

    debugger.restore_and_detach();
    const std::optional<uint16_t> scan_cw =
        state.process_exited ? std::nullopt
                             : scan_control_word_for_pid(options.pid, state.generation_tid);

    if (state.process_exited) {
        log_error("the target process exited before the capture completed");
        CloseHandle(process);
        return 6;
    }

    ScanResult scan = scan_until_stable(mem, server);
    for (const std::string &warning : scan.warnings)
        log_warn(warning);
    log_info("scan: entities=" + std::to_string(scan.entity_count) +
             " persons=" + std::to_string(scan.person_count) +
             " items=" + std::to_string(scan.items.size()) +
             " mods=" + std::to_string(scan.total_mods));

    const bool valid = state.state_captured && state.second_draw_valid && !scan.items.empty();
    if (!state.state_captured)
        log_error("RNG state was not captured; writing a partial diagnostic file only");
    else if (!state.second_draw_valid)
        log_error("LCG double-draw check failed; writing a partial diagnostic file only");
    else if (scan.items.empty())
        log_error("no NPC main-container items were found; writing a partial diagnostic file only");

    if (!write_capture(std::filesystem::path(options.out_path), valid, options, server, state,
                       scan_cw, scan, error)) {
        log_error(error);
        CloseHandle(process);
        return 5;
    }
    log_info("wrote " + to_utf8(options.out_path) + (valid ? "" : ".partial"));
    CloseHandle(process);
    if (!valid)
        return (state.state_captured && state.second_draw_valid) ? 4 : 3;
    return 0;
}

} // namespace

int main() {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        log_error("cannot parse the command line");
        return 1;
    }
    Options options;
    std::string error;
    const bool parsed = parse_args(argc, argv, options, error);
    LocalFree(argv);
    if (!parsed) {
        log_error(error);
        print_usage();
        return 1;
    }
    return run(options);
}
