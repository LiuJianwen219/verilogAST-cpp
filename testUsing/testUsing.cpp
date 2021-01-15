//
// Created by ljw on 2021/1/15.
//
#include <iostream>
using namespace std;

#include "verilogAST.hpp"
namespace vAST = verilogAST;

#include "verilogAST/transformer.hpp"

vAST::Parameters make_simple_params();
std::unique_ptr<vAST::Connections> make_simple_connections();
std::vector<std::variant<std::unique_ptr<vAST::StructuralStatement>,
    std::unique_ptr<vAST::Declaration>>>
make_simple_body();
std::vector<std::unique_ptr<vAST::BehavioralStatement>>
make_simple_always_body();
void test01();

int main(){
  cout << "hello verilog AST!" << endl;
  vAST::NumericLiteral n0("23", 16, false, vAST::DECIMAL);
  cout << n0.toString() << endl;

  test01();

  return 0;
}



vAST::Parameters make_simple_params() {
  vAST::Parameters parameters;
  parameters.push_back(
      std::make_pair(vAST::make_id("param0"), vAST::make_num("0")));
  parameters.push_back(
      std::make_pair(vAST::make_id("param1"), vAST::make_num("1")));
  return parameters;
}

std::unique_ptr<vAST::Connections> make_simple_connections() {
  std::unique_ptr<vAST::Connections> connections =
      std::make_unique<vAST::Connections>();
  connections->insert("a", vAST::make_id("a"));
  connections->insert("b", std::make_unique<vAST::Index>(vAST::make_id("b"),
                                                         vAST::make_num("0")));
  connections->insert(
      "c", std::make_unique<vAST::Slice>(
          vAST::make_id("c"), vAST::make_num("31"), vAST::make_num("0")));

  return connections;
}

std::vector<std::variant<std::unique_ptr<vAST::StructuralStatement>,
    std::unique_ptr<vAST::Declaration>>>
make_simple_body() {
  std::vector<std::variant<std::unique_ptr<vAST::StructuralStatement>,
      std::unique_ptr<vAST::Declaration>>>
      body;

  std::string module_name = "other_module";
  std::string instance_name = "other_module_inst";

  body.push_back(std::make_unique<vAST::ModuleInstantiation>(
      module_name, make_simple_params(), instance_name,
      make_simple_connections()));
  return body;
}

std::vector<std::unique_ptr<vAST::BehavioralStatement>>
make_simple_always_body() {
  std::vector<std::unique_ptr<vAST::BehavioralStatement>> body;
  body.push_back(std::make_unique<vAST::BlockingAssign>(
      std::make_unique<vAST::Identifier>("a"),
      std::make_unique<vAST::Identifier>("b")));
  body.push_back(std::make_unique<vAST::NonBlockingAssign>(
      std::make_unique<vAST::Identifier>("b"),
      std::make_unique<vAST::Identifier>("c")));
  std::vector<std::unique_ptr<vAST::Expression>> args;
  args.push_back(std::make_unique<vAST::String>("b=%d, c=%d"));
  args.push_back(std::make_unique<vAST::Identifier>("b"));
  args.push_back(std::make_unique<vAST::Identifier>("c"));
  body.push_back(std::make_unique<vAST::CallStmt>("$display", std::move(args)));

  std::vector<std::unique_ptr<vAST::BehavioralStatement>> true_body;
  true_body.push_back(std::make_unique<vAST::BlockingAssign>(
      std::make_unique<vAST::Identifier>("e"),
      std::make_unique<vAST::Identifier>("f")));

  std::vector<
      std::pair<std::unique_ptr<vAST::Expression>,
          std::vector<std::unique_ptr<vAST::BehavioralStatement>>>>
      else_ifs;
  for (int i = 0; i < 2; i++) {
    std::unique_ptr<vAST::Expression> cond =
        vAST::make_id("x" + std::to_string(i));
    std::vector<std::unique_ptr<vAST::BehavioralStatement>> body;
    body.push_back(std::make_unique<vAST::BlockingAssign>(
        std::make_unique<vAST::Identifier>("e"),
        std::make_unique<vAST::Identifier>("g" + std::to_string(i))));
    else_ifs.push_back({std::move(cond), std::move(body)});
  }

  std::vector<std::unique_ptr<vAST::BehavioralStatement>> else_body;
  else_body.push_back(std::make_unique<vAST::BlockingAssign>(
      std::make_unique<vAST::Identifier>("e"),
      std::make_unique<vAST::Identifier>("g")));

  body.push_back(
      std::make_unique<vAST::If>(vAST::make_id("b"), std::move(true_body),
                                 std::move(else_ifs), std::move(else_body)));

  return body;
}

class ModuleTransformer : public vAST::Transformer {
 public:
  using vAST::Transformer::visit;
  virtual std::unique_ptr<vAST::Identifier> visit(
      std::unique_ptr<vAST::Identifier> node) {
    if (node->value == "c") {
      return vAST::make_id("g");
    } else if (node->value == "param0") {
      return vAST::make_id("y");
    }
    return node;
  };
};

void test01(){
  std::vector<std::unique_ptr<vAST::AbstractPort>> ports;
  ports.push_back(std::make_unique<vAST::Port>(vAST::make_id("i"), vAST::INPUT,
                                               vAST::WIRE));
  std::vector<std::pair<std::unique_ptr<vAST::Expression>,
      std::unique_ptr<vAST::Expression>>>
      outer_dims;
  outer_dims.push_back({vAST::make_num("7"), vAST::make_num("0")});
  outer_dims.push_back({vAST::make_id("c"), vAST::make_num("0")});
  ports.push_back(std::make_unique<vAST::Port>(
      std::make_unique<vAST::NDVector>(vAST::make_id("o"), vAST::make_num("3"),
                                       vAST::make_id("c"),
                                       std::move(outer_dims)),
      vAST::OUTPUT, vAST::WIRE));

  ports.push_back(
      std::make_unique<vAST::StringPort>("output reg [width-1:0] k"));

  std::vector<std::variant<std::unique_ptr<vAST::StructuralStatement>,
      std::unique_ptr<vAST::Declaration>>>
      body = make_simple_body();

  body.push_back(std::make_unique<vAST::ContinuousAssign>(
      std::make_unique<vAST::Identifier>("c"),
      std::make_unique<vAST::Identifier>("b")));

  body.push_back(
      std::make_unique<vAST::Wire>(std::make_unique<vAST::Identifier>("c")));

  body.push_back(
      std::make_unique<vAST::Reg>(std::make_unique<vAST::Identifier>("c")));

  std::vector<std::variant<
      std::unique_ptr<vAST::Identifier>, std::unique_ptr<vAST::PosEdge>,
      std::unique_ptr<vAST::NegEdge>, std::unique_ptr<vAST::Star>>>
      sensitivity_list;
  sensitivity_list.push_back(std::make_unique<vAST::Identifier>("a"));

  body.push_back(std::make_unique<vAST::Always>(std::move(sensitivity_list),
                                                make_simple_always_body()));

  body.push_back(std::make_unique<vAST::SingleLineComment>("Test comment"));
  body.push_back(
      std::make_unique<vAST::BlockComment>("Test comment\non multiple lines"));

  body.push_back(
      std::make_unique<vAST::InlineVerilog>("// Test inline verilog"));

  std::unique_ptr<vAST::AbstractModule> module = std::make_unique<vAST::Module>(
      "test_module0", std::move(ports), std::move(body), make_simple_params());
//  std::string expected_str =
//      "module test_module0 #(\n"
//      "    parameter y = 0,\n"
//      "    parameter param1 = 1\n"
//      ") (\n"
//      "    input i,\n"
//      "    output [3:g] o [7:0][g:0],\n"
//      "    output reg [width-1:0] k\n"
//      ");\n"
//      "other_module #(\n"
//      "    .y(0),\n"
//      "    .param1(1)\n"
//      ") other_module_inst (\n"
//      "    .a(a),\n"
//      "    .b(b[0]),\n"
//      "    .c(g[31:0])\n"
//      ");\n"
//      "assign g = b;\n"
//      "wire g;\n"
//      "reg g;\n"
//      "always @(a) begin\n"
//      "a = b;\n"
//      "b <= g;\n"
//      "$display(\"b=%d, c=%d\", b, g);\n"
//      "if (b) begin\n"
//      "    e = f;\n"
//      "end else if (x0) begin\n"
//      "    e = g0;\n"
//      "end else if (x1) begin\n"
//      "    e = g1;\n"
//      "end else begin\n"
//      "    e = g;\n"
//      "end\n"
//      "end\n\n"
//      "// Test comment\n"
//      "/*\nTest comment\non multiple lines\n*/\n"
//      "// Test inline verilog\n"
//      "endmodule\n";

  ModuleTransformer transformer;
  cout << transformer.visit(std::move(module))->toString() << endl;
//  EXPECT_EQ(transformer.visit(std::move(module))->toString(), expected_str);
}

