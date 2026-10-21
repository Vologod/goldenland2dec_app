#include "gl/script/script_vm.hpp"
#include "gl/assets/log.hpp"
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace gl::script {
namespace {
constexpr std::uint32_t kFunctionExit = 0x1000000;
constexpr std::uint32_t kFunctionSignal = 0x1000001;
constexpr std::uint32_t kFunctionConsole = 0x1000002;
constexpr std::uint32_t kFunctionCmd = 0x1000003;
constexpr std::uint32_t kFunctionSay = 0x1000004;
constexpr std::uint32_t kFunctionCloseDialog = 0x1000005;
constexpr std::uint32_t kFunctionAnswer = 0x1000006;

bool is_comparison(std::int32_t opcode) { return opcode >= 6 && opcode <= 11; }
bool is_logical(std::int32_t opcode) { return opcode == 0 || opcode == 2; }
bool is_value(std::int32_t opcode) { return opcode >= 21 && opcode <= 24; }

void log_warning(std::string_view message) {
    gl::log(gl::LogLevel::warning, "script vm: " + std::string(message));
}

std::int32_t to_int32(const Value& value) {
    return std::visit(
        [](const auto& item) -> std::int32_t {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::int32_t>) {
                return item;
            } else if constexpr (std::is_same_v<T, std::uint32_t>) {
                return static_cast<std::int32_t>(item);
            } else if constexpr (std::is_same_v<T, double>) {
                if (!std::isfinite(item)) return 0;
                if (item >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
                    return std::numeric_limits<std::int32_t>::max();
                if (item <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
                    return std::numeric_limits<std::int32_t>::min();
                return static_cast<std::int32_t>(item);
            } else {
                return 0;
            }
        },
        value);
}

std::uint32_t to_uint32(const Value& value) {
    return std::visit(
        [](const auto& item) -> std::uint32_t {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::int32_t>) {
                return static_cast<std::uint32_t>(item);
            } else if constexpr (std::is_same_v<T, std::uint32_t>) {
                return item;
            } else if constexpr (std::is_same_v<T, double>) {
                if (!std::isfinite(item) || item <= 0.0) return 0;
                if (item >= static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
                    return std::numeric_limits<std::uint32_t>::max();
                return static_cast<std::uint32_t>(item);
            } else {
                return 0;
            }
        },
        value);
}

Value coerce_number(double value, const Value& target) {
    if (std::holds_alternative<std::int32_t>(target))
        return std::int32_t{to_int32(Value{value})};
    if (std::holds_alternative<std::uint32_t>(target))
        return std::uint32_t{to_uint32(Value{value})};
    return value;
}

double to_double(const Value& value) {
    return std::visit(
        [](const auto& item) -> double {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return 0.0;
            } else {
                return static_cast<double>(item);
            }
        },
        value);
}

bool is_true(const Value& value) {
    return std::visit(
        [](const auto& item) -> bool {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return !item.empty();
            } else {
                return item != 0;
            }
        },
        value);
}

template <typename T>
bool compare_op(T left, T right, std::int32_t opcode) {
    switch (opcode) {
    case 6: return left != right;
    case 7: return left == right;
    case 8: return left >= right;
    case 9: return left <= right;
    case 10: return left > right;
    case 11: return left < right;
    default: return false;
    }
}
} // namespace

ScriptVm::ScriptVm(std::vector<gl::assets::CsNode> nodes, ScriptHost& host)
    : nodes_(std::move(nodes)), host_(host) {}

void ScriptVm::set_globals(std::span<const ScriptGlobal> globals) {
    globals_.clear();
    for (const ScriptGlobal& global : globals) globals_.emplace(global.name, global.value);
    rebuild_variables();
}

void ScriptVm::restart(bool reset_all) {
    counter_ = 0;
    pc_ = 0;
    status_ = VmStatus::start;
    dialog_calls_.clear();
    dialog_say_ = false;
    end_request_ = false;
    failed_ = false;

    if (reset_all) {
        rebuild_variables();
    } else {
        variables_["LastPhrase"] = std::uint32_t{0};
        variables_["LastAnswer"] = std::uint32_t{0};
    }
}

VmStatus ScriptVm::status() const { return status_; }

std::uint64_t ScriptVm::steps() const { return counter_; }

Value* ScriptVm::variable(std::string_view name) {
    auto it = variables_.find(std::string(name));
    if (it == variables_.end()) return nullptr;
    return &it->second;
}

const std::map<std::string, Value>& ScriptVm::variables() const { return variables_; }

bool ScriptVm::step() {
    if (status_ == VmStatus::wait_user || status_ == VmStatus::end ||
        status_ == VmStatus::infinity)
        return false;

    if (counter_ >= kStepLimit) {
        gl::log(gl::LogLevel::error, "script vm: step limit reached");
        status_ = VmStatus::infinity;
        return false;
    }

    if (!valid_index(pc_)) {
        gl::log(gl::LogLevel::error,
                "script vm: node index out of range: " + std::to_string(pc_));
        status_ = VmStatus::end;
        return false;
    }

    ++counter_;
    failed_ = false;
    end_request_ = false;

    const gl::assets::CsNode& node = nodes_[static_cast<std::size_t>(pc_)];
    std::int32_t next = -1;

    if (is_logical(node.opcode) || is_comparison(node.opcode)) {
        const Value value = eval(pc_, 0);
        if (failed_) {
            status_ = VmStatus::end;
            return false;
        }
        next = is_true(value) ? node.c : node.d;
    } else {
        switch (node.opcode) {
        case 49:
            if (node.d == -1) {
                status_ = dialog_say_ ? VmStatus::wait_user : VmStatus::end;
                return false;
            }
            next = node.c;
            break;

        case 50: {
            if (!valid_index(node.a)) {
                fail("assign destination index out of range: " + std::to_string(node.a));
                break;
            }
            const gl::assets::CsNode& destination =
                nodes_[static_cast<std::size_t>(node.a)];
            if (destination.opcode != 23) {
                fail("assign destination is not a variable");
                break;
            }

            if (!valid_index(node.b)) {
                fail("assign source index out of range: " + std::to_string(node.b));
                break;
            }

            const gl::assets::CsNode& source = nodes_[static_cast<std::size_t>(node.b)];
            Value result;
            if (source.opcode == 23) {
                result = lookup_variable(source.text);
            } else if (source.opcode == 21 || source.opcode == 24) {
                result = std::int32_t{to_int32(Value{source.value})};
            } else {
                result = eval(node.b, 0);
            }

            if (failed_) break;
            variables_[destination.text] = std::move(result);
            next = node.d;
            break;
        }

        case 48: {
            (void)call_function(node, 0);
            if (failed_ || end_request_) break;
            if (node.d >= 0) {
                next = node.d;
            } else if (valid_index(pc_ + 1)) {
                next = pc_ + 1;
            } else {
                status_ = VmStatus::end;
                return false;
            }
            break;
        }

        default:
            if (is_value(node.opcode)) {
                (void)eval(pc_, 0);
                if (failed_) break;
                if (valid_index(pc_ + 1)) {
                    next = pc_ + 1;
                    break;
                }
                status_ = VmStatus::end;
                return false;
            }
            fail("unsupported top-level opcode: " + std::to_string(node.opcode));
            break;
        }
    }

    if (failed_ || end_request_) {
        status_ = VmStatus::end;
        return false;
    }

    status_ = VmStatus::continue_;
    pc_ = next;
    return true;
}

bool ScriptVm::valid_index(std::int32_t index) const {
    return index >= 0 && static_cast<std::size_t>(index) < nodes_.size();
}

Value ScriptVm::eval(std::int32_t index, int depth) {
    if (depth > kMaxEvalDepth) {
        fail("expression nesting is too deep");
        return std::int32_t{0};
    }
    if (!valid_index(index)) {
        fail("expression node index out of range: " + std::to_string(index));
        return std::int32_t{0};
    }

    const gl::assets::CsNode& node = nodes_[static_cast<std::size_t>(index)];
    switch (node.opcode) {
    case 21:
    case 24:
        return node.value;
    case 22:
        return node.text;
    case 23:
        return lookup_variable(node.text);
    case 48:
        return call_function(node, depth);
    case 14:
    case 15:
    case 16:
    case 17:
    case 18: {
        const Value left = eval(node.a, depth + 1);
        if (failed_) return std::int32_t{0};
        if (!valid_index(node.b)) {
            fail("expression node index out of range: " + std::to_string(node.b));
            return std::int32_t{0};
        }
        const gl::assets::CsNode& right_node = nodes_[static_cast<std::size_t>(node.b)];
        const Value right =
            (right_node.opcode == 21 || right_node.opcode == 24)
                ? coerce_number(right_node.value, left)
                : eval(node.b, depth + 1);
        if (failed_) return std::int32_t{0};
        return apply_arithmetic(left, right, node.opcode);
    }
    default:
        if (is_comparison(node.opcode)) {
            const Value left = eval(node.a, depth + 1);
            const Value right = eval(node.b, depth + 1);
            if (failed_) return std::int32_t{0};
            return std::int32_t{compare_values(left, right, node.opcode) ? 1 : 0};
        }
        if (is_logical(node.opcode)) {
            const Value left = eval(node.a, depth + 1);
            const Value right = eval(node.b, depth + 1);
            if (failed_) return std::int32_t{0};
            const bool left_true = is_true(left);
            const bool right_true = is_true(right);
            const bool result =
                node.opcode == 0 ? (left_true || right_true) : (left_true && right_true);
            return std::int32_t{result ? 1 : 0};
        }
        fail("unsupported expression opcode: " + std::to_string(node.opcode));
        return std::int32_t{0};
    }
}

Value ScriptVm::call_function(const gl::assets::CsNode& node, int depth) {
    if (!std::isfinite(node.value) || node.value < 0.0 ||
        node.value > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        fail("invalid function id");
        return std::int32_t{0};
    }
    const auto id = static_cast<std::uint32_t>(node.value);

    std::vector<Value> args;
    for (std::int32_t argument : node.args) {
        if (argument == -1) break;
        if (!valid_index(argument)) {
            fail("function argument index out of range: " + std::to_string(argument));
            return std::int32_t{0};
        }
        args.push_back(eval(argument, depth + 1));
        if (failed_) return std::int32_t{0};
    }

    if (id == kFunctionSay || id == kFunctionAnswer) {
        dialog_calls_.push_back(DialogCall{id, args});
        if (id == kFunctionSay) dialog_say_ = true;
        return std::int32_t{0};
    }

    if (id == kFunctionCloseDialog) {
        end_request_ = true;
        return std::int32_t{0};
    }

    if (id == kFunctionExit || id == kFunctionSignal || id == kFunctionConsole ||
        id == kFunctionCmd) {
        gl::log(gl::LogLevel::info,
                "script vm: stub function " + std::string(gl::assets::cs_function_name(id)));
        return std::int32_t{0};
    }

    return host_.call(id, std::span<const Value>(args));
}

Value ScriptVm::apply_arithmetic(const Value& left, const Value& right, std::int32_t opcode) {
    if (std::holds_alternative<std::string>(left) ||
        std::holds_alternative<std::string>(right)) {
        log_warning("arithmetic on strings is not supported");
        return std::int32_t{0};
    }

    if (std::holds_alternative<double>(left) || std::holds_alternative<double>(right)) {
        const auto a = to_double(left);
        const auto b = to_double(right);
        switch (opcode) {
        case 14: return a + b;
        case 15: return a - b;
        case 16: return a * b;
        case 17:
            if (b == 0.0) {
                log_warning("division by zero");
                return std::int32_t{0};
            }
            return a / b;
        case 18:
            log_warning("modulo requires integers");
            return std::int32_t{0};
        default:
            return std::int32_t{0};
        }
    }

    if (std::holds_alternative<std::uint32_t>(left) ||
        std::holds_alternative<std::uint32_t>(right)) {
        const auto a = to_uint32(left);
        const auto b = to_uint32(right);
        switch (opcode) {
        case 14: return static_cast<std::uint32_t>(a + b);
        case 15: return static_cast<std::uint32_t>(a - b);
        case 16: return static_cast<std::uint32_t>(a * b);
        case 17:
            if (b == 0) {
                log_warning("division by zero");
                return std::int32_t{0};
            }
            return a / b;
        case 18:
            if (b == 0) {
                log_warning("modulo by zero");
                return std::int32_t{0};
            }
            return a % b;
        default:
            return std::int32_t{0};
        }
    }

    const std::int64_t a = to_int32(left);
    const std::int64_t b = to_int32(right);
    switch (opcode) {
    case 14: return static_cast<std::int32_t>(a + b);
    case 15: return static_cast<std::int32_t>(a - b);
    case 16: return static_cast<std::int32_t>(a * b);
    case 17:
        if (b == 0) {
            log_warning("division by zero");
            return std::int32_t{0};
        }
        return static_cast<std::int32_t>(a / b);
    case 18:
        if (b == 0) {
            log_warning("modulo by zero");
            return std::int32_t{0};
        }
        return static_cast<std::int32_t>(a % b);
    default:
        return std::int32_t{0};
    }
}

bool ScriptVm::compare_values(const Value& left, const Value& right, std::int32_t opcode) {
    if (std::holds_alternative<std::string>(left)) {
        if (!std::holds_alternative<std::string>(right)) {
            log_warning("comparison between string and number");
            return opcode == 6;
        }
        const auto& a = std::get<std::string>(left);
        const auto& b = std::get<std::string>(right);
        if (opcode == 6) return a != b;
        if (opcode == 7) return a == b;
        log_warning("relational comparison on strings is not supported");
        return false;
    }

    if (std::holds_alternative<std::string>(right)) {
        log_warning("comparison between number and string");
        return opcode == 6;
    }

    if (std::holds_alternative<std::uint32_t>(left))
        return compare_op(std::get<std::uint32_t>(left), to_uint32(right), opcode);

    if (std::holds_alternative<double>(left))
        return compare_op(std::get<double>(left), to_double(right), opcode);

    return compare_op(to_int32(left), to_int32(right), opcode);
}

Value ScriptVm::lookup_variable(const std::string& name) {
    auto it = variables_.find(name);
    if (it != variables_.end()) return it->second;

    auto global = globals_.find(name);
    if (global != globals_.end())
        return variables_.emplace(name, global->second).first->second;

    return variables_.emplace(name, Value{std::int32_t{0}}).first->second;
}

void ScriptVm::rebuild_variables() {
    variables_.clear();
    std::set<std::string> missing;
    for (const gl::assets::CsNode& node : nodes_) {
        if (node.opcode != 23) continue;
        auto global = globals_.find(node.text);
        if (global != globals_.end()) {
            variables_.emplace(node.text, global->second);
        } else if (missing.insert(node.text).second) {
            gl::log(gl::LogLevel::warning,
                    "script vm: variable '" + node.text + "' not found in globals");
        }
    }
}

void ScriptVm::fail(std::string message) {
    if (!failed_) gl::log(gl::LogLevel::error, "script vm: " + message);
    failed_ = true;
}
} // namespace gl::script
