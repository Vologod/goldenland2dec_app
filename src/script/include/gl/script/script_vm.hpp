#pragma once
#include "gl/assets/cs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/host.hpp"
#include "gl/script/value.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gl::script {
enum class VmStatus { start, restart, continue_, wait_user, end, infinity };

class ScriptVm {
public:
    ScriptVm(std::vector<gl::assets::CsNode> nodes, ScriptHost& host);
    void set_globals(std::span<const ScriptGlobal> globals);
    void restart(bool reset_all);
    VmStatus status() const;
    bool step();
    bool failed() const;
    std::uint64_t steps() const;
    Value* variable(std::string_view name);
    const std::map<std::string, Value>& variables() const;
    std::size_t answers_count() const;
    std::vector<std::uint32_t> dialogs_data() const;
    bool user_input(std::size_t answer);

private:
    struct DialogCall {
        std::uint32_t id;
        std::uint32_t value;
    };

    static constexpr std::uint64_t kStepLimit = 10'000'000;
    static constexpr int kMaxEvalDepth = 1024;
    static constexpr std::uint64_t kEvalBudget = 100'000;

    bool valid_index(std::int32_t index) const;
    Value eval(std::int32_t index, int depth);
    Value call_function(const gl::assets::CsNode& node, int depth);
    Value apply_arithmetic(const Value& left, const Value& right, std::int32_t opcode);
    Value apply_bitwise(const Value& left, const Value& right, std::int32_t opcode);
    Value apply_unary(const Value& operand, std::int32_t opcode);
    Value coerce_assignment(const Value& value, const Value& destination);
    bool compare_values(const Value& left, const Value& right, std::int32_t opcode);
    Value lookup_variable(const std::string& name);
    void rebuild_variables();
    void fail(std::string message);
    void record_error(std::string_view message);

    std::vector<gl::assets::CsNode> nodes_;
    ScriptHost& host_;
    std::map<std::string, Value> globals_;
    std::map<std::string, Value> variables_;
    std::vector<DialogCall> dialog_calls_;
    std::int32_t pc_ = 0;
    std::uint64_t counter_ = 0;
    std::uint64_t eval_ops_ = 0;
    VmStatus status_ = VmStatus::start;
    bool dialog_say_ = false;
    bool end_request_ = false;
    bool failed_ = false;
    bool error_ = false;
};
}
