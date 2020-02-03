#include "verilogAST/assign_inliner.hpp"
#include <iostream>

namespace verilogAST {

std::unique_ptr<Identifier> WireReadCounter::visit(
    std::unique_ptr<Identifier> node) {
  this->read_count[node->toString()]++;
  return node;
}

std::unique_ptr<Declaration> WireReadCounter::visit(
    std::unique_ptr<Declaration> node) {
  return node;
}

std::unique_ptr<ContinuousAssign> WireReadCounter::visit(
    std::unique_ptr<ContinuousAssign> node) {
  node->value = this->visit(std::move(node->value));
  return node;
}

std::unique_ptr<Port> AssignMapBuilder::visit(std::unique_ptr<Port> node) {
  std::string port_str = std::visit(
      [](auto&& value) -> std::string {
        if (auto ptr = dynamic_cast<Identifier*>(value.get())) {
          return ptr->toString();
        } else if (auto ptr = dynamic_cast<Vector*>(value.get())) {
          return ptr->id->toString();
        }
        throw std::runtime_error("Unreachable");  // LCOV_EXCL_LINE
        return "";                                // LCOV_EXCL_LINE
      },
      node->value);
  if (node->direction != Direction::INPUT) {
    this->non_input_ports.insert(port_str);
    if (node->direction == Direction::OUTPUT) {
      this->output_ports.insert(port_str);
    }
  } else {
    this->input_ports.insert(port_str);
  }
  return node;
}

std::unique_ptr<ContinuousAssign> AssignMapBuilder::visit(
    std::unique_ptr<ContinuousAssign> node) {
  node = Transformer::visit(std::move(node));
  std::string key =
      std::visit([](auto&& value) -> std::string { return value->toString(); },
                 node->target);
  this->assign_map[key] = node->value->clone();
  this->assign_count[key]++;
  return node;
}

std::unique_ptr<Expression> AssignInliner::visit(
    std::unique_ptr<Expression> node) {
  if (auto ptr = dynamic_cast<Identifier*>(node.get())) {
    node.release();
    std::unique_ptr<Identifier> id(ptr);
    std::string key = id->toString();
    std::map<std::string, std::unique_ptr<Expression>>::iterator it =
        assign_map.find(key);
    if (it != assign_map.end() && (this->assign_count[key] == 1) &&
        (this->read_count[id->toString()] == 1 ||
         dynamic_cast<Identifier*>(it->second.get()) ||
         dynamic_cast<NumericLiteral*>(it->second.get()))) {
      return this->visit(it->second->clone());
    }
    return id;
  }
  return Transformer::visit(std::move(node));
}

std::unique_ptr<Wire> AssignInliner::visit(std::unique_ptr<Wire> node) {
  bool remove = false;
  std::visit(
      [&](auto&& value) {
        if (auto ptr = dynamic_cast<Identifier*>(value.get())) {
          std::string key = ptr->toString();
          std::map<std::string, std::unique_ptr<Expression>>::iterator it =
              this->assign_map.find(key);
          if (it != assign_map.end() && (this->assign_count[key] == 1) &&
              (this->read_count[key] == 1 ||
               dynamic_cast<Identifier*>(it->second.get()) ||
               dynamic_cast<NumericLiteral*>(it->second.get()))) {
            remove = true;
          };
        } else if (auto ptr = dynamic_cast<Vector*>(value.get())) {
          std::string key = ptr->id->toString();
          std::map<std::string, std::unique_ptr<Expression>>::iterator it =
              this->assign_map.find(key);
          if (it != assign_map.end() && (this->assign_count[key] == 1) &&
              (this->read_count[key] == 1 ||
               dynamic_cast<Identifier*>(it->second.get()) ||
               dynamic_cast<NumericLiteral*>(it->second.get()))) {
            remove = true;
          };
        }
      },
      node->value);
  if (remove) {
    return std::unique_ptr<Wire>{};
  }
  node->value = this->visit(std::move(node->value));
  return node;
}

std::unique_ptr<ContinuousAssign> AssignInliner::visit(
    std::unique_ptr<ContinuousAssign> node) {
  node->value = this->visit(std::move(node->value));
  std::string key =
      std::visit([](auto&& value) -> std::string { return value->toString(); },
                 node->target);
  bool remove = false;
  std::visit(
      [&](auto&& value) {
        if (auto ptr = dynamic_cast<Identifier*>(value.get())) {
          std::map<std::string, std::unique_ptr<Expression>>::iterator it =
              this->assign_map.find(ptr->toString());
          if (it != assign_map.end() && (this->assign_count[key] == 1) &&
              (this->read_count[ptr->toString()] == 1 ||
               dynamic_cast<Identifier*>(it->second.get()) ||
               dynamic_cast<NumericLiteral*>(it->second.get())) &&
              this->non_input_ports.count(ptr->toString()) == 0) {
            remove = true;
          } else if (this->inlined_outputs.count(ptr->toString())) {
            remove = true;
          };
        }
      },
      node->target);
  if (remove) {
    return std::unique_ptr<ContinuousAssign>{};
  }
  return node;
}

std::vector<std::variant<std::unique_ptr<StructuralStatement>,
                         std::unique_ptr<Declaration>>>
AssignInliner::do_inline(
    std::vector<std::variant<std::unique_ptr<StructuralStatement>,
                             std::unique_ptr<Declaration>>>
        body) {
  std::vector<std::variant<std::unique_ptr<StructuralStatement>,
                           std::unique_ptr<Declaration>>>
      new_body;
  for (auto&& item : body) {
    std::variant<std::unique_ptr<StructuralStatement>,
                 std::unique_ptr<Declaration>>
        result = this->visit(std::move(item));
    bool is_null;
    std::visit(
        [&](auto&& value) {
          if (value) {
            is_null = false;
          } else {
            is_null = true;
          }
        },
        result);
    if (!is_null) {
      new_body.push_back(std::move(result));
    }
  }
  return new_body;
}

std::unique_ptr<Module> AssignInliner::visit(std::unique_ptr<Module> node) {
  AssignMapBuilder builder(this->assign_count, this->assign_map,
                           this->non_input_ports, this->output_ports,
                           this->input_ports);
  node = builder.visit(std::move(node));

  WireReadCounter counter(this->read_count);
  node = counter.visit(std::move(node));

  std::vector<std::unique_ptr<AbstractPort>> new_ports;
  for (auto&& item : node->ports) {
    new_ports.push_back(this->visit(std::move(item)));
  }
  node->ports = std::move(new_ports);

  node->body = this->do_inline(std::move(node->body));
  // Now "reverse inline" output wires
  for (auto output : this->output_ports) {
    std::unique_ptr<Expression> value = this->assign_map[output]->clone();
    this->assign_map.erase(output);
    if (dynamic_cast<Identifier*>(value.get()) &&
        this->assign_count[value->toString()] == 0 &&
        this->input_ports.count(value->toString()) == 0) {
      this->assign_map[value->toString()] = make_id(output);
      this->assign_count[value->toString()]++;
      this->inlined_outputs.insert(output);
    }
  }
  node->body = this->do_inline(std::move(node->body));
  return node;
}

}  // namespace verilogAST