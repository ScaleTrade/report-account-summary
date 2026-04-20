#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#include "rapidjson/document.h"
#include "utils/Utils.h"
#include "ReportServerInterface.h"
#include "Structures.h"
#include "ast/Ast.hpp"
#include "sbxTableBuilder/SBXTableBuilder.hpp"
#include "structures/ReportStructures.h"
#include "structures/ReportType.h"

extern "C" {
    void AboutReport(rapidjson::Value& request,
                     rapidjson::Value& response,
                     rapidjson::Document::AllocatorType& allocator,
                     ReportServerInterface* server);

    void DestroyReport();

    void CreateReport(rapidjson::Value& request,
                     rapidjson::Value& response,
                     rapidjson::Document::AllocatorType& allocator,
                     ReportServerInterface* server);
}