#pragma once

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "ReportServerInterface.h"
#include "Structures.h"
#include "ast/Ast.hpp"
#include "rapidjson/document.h"
#include "structures/ReportStructures.h"

using namespace ast;

namespace utils {
    void CreateUI(const ast::Node&                    node,
                  rapidjson::Value&                   response,
                  rapidjson::Document::AllocatorType& allocator);

    std::string FormatTimestampToString(const time_t&      timestamp,
                                        const std::string& format = "%Y.%m.%d %H:%M:%S");

    double TruncateDouble(const double& value, const int& digits);

    std::string FormatDouble(const double value, const int digits);

    std::string ConvertCmdToString(const int cmd);

    double GetMarketPriceByCmd(const int cmd, const ReportSymbolRecord& symbol_record);

} // namespace utils