#ifndef EASYLANG_LEXERTESTS_H
#define EASYLANG_LEXERTESTS_H

#define CATCH_CONFIG_RUNNER
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cmath>

#include "../Catch.h"
#include "../Lexer.h"

using namespace std;

TEST_CASE( "Double test" ) {
Tokinizer* parser = new StandartTokinizer();
auto tokens = make_shared<std::vector<Token*>>();
parser->Parse(L".1", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::DOUBLE);
REQUIRE(reinterpret_cast<DoubleToken*>(tokens->at(0))->Value == 0.1);

parser->Parse(L"1.1", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::DOUBLE);
REQUIRE(reinterpret_cast<DoubleToken*>(tokens->at(0))->Value == 1.1);

parser->Parse(L"1990.0001", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::DOUBLE);
REQUIRE(reinterpret_cast<DoubleToken*>(tokens->at(0))->Value == 1990.0001);

parser->Parse(L"-10.1", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::DOUBLE);
REQUIRE(reinterpret_cast<DoubleToken*>(tokens->at(0))->Value == -10.1);

parser->Parse(L"-.1", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::DOUBLE);
REQUIRE(reinterpret_cast<DoubleToken*>(tokens->at(0))->Value == -0.1);
}

TEST_CASE( "Integer test" ) {
Tokinizer* parser = new StandartTokinizer();
auto tokens = make_shared<std::vector<Token*>>();
parser->Parse(L"1", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(0))->Value == 1);

parser->Parse(L"12345", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(0))->Value == 12345);

parser->Parse(L"-12345", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(0))->Value == -12345);
}

TEST_CASE( "String test" ) {
Tokinizer* parser = new StandartTokinizer();
auto tokens = make_shared<std::vector<Token*>>();
parser->Parse(L"\"erhan\"", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::TEXT);
REQUIRE(reinterpret_cast<TextToken*>(tokens->at(0))->Value == L"erhan");

parser->Parse(L"\"'erhan'\"", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::TEXT);
REQUIRE(reinterpret_cast<TextToken*>(tokens->at(0))->Value == L"'erhan'");

parser->Parse(L"\"\\\"erhan\\\"\"", tokens);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::TEXT);
REQUIRE(reinterpret_cast<TextToken*>(tokens->at(0))->Value == L"\"erhan\"");

SECTION( "Atama" ) {
parser->Parse(L"erhan'a 15", tokens);
REQUIRE(tokens->size() == 4);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::SYMBOL);
REQUIRE(reinterpret_cast<TextToken*>(tokens->at(0))->Value == L"erhan");

REQUIRE(tokens->at(1)->GetType() == EASY_TOKEN_TYPE::OPERATOR);
REQUIRE(reinterpret_cast<OperatorToken*>(tokens->at(1))->Value == EASY_OPERATOR_TYPE::SINGLE_QUOTES);

REQUIRE(tokens->at(2)->GetType() == EASY_TOKEN_TYPE::KEYWORD);
REQUIRE(reinterpret_cast<KeywordToken*>(tokens->at(2))->Value == EASY_KEYWORD_TYPE::ASSIGNMENT_SUFFIX);

REQUIRE(tokens->at(3)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(3))->Value == 15);
}

SECTION( "Sembol" ) {
parser->Parse(L"17 <= 40", tokens);
REQUIRE(tokens->size() == 3);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(0))->Value == 17);

REQUIRE(tokens->at(1)->GetType() == EASY_TOKEN_TYPE::OPERATOR);
REQUIRE(reinterpret_cast<OperatorToken*>(tokens->at(1))->Value == EASY_OPERATOR_TYPE::LOWER_EQUAL);

REQUIRE(tokens->at(2)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(2))->Value == 40);
}

SECTION( "17 büyüktür 40" ) {
parser->Parse(L"17 büyüktür 40", tokens);
REQUIRE(tokens->size() == 3);
REQUIRE(tokens->at(0)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(0))->Value == 17);

REQUIRE(tokens->at(1)->GetType() == EASY_TOKEN_TYPE::OPERATOR);
REQUIRE(reinterpret_cast<OperatorToken*>(tokens->at(1))->Value == EASY_OPERATOR_TYPE::GREATOR);

REQUIRE(tokens->at(2)->GetType() == EASY_TOKEN_TYPE::INTEGER);
REQUIRE(reinterpret_cast<IntegerToken*>(tokens->at(2))->Value == 40);
}
}

#endif