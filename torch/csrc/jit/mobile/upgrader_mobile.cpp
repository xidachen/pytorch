#include <torch/csrc/jit/mobile/upgrader_mobile.h>

namespace c10 {
TypePtr parseType(const std::string& pythonStr);
} // namespace c10

namespace torch {
namespace jit {

const std::unordered_map<std::string, std::vector<Upgrader>>
getOperatorVersionMapForMobile() {
  static std::unordered_map<std::string, std::vector<Upgrader>>
      operatorVersionMapForMobile({
          {std::string("aten::div.Tensor"),
           std::vector<Upgrader>({Upgrader({0, 3, "div_Tensor_0_3", 0})})},
          {std::string("aten::div.Scalar"),
           std::vector<Upgrader>({Upgrader({0, 3, "div_Scalar_0_3", 1})})},
          {std::string("aten::div.out"),
           std::vector<Upgrader>({Upgrader({0, 3, "div_out_0_3", 2})})},
          {std::string("aten::div_.Tensor"),
           std::vector<Upgrader>({Upgrader({0, 3, "div__Tensor_0_3", 3})})},
          {std::string("aten::div_.Scalar"),
           std::vector<Upgrader>({Upgrader({0, 3, "div__Scalar_0_3", 4})})},
      });
  return operatorVersionMapForMobile;
}

std::vector<ByteCodeFunctionWithOperator> getUpgraderBytecodeList() {
  static std::vector<ByteCodeFunctionWithOperator> upgraderBytecodeList({
      ByteCodeFunctionWithOperator({
          mobile::Function::registerFunc(
              "div_Tensor_0_3",
              std::vector<Instruction>({
                  Instruction{OpCode::STOREN, 1, 2},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::JF, 3, 0},
                  Instruction{OpCode::LOADC, 1, 0},
                  Instruction{OpCode::JMP, 3, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::STORE, 3, 0},
                  Instruction{OpCode::MOVE, 3, 0},
                  Instruction{OpCode::JF, 5, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 1, 0},
                  Instruction{OpCode::JMP, 5, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::LOADC, 0, 0},
                  Instruction{OpCode::OP, 2, 0},
                  Instruction{OpCode::STORE, 4, 0},
                  Instruction{OpCode::DROPR, 2, 0},
                  Instruction{OpCode::DROPR, 1, 0},
                  Instruction{OpCode::MOVE, 4, 0},
                  Instruction{OpCode::RET, 0, 0},
              }), // instructions_
              std::vector<c10::IValue>({
                  c10::IValue("trunc"),
                  c10::IValue(true),
              }), // constants
              std::vector<c10::TypePtr>(), // types
              4 // register_size_
              ),
          std::vector<OperatorString>({
              OperatorString({"aten::is_floating_point", "", 1}),
              OperatorString({"aten::div", "Tensor", 2}),
              OperatorString({"aten::div", "Tensor_mode", 3}),
          }), // op_names
      }),
      ByteCodeFunctionWithOperator({
          mobile::Function::registerFunc(
              "div_Scalar_0_3",
              std::vector<Instruction>({
                  Instruction{OpCode::STOREN, 1, 2},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::JF, 3, 0},
                  Instruction{OpCode::LOADC, 1, 0},
                  Instruction{OpCode::JMP, 3, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::ISINSTANCE, 0, 1},
                  Instruction{OpCode::STORE, 3, 0},
                  Instruction{OpCode::MOVE, 3, 0},
                  Instruction{OpCode::JF, 5, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 1, 0},
                  Instruction{OpCode::JMP, 6, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 2, 0},
                  Instruction{OpCode::LOADC, 0, 0},
                  Instruction{OpCode::OP, 3, 0},
                  Instruction{OpCode::STORE, 4, 0},
                  Instruction{OpCode::DROPR, 2, 0},
                  Instruction{OpCode::DROPR, 1, 0},
                  Instruction{OpCode::MOVE, 4, 0},
                  Instruction{OpCode::RET, 0, 0},
              }), // instructions_
              std::vector<c10::IValue>({
                  c10::IValue("trunc"),
                  c10::IValue(true),
              }), // constants
              std::vector<c10::TypePtr>({c10::parseType("float")}), // types
              4 // register_size_
              ),
          std::vector<OperatorString>({
              OperatorString({"aten::is_floating_point", "", 1}),
              OperatorString({"aten::div", "Scalar", 2}),
              OperatorString({"prim::unchecked_cast", "", 1}),
              OperatorString({"aten::div", "Scalar_mode", 3}),
          }), // op_names
      }),
      ByteCodeFunctionWithOperator({
          mobile::Function::registerFunc(
              "div_out_0_3",
              std::vector<Instruction>({
                  Instruction{OpCode::STOREN, 1, 3},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::JF, 3, 0},
                  Instruction{OpCode::LOADC, 1, 0},
                  Instruction{OpCode::JMP, 3, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::JF, 3, 0},
                  Instruction{OpCode::LOADC, 1, 0},
                  Instruction{OpCode::JMP, 3, 0},
                  Instruction{OpCode::LOAD, 3, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::STORE, 4, 0},
                  Instruction{OpCode::MOVE, 4, 0},
                  Instruction{OpCode::JF, 6, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::LOAD, 3, 0},
                  Instruction{OpCode::OP, 1, 0},
                  Instruction{OpCode::JMP, 6, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::LOADC, 0, 0},
                  Instruction{OpCode::LOAD, 3, 0},
                  Instruction{OpCode::OP, 2, 0},
                  Instruction{OpCode::STORE, 5, 0},
                  Instruction{OpCode::DROPR, 3, 0},
                  Instruction{OpCode::DROPR, 2, 0},
                  Instruction{OpCode::DROPR, 1, 0},
                  Instruction{OpCode::MOVE, 5, 0},
                  Instruction{OpCode::RET, 0, 0},
              }), // instructions_
              std::vector<c10::IValue>({
                  c10::IValue("trunc"),
                  c10::IValue(true),
              }), // constants
              std::vector<c10::TypePtr>(), // types
              5 // register_size_
              ),
          std::vector<OperatorString>({
              OperatorString({"aten::is_floating_point", "", 1}),
              OperatorString({"aten::div", "out", 3}),
              OperatorString({"aten::div", "out_mode", 4}),
          }), // op_names
      }),
      ByteCodeFunctionWithOperator({
          mobile::Function::registerFunc(
              "div__Tensor_0_3",
              std::vector<Instruction>({
                  Instruction{OpCode::STOREN, 1, 2},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::JF, 3, 0},
                  Instruction{OpCode::LOADC, 1, 0},
                  Instruction{OpCode::JMP, 3, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::STORE, 3, 0},
                  Instruction{OpCode::MOVE, 3, 0},
                  Instruction{OpCode::JF, 5, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 1, 0},
                  Instruction{OpCode::JMP, 5, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::LOADC, 0, 0},
                  Instruction{OpCode::OP, 2, 0},
                  Instruction{OpCode::STORE, 4, 0},
                  Instruction{OpCode::DROPR, 2, 0},
                  Instruction{OpCode::DROPR, 1, 0},
                  Instruction{OpCode::MOVE, 4, 0},
                  Instruction{OpCode::RET, 0, 0},
              }), // instructions_
              std::vector<c10::IValue>({
                  c10::IValue("trunc"),
                  c10::IValue(true),
              }), // constants
              std::vector<c10::TypePtr>(), // types
              4 // register_size_
              ),
          std::vector<OperatorString>({
              OperatorString({"aten::is_floating_point", "", 1}),
              OperatorString({"aten::div_", "Tensor", 2}),
              OperatorString({"aten::div_", "Tensor_mode", 3}),
          }), // op_names
      }),
      ByteCodeFunctionWithOperator({
          mobile::Function::registerFunc(
              "div__Scalar_0_3",
              std::vector<Instruction>({
                  Instruction{OpCode::STOREN, 1, 2},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::OP, 0, 0},
                  Instruction{OpCode::JF, 3, 0},
                  Instruction{OpCode::LOADC, 1, 0},
                  Instruction{OpCode::JMP, 3, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::ISINSTANCE, 0, 1},
                  Instruction{OpCode::STORE, 3, 0},
                  Instruction{OpCode::MOVE, 3, 0},
                  Instruction{OpCode::JF, 5, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 1, 0},
                  Instruction{OpCode::JMP, 6, 0},
                  Instruction{OpCode::LOAD, 1, 0},
                  Instruction{OpCode::LOAD, 2, 0},
                  Instruction{OpCode::OP, 2, 0},
                  Instruction{OpCode::LOADC, 0, 0},
                  Instruction{OpCode::OP, 3, 0},
                  Instruction{OpCode::STORE, 4, 0},
                  Instruction{OpCode::DROPR, 2, 0},
                  Instruction{OpCode::DROPR, 1, 0},
                  Instruction{OpCode::MOVE, 4, 0},
                  Instruction{OpCode::RET, 0, 0},
              }), // instructions_
              std::vector<c10::IValue>({
                  c10::IValue("trunc"),
                  c10::IValue(true),
              }), // constants
              std::vector<c10::TypePtr>(), // types
              4 // register_size_
              ),
          std::vector<OperatorString>({
              OperatorString({"aten::is_floating_point", "", 1}),
              OperatorString({"aten::div_", "Scalar", 2}),
              OperatorString({"prim::unchecked_cast", "", 1}),
              OperatorString({"aten::div_", "Scalar_mode", 3}),
          }), // op_names
      }),
  });
  return upgraderBytecodeList;
}

} // namespace jit
} // namespace torch
