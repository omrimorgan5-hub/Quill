#pragma once

#include <string>

enum class TokenType {
    Identifier,
    Number,
    Double,
    String,
    Char,
    FString,
    Keyword,
    Operator,
    EOF_T,
    Illegal,
    Comment,
    Dot,
    Incrementation,
    Function,
    Boolean,
    Conditional,
    Loop,
    Integer,
    Punctuation
};

struct Token {
    TokenType type = TokenType::EOF_T;
    std::string value;
};