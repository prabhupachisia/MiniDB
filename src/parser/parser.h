#pragma once
#include <memory>
#include <string>
#include "ast.h"

std::unique_ptr<Query> parse_query(const std::string& input);