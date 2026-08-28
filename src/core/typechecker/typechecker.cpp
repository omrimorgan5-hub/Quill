#pragma once

#include <stdexcept>
#include <string>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../ast/ast.cpp"

class TypeChecker {
public:
    // functionReturnTypes is the transpiler's own already-resolved map
    // of function name -> return type (explicit annotation, or
    // inferred from the function's own return statements when one
    // wasn't given) -- see buildFunctionReturnTypes() in trans.cpp.
    // Reusing it here means this checker and the code actually emitted
    // by the transpiler can never disagree about what a function
    // returns.
    void check(const Program& program, const std::map<std::string, std::string>& functionReturnTypes) {
        this->functionReturnTypes = functionReturnTypes;
        for (::Node* stmt : program.body) {
            checkNode(stmt);
        }
    }

private:
    std::unordered_map<std::string, std::string> symbols;
    std::map<std::string, std::string> functionReturnTypes;
    std::map<std::string, std::map<std::string, std::string>> structFields;

    void checkNode(::Node* node) {
        if (!node) return;

        if (auto* decl = dynamic_cast<VariableDeclaration*>(node)) {
            checkVariableDeclaration(decl);
            return;
        }

        if (auto* assign = dynamic_cast<Assignment*>(node)) {
            checkAssignment(assign);
            return;
        }

        if (auto* expr = dynamic_cast<ExpressionStatement*>(node)) {
            checkNode(expr->expression);
            return;
        }

        if (auto* fn = dynamic_cast<Function*>(node)) {
            auto saved = symbols;
            for (const Param& param : fn->params) {
                symbols[param.name] = param.type;
            }

            for (::Node* stmt : fn->body) {
                checkNode(stmt);
            }

            symbols = saved;
            return;
        }

        if (auto* ifStmt = dynamic_cast<IfStatement*>(node)) {
            checkNode(ifStmt->condition);
            for (::Node* stmt : ifStmt->consequent) checkNode(stmt);
            for (::Node* stmt : ifStmt->alternate) checkNode(stmt);
            return;
        }

        if (auto* whileLoop = dynamic_cast<WhileLoop*>(node)) {
            checkNode(whileLoop->condition);
            for (::Node* stmt : whileLoop->body) checkNode(stmt);
            return;
        }

        if (auto* fl = dynamic_cast<ForLoop*>(node)) {
            auto saved = symbols;
            symbols[fl->iterator] = "number";
            if (fl->collection) {
                checkNode(fl->collection);
                std::string ct = inferType(fl->collection);
                auto b = ct.find('[');
                if (b != std::string::npos)
                    symbols[fl->iterator] = ct.substr(0, b);
            }
            if (fl->start) checkNode(fl->start);
            if (fl->end) checkNode(fl->end);
            for (::Node* stmt : fl->body) checkNode(stmt);
            symbols = saved;
            return;
        }

        if (auto* sd = dynamic_cast<StructDecl*>(node)) {
            std::map<std::string, std::string> fields;
            for (const auto& f : sd->fields) {
                fields[f.name] = f.type.empty() ? "number" : f.type;
            }
            structFields[sd->name] = fields;
            return;
        }

        if (auto* fa = dynamic_cast<FieldAccess*>(node)) {
            checkNode(fa->object);
            return;
        }
        if (auto* fas = dynamic_cast<FieldAssignment*>(node)) {
            checkNode(fas->object);
            checkNode(fas->value);
            return;
        }
        if (auto* sl = dynamic_cast<StructLiteral*>(node)) {
            for (::Node* v : sl->fieldValues) checkNode(v);
            return;
        }

        if (auto* ret = dynamic_cast<ReturnStatement*>(node)) {
            checkNode(ret->value);
            return;
        }

        if (auto* print = dynamic_cast<PrintStatement*>(node)) {
            checkNode(print->expression);
            return;
        }

        if (auto* call = dynamic_cast<CallExpression*>(node)) {
            for (::Node* arg : call->arguments) {
                checkNode(arg);
            }
            return;
        }

        if (auto* bin = dynamic_cast<BinaryExpression*>(node)) {
            checkNode(bin->left);
            checkNode(bin->right);
            return;
        }

        if (auto* unary = dynamic_cast<UnaryExpression*>(node)) {
            checkNode(unary->operand);
            return;
        }

        if (auto* idx = dynamic_cast<IndexExpression*>(node)) {
            checkNode(idx->object);
            checkNode(idx->index);
            std::string objType = inferType(idx->object);
            bool isArray = objType.find('[') != std::string::npos;
            bool isMap = objType.rfind("map[", 0) == 0;
            if (objType != "string" && !isArray && !isMap) {
                throw std::runtime_error("indexing with [] is only supported on string, array, or map values");
            }
            return;
        }

        if (auto* idxAssign = dynamic_cast<IndexAssignment*>(node)) {
            checkNode(idxAssign->object);
            checkNode(idxAssign->index);
            checkNode(idxAssign->value);

            std::string objType = inferType(idxAssign->object);
            if (objType.rfind("map[", 0) == 0) {
                auto comma = objType.find(',');
                auto rb = objType.rfind(']');
                std::string valT = "number";
                if (comma != std::string::npos && rb != std::string::npos)
                    valT = objType.substr(comma + 1, rb - comma - 1);
                std::string valueType = inferType(idxAssign->value);
                if (valueType != valT && valueType != "unknown") {
                    throw std::runtime_error("type mismatch assigning into map");
                }
                return;
            }
            auto bracketPos = objType.find('[');
            if (bracketPos == std::string::npos) {
                throw std::runtime_error("assignment with [] is only supported on array or map values");
            }

            std::string elemType = objType.substr(0, bracketPos);
            std::string valueType = inferType(idxAssign->value);
            if (valueType != elemType) {
                throw std::runtime_error("type mismatch assigning into array");
            }
            return;
        }

        if (auto* lit = dynamic_cast<ArrayLiteral*>(node)) {
            for (::Node* el : lit->elements) checkNode(el);
            return;
        }

        if (auto* ident = dynamic_cast<Identifier*>(node)) {
            if (symbols.find(ident->name) == symbols.end()) {
                throw std::runtime_error("undefined identifier: " + ident->name);
            }
            return;
        }
    }

    // C_call()/C_top() splice raw, unverifiable C text -- Quill has no
    // way to know their real type, so (the same trust model as `extern
    // func`) the student's own annotation is trusted rather than
    // checked against a guess that can only ever be wrong.
    bool isRawCSplice(::Node* node) {
        auto* call = dynamic_cast<CallExpression*>(node);
        return call && (call->callee == "C_call" || call->callee == "C_top");
    }

    void checkVariableDeclaration(VariableDeclaration* decl) {
        if (!decl->value) {
            // Arrays (and any other declaration without an initializer)
            // still need to be registered so later uses aren't flagged
            // as undefined identifiers.
            if (!decl->declaredType.empty()) {
                symbols[decl->identifier] = decl->declaredType;
            }
            return;
        }
        checkNode(decl->value);

        if (isRawCSplice(decl->value)) {
            symbols[decl->identifier] = decl->declaredType.empty() ? "unknown" : decl->declaredType;
            return;
        }

        std::string valueType = inferType(decl->value);
        if (!decl->declaredType.empty()) {
            bool declArr = decl->declaredType.find('[') != std::string::npos;
            bool valArr = valueType.find('[') != std::string::npos;
            if (declArr && valArr) {
                std::string declElem = decl->declaredType.substr(0, decl->declaredType.find('['));
                std::string valElem = valueType.substr(0, valueType.find('['));
                if (declElem != valElem)
                    throw std::runtime_error("type mismatch for variable: " + decl->identifier);
            } else if (decl->declaredType != valueType && valueType != "unknown") {
                // allow struct name match when value is struct literal
                if (!(structFields.count(decl->declaredType) && valueType == decl->declaredType))
                    throw std::runtime_error("type mismatch for variable: " + decl->identifier);
            }
            symbols[decl->identifier] = decl->declaredType;
        } else {
            symbols[decl->identifier] = valueType;
        }
    }

    void checkAssignment(Assignment* assign) {
        auto it = symbols.find(assign->identifier);
        if (it == symbols.end()) {
            throw std::runtime_error("assignment to undefined variable: " + assign->identifier);
        }

        if (!assign->value) return;
        checkNode(assign->value);

        if (isRawCSplice(assign->value)) {
            return;
        }

        std::string valueType = inferType(assign->value);
        if (valueType != it->second) {
            throw std::runtime_error("type mismatch on assignment to: " + assign->identifier);
        }
    }

    std::string inferType(::Node* node) {
        if (!node) return "void";

        if (dynamic_cast<LiteralInt*>(node)) return "number";
        if (dynamic_cast<LiteralDoudle*>(node)) return "double";
        if (dynamic_cast<LiteralString*>(node)) return "string";
        if (dynamic_cast<LiteralBool*>(node)) return "bool";
        if (dynamic_cast<LiteralChar*>(node)) return "char";

        if (auto* ident = dynamic_cast<Identifier*>(node)) {
            auto it = symbols.find(ident->name);
            if (it != symbols.end()) return it->second;
            return "unknown";
        }

        if (auto* idx = dynamic_cast<IndexExpression*>(node)) {
            std::string objType = inferType(idx->object);
            if (objType.rfind("map[", 0) == 0) {
                auto comma = objType.find(',');
                auto rb = objType.rfind(']');
                if (comma != std::string::npos && rb != std::string::npos)
                    return objType.substr(comma + 1, rb - comma - 1);
                return "number";
            }
            auto bracketPos = objType.find('[');
            if (bracketPos != std::string::npos)
                return objType.substr(0, bracketPos);
            if (objType == "string")
                return "number";
            return "number";
        }

        if (auto* lit = dynamic_cast<ArrayLiteral*>(node)) {
            if (lit->elements.empty())
                return "number[]";
            std::string elem = inferType(lit->elements[0]);
            for (size_t i = 1; i < lit->elements.size(); i++) {
                std::string t = inferType(lit->elements[i]);
                if (t != elem) {
                    if ((elem == "number" && t == "double") || (elem == "double" && t == "number"))
                        elem = "double";
                    else
                        return "unknown";
                }
            }
            return elem + "[]";
        }

        if (auto* fa = dynamic_cast<FieldAccess*>(node)) {
            std::string ot = inferType(fa->object);
            auto it = structFields.find(ot);
            if (it != structFields.end()) {
                auto ft = it->second.find(fa->field);
                if (ft != it->second.end()) return ft->second;
            }
            return "unknown";
        }
        if (auto* sl = dynamic_cast<StructLiteral*>(node)) {
            return sl->name;
        }

        if (auto* binary = dynamic_cast<BinaryExpression*>(node)) {
            std::string left = inferType(binary->left);
            std::string right = inferType(binary->right);
            if (left == right) return left;
            if ((left == "number" && right == "double") || (left == "double" && right == "number")) return "double";
            return "unknown";
        }

        if (auto* unary = dynamic_cast<UnaryExpression*>(node)) {
            return inferType(unary->operand);
        }

        if (auto* call = dynamic_cast<CallExpression*>(node)) {
            auto it = functionReturnTypes.find(call->callee);
            return it != functionReturnTypes.end() ? it->second : "unknown";
        }
        if (dynamic_cast<Assignment*>(node)) return "void";

        return "unknown";
    }
};