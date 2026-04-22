#include "PluginInterface.h"

using namespace ast;

extern "C" void AboutReport(rapidjson::Value&                   request,
                            rapidjson::Value&                   response,
                            rapidjson::Document::AllocatorType& allocator,
                            ReportServerInterface*              server) {
    response.AddMember("version", 1, allocator);
    response.AddMember("name", Value().SetString("Account Summary report", allocator), allocator);
    response.AddMember(
        "description",
        Value().SetString(
            "Summary of a selected account over a specified period. "
            "Includes account details, balance, equity, margin, open and closed trades, "
            "pending orders, and financial transactions.",
            allocator),
        allocator);
    response.AddMember("type", static_cast<int>(ReportType::RangeAccount), allocator);
    response.AddMember("key", Value().SetString("ACCOUNT_SUMMARY_REPORT", allocator), allocator);
}

extern "C" void DestroyReport() {}

extern "C" void CreateReport(rapidjson::Value&                   request,
                             rapidjson::Value&                   response,
                             rapidjson::Document::AllocatorType& allocator,
                             ReportServerInterface*              server) {
    int login = 0;
    int from  = 0;
    int to    = 0;

    if (request.HasMember("login") && request["login"].IsInt())
        login = request["login"].GetInt();

    if (request.HasMember("from") && request["from"].IsInt())
        from = request["from"].GetInt();

    if (request.HasMember("to") && request["to"].IsInt())
        to = request["to"].GetInt();

    ReportAccountRecord            account_record{};
    ReportGroupRecord              group_record{};
    ReportMarginLevel              margin_level{};
    std::vector<ReportTradeRecord> closed_trades_vector;
    std::vector<ReportTradeRecord> open_trades_vector;
    std::vector<ReportTradeRecord> pending_trades_vector;
    std::vector<ReportTradeRecord> transactions_vector;

    try {
        server->GetAccountByLogin(login, &account_record);
        server->GetGroup(account_record.group, &group_record);
        server->GetAccountBalanceByLogin(login, &margin_level);
        server->GetCloseTradesByLogin(login, &closed_trades_vector);
        server->GetOpenTradesByLogin(login, &open_trades_vector);
        server->GetPendingTradesByLogin(login, &pending_trades_vector);
        server->GetTransactionsByLogin(login, from, to, &transactions_vector);

        // Filtering for closed_trades_vector
        std::erase_if(closed_trades_vector, [from, to](const ReportTradeRecord& closed_trade) {
            return !(closed_trade.close_time >= from && closed_trade.close_time <= to);
        });
    } catch (const std::exception& e) {
        std::cerr << "[AccountSummaryReport]: " << e.what() << std::endl;

        response.SetObject();
        response.AddMember("error", "Failed to load account data", allocator);
        return;
    }

    // Main info
    const time_t      now = time(nullptr);
    const std::string period =
        utils::FormatTimestampToString(from) + " - " + utils::FormatTimestampToString(to);
    const std::string account_info = std::to_string(account_record.login) + " (" +
                                     account_record.group +
                                     ", 1:" + std::to_string(account_record.leverage) + ")";

    auto left_column =
        div({p({span({text("Name: ")}),
                span({text(account_record.name.empty() ? "-" : account_record.name)})}),
             p({span({text("Account: ")}), span({text(account_info)})}),
             p({span({text("Period: ")}), span({text(period)})}),
             p({span({text("Currency: ")}), span({text(group_record.currency)})}),
             p({span({text("Date: ")}), span({text(utils::FormatTimestampToString(now))})})},
            {{"style", "font-size: 14px"}});

    auto right_column = div(
        {p({span({text("Balance: ")}), span({text(utils::FormatDouble(margin_level.balance, 2))})}),
         p({span({text("Equity: ")}), span({text(utils::FormatDouble(margin_level.equity, 2))})}),
         p({span({text("Margin: ")}), span({text(utils::FormatDouble(margin_level.margin, 2))})}),
         p({span({text("Free Margin: ")}),
            span({text(utils::FormatDouble(margin_level.margin_free, 2))})}),
         p({span({text("Margin Level: ")}),
            span({text(utils::FormatDouble(margin_level.margin_level, 2))})})},
        {{"style", "font-size: 14px"}});

    auto main_info = div(
        {left_column, right_column},
        {{"style", "display:flex; gap:40px; align-items:flex-start; justify-content: center;"}});

    // Common table filters
    FilterConfig search_filter;
    search_filter.type = FilterType::Search;
    FilterConfig date_time_filter;
    date_time_filter.type = FilterType::DateTime;

    // Common totals
    std::unordered_map<std::string, ClosedOrdersTotal>  closed_orders_total_map;
    std::unordered_map<std::string, OpenOrdersTotal>    open_orders_total_map;
    std::unordered_map<std::string, PendingOrdersTotal> pending_orders_total_map;
    std::unordered_map<std::string, TransactionsTotal>  transactions_total_map;

    // Open orders table
    TableBuilder open_orders_table_builder("AccountOpenOrdersTable");

    open_orders_table_builder.SetIdColumn("order");
    open_orders_table_builder.SetOrderBy("order", "DESC");
    open_orders_table_builder.EnableAutoSave(false);
    open_orders_table_builder.EnableRefreshButton(false);
    open_orders_table_builder.EnableBookmarksButton(false);
    open_orders_table_builder.EnableExportButton(true);
    open_orders_table_builder.SetLimit(0);
    open_orders_table_builder.EnableTotal(true);
    open_orders_table_builder.SetTotalDataTitle("TOTAL");

    open_orders_table_builder.AddColumn({"order", "ORDER", 1, search_filter});
    open_orders_table_builder.AddColumn({"symbol", "SYMBOL", 2, search_filter});
    open_orders_table_builder.AddColumn({"login", "LOGIN", 3, search_filter});
    open_orders_table_builder.AddColumn({"type", "ORDER_TYPE", 4});
    open_orders_table_builder.AddColumn({"volume", "VOLUME", 5, search_filter});
    open_orders_table_builder.AddColumn({"open_time", "OPEN_TIME", 6, date_time_filter});
    open_orders_table_builder.AddColumn({"open_price", "OPEN_PRICE", 7, search_filter});
    open_orders_table_builder.AddColumn({"market_price", "MARKET_PRICE", 8, search_filter});
    open_orders_table_builder.AddColumn({"profit", "GROSS_PROFIT", 9, search_filter});
    open_orders_table_builder.AddColumn({"sl", "S / L", 10, search_filter});
    open_orders_table_builder.AddColumn({"tp", "T / P", 11, search_filter});
    open_orders_table_builder.AddColumn({"commission", "COMMISSION", 12, search_filter});
    open_orders_table_builder.AddColumn({"storage", "SWAP", 13, search_filter});
    open_orders_table_builder.AddColumn({"currency", "CURRENCY", 14, search_filter});
    open_orders_table_builder.AddColumn({"comment", "COMMENT", 15, search_filter});

    for (const auto& open_trade : open_trades_vector) {
        double             multiplier = 1; // for USD
        ReportSymbolRecord symbol_record{};

        // Conversion disabled
        // if (group_record.currency != "USD") {
        //     try {
        //         server->CalculateConvertRateByCurrency(
        //             group_record.currency, "USD", static_cast<int>(open_trade.cmd), &multiplier);
        //     } catch (const std::exception& e) {
        //         std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
        //     }
        // }

        try {
            server->GetSymbol(open_trade.symbol, &symbol_record);
        } catch (const std::exception& e) {
            std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
        }

        open_orders_total_map[group_record.currency].volume += open_trade.volume;
        open_orders_total_map[group_record.currency].profit += open_trade.profit * multiplier;
        open_orders_total_map[group_record.currency].commission +=
            open_trade.commission * multiplier;
        open_orders_total_map[group_record.currency].storage += open_trade.storage * multiplier;

        double market_price =
            utils::GetMarketPriceByCmd(static_cast<int>(open_trade.cmd), symbol_record);
        open_orders_table_builder.AddRow(
            {utils::TruncateDouble(open_trade.order, 0),
             open_trade.symbol,
             std::to_string(open_trade.login),
             utils::ConvertCmdToString(static_cast<int>(open_trade.cmd)),
             utils::TruncateDouble(open_trade.volume / 100.0, 2),
             utils::FormatTimestampToString(open_trade.open_time),
             utils::TruncateDouble(open_trade.open_price * multiplier, open_trade.digits),
             utils::TruncateDouble(market_price * multiplier, open_trade.digits),
             utils::TruncateDouble(open_trade.profit * multiplier, 2),
             utils::TruncateDouble(open_trade.sl * multiplier, 5),
             utils::TruncateDouble(open_trade.tp * multiplier, 5),
             utils::TruncateDouble(open_trade.commission * multiplier, 2),
             utils::TruncateDouble(open_trade.storage * multiplier, 2),
             group_record.currency,
             open_trade.comment});
    }

    JSONArray open_orders_total_array;
    open_orders_total_array.emplace_back(JSONObject{
        {"volume",
         utils::TruncateDouble(open_orders_total_map[group_record.currency].volume / 100.0, 2)},
        {"profit", utils::TruncateDouble(open_orders_total_map[group_record.currency].profit, 2)},
        {"commission",
         utils::TruncateDouble(open_orders_total_map[group_record.currency].commission, 2)},
        {"currency", group_record.currency},
        {"storage",
         utils::TruncateDouble(open_orders_total_map[group_record.currency].storage, 2)}});
    open_orders_table_builder.SetTotalData(open_orders_total_array);

    const JSONObject open_orders_table_props = open_orders_table_builder.CreateTableProps();
    const Node       open_orders_table_node  = Table({}, open_orders_table_props);

    // Pending orders table
    TableBuilder pending_orders_table_builder("AccountPendingOrdersTable");

    pending_orders_table_builder.SetIdColumn("order");
    pending_orders_table_builder.SetOrderBy("order", "DESC");
    pending_orders_table_builder.EnableAutoSave(false);
    pending_orders_table_builder.EnableRefreshButton(false);
    pending_orders_table_builder.EnableBookmarksButton(false);
    pending_orders_table_builder.EnableExportButton(true);
    pending_orders_table_builder.SetLimit(0);
    pending_orders_table_builder.EnableTotal(true);
    pending_orders_table_builder.SetTotalDataTitle("TOTAL");

    pending_orders_table_builder.AddColumn({"order", "ORDER", 1, search_filter});
    pending_orders_table_builder.AddColumn({"symbol", "SYMBOL", 2, search_filter});
    pending_orders_table_builder.AddColumn({"login", "LOGIN", 3, search_filter});
    pending_orders_table_builder.AddColumn({"type", "ORDER_TYPE", 4, search_filter});
    pending_orders_table_builder.AddColumn({"volume", "VOLUME", 5, search_filter});
    pending_orders_table_builder.AddColumn({"open_time", "OPEN_TIME", 6, date_time_filter});
    pending_orders_table_builder.AddColumn(
        {"activation_price", "ACTIVATION_PRICE", 7, search_filter});
    pending_orders_table_builder.AddColumn({"market_price", "MARKET_PRICE", 8, search_filter});
    pending_orders_table_builder.AddColumn({"sl", "S / L", 9, search_filter});
    pending_orders_table_builder.AddColumn({"tp", "T / P", 10, search_filter});
    pending_orders_table_builder.AddColumn({"expiration", "EXPIRATION", 11, date_time_filter});
    pending_orders_table_builder.AddColumn({"currency", "CURRENCY", 12, search_filter});
    pending_orders_table_builder.AddColumn({"comment", "COMMENT", 13, search_filter});

    for (const auto& pending_trade : pending_trades_vector) {
        double             multiplier = 1; // For USD
        ReportSymbolRecord symbol_record{};

        // Conversion disabled
        // if (group_record.currency != "USD") {
        //     try {
        //         server->CalculateConvertRateByCurrency(
        //             group_record.currency, "USD", static_cast<int>(pending_trade.cmd),
        //             &multiplier);
        //     } catch (const std::exception& e) {
        //         std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
        //     }
        // }

        try {
            server->GetSymbol(pending_trade.symbol, &symbol_record);
        } catch (const std::exception& e) {
            std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
        }

        pending_orders_total_map[group_record.currency].volume += pending_trade.volume;

        double market_price =
            utils::GetMarketPriceByCmd(static_cast<int>(pending_trade.cmd), symbol_record);
        pending_orders_table_builder.AddRow(
            {utils::TruncateDouble(pending_trade.order, 0),
             pending_trade.symbol,
             std::to_string(pending_trade.login),
             utils::ConvertCmdToString(static_cast<int>(pending_trade.cmd)),
             utils::TruncateDouble(pending_trade.volume / 100.0, 2),
             utils::FormatTimestampToString(pending_trade.open_time),
             utils::TruncateDouble(pending_trade.open_price * multiplier, pending_trade.digits),
             utils::TruncateDouble(market_price * multiplier, pending_trade.digits),
             utils::TruncateDouble(pending_trade.sl * multiplier, 5),
             utils::TruncateDouble(pending_trade.tp * multiplier, 5),
             utils::FormatTimestampToString(pending_trade.expiration),
             group_record.currency,
             pending_trade.comment});
    }

    JSONArray pending_orders_total_array;
    pending_orders_total_array.emplace_back(JSONObject{
        {"volume",
         utils::TruncateDouble(pending_orders_total_map[group_record.currency].volume / 100.0, 2)},
        {"currency", group_record.currency}});
    pending_orders_table_builder.SetTotalData(pending_orders_total_array);

    const JSONObject pending_trades_table_props = pending_orders_table_builder.CreateTableProps();
    const Node       pending_trades_table_node  = Table({}, pending_trades_table_props);

    // Close orders table
    TableBuilder closed_orders_table_builder("AccountClosedOrdersTable");

    closed_orders_table_builder.SetIdColumn("order");
    closed_orders_table_builder.SetOrderBy("order", "DESC");
    closed_orders_table_builder.EnableAutoSave(false);
    closed_orders_table_builder.EnableRefreshButton(false);
    closed_orders_table_builder.EnableBookmarksButton(false);
    closed_orders_table_builder.EnableExportButton(true);
    closed_orders_table_builder.SetLimit(0);
    closed_orders_table_builder.EnableTotal(true);
    closed_orders_table_builder.SetTotalDataTitle("TOTAL");

    closed_orders_table_builder.AddColumn({"order", "ORDER", 1, search_filter});
    closed_orders_table_builder.AddColumn({"symbol", "SYMBOL", 2, search_filter});
    closed_orders_table_builder.AddColumn({"login", "LOGIN", 3, search_filter});
    closed_orders_table_builder.AddColumn({"type", "ORDER_TYPE", 4, search_filter});
    closed_orders_table_builder.AddColumn({"volume", "VOLUME", 5, search_filter});
    closed_orders_table_builder.AddColumn({"open_time", "OPEN_TIME", 6, date_time_filter});
    closed_orders_table_builder.AddColumn({"open_price", "OPEN_PRICE", 7, search_filter});
    closed_orders_table_builder.AddColumn({"close_time", "CLOSE_TIME", 8, date_time_filter});
    closed_orders_table_builder.AddColumn({"close_price", "CLOSE_PRICE", 9, search_filter});
    closed_orders_table_builder.AddColumn({"profit", "GROSS_PROFIT", 10, search_filter});
    closed_orders_table_builder.AddColumn({"sl", "S / L", 11, search_filter});
    closed_orders_table_builder.AddColumn({"tp", "T / P", 12, search_filter});
    closed_orders_table_builder.AddColumn({"commission", "COMMISSION", 13, search_filter});
    closed_orders_table_builder.AddColumn({"storage", "SWAP", 14, search_filter});
    closed_orders_table_builder.AddColumn({"currency", "CURRENCY", 15, search_filter});
    closed_orders_table_builder.AddColumn({"comment", "COMMENT", 16, search_filter});

    for (const auto& closed_trade : closed_trades_vector) {
        double multiplier = 1;

        // Conversion disabled
        // if (group_record.currency != "USD") {
        //     try {
        //         server->CalculateConvertRateByCurrency(
        //             group_record.currency, "USD", static_cast<int>(closed_trade.cmd),
        //             &multiplier);
        //     } catch (const std::exception& e) {
        //         std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
        //     }
        // }

        closed_orders_total_map[group_record.currency].volume += closed_trade.volume;
        closed_orders_total_map[group_record.currency].profit += closed_trade.profit * multiplier;
        closed_orders_total_map[group_record.currency].commission +=
            closed_trade.commission * multiplier;
        closed_orders_total_map[group_record.currency].storage += closed_trade.storage * multiplier;

        closed_orders_table_builder.AddRow(
            {utils::TruncateDouble(closed_trade.order, 0),
             closed_trade.symbol,
             std::to_string(closed_trade.login),
             utils::ConvertCmdToString(static_cast<int>(closed_trade.cmd)),
             utils::TruncateDouble(closed_trade.volume / 100.0, 2),
             utils::FormatTimestampToString(closed_trade.open_time),
             utils::TruncateDouble(closed_trade.open_price * multiplier, closed_trade.digits),
             utils::FormatTimestampToString(closed_trade.close_time),
             utils::TruncateDouble(closed_trade.close_price * multiplier, closed_trade.digits),
             utils::TruncateDouble(closed_trade.profit * multiplier, 2),
             utils::TruncateDouble(closed_trade.sl, 5),
             utils::TruncateDouble(closed_trade.tp, 5),
             utils::TruncateDouble(closed_trade.commission * multiplier, 2),
             utils::TruncateDouble(closed_trade.storage * multiplier, 2),
             group_record.currency,
             closed_trade.comment});
    }

    JSONArray closed_orders_total_array;
    closed_orders_total_array.emplace_back(JSONObject{
        {"volume",
         utils::TruncateDouble(closed_orders_total_map[group_record.currency].volume / 100.0, 2)},
        {"profit", utils::TruncateDouble(closed_orders_total_map[group_record.currency].profit, 2)},
        {"commission",
         utils::TruncateDouble(closed_orders_total_map[group_record.currency].commission, 2)},
        {"currency", group_record.currency},
        {"storage",
         utils::TruncateDouble(closed_orders_total_map[group_record.currency].storage, 2)}});
    closed_orders_table_builder.SetTotalData(closed_orders_total_array);

    const JSONObject closed_orders_table_props = closed_orders_table_builder.CreateTableProps();
    const Node       closed_orders_table_node  = Table({}, closed_orders_table_props);

    // Transactions table
    TableBuilder transactions_table_builder("AccountTransactionsTable");

    transactions_table_builder.SetIdColumn("order");
    transactions_table_builder.SetOrderBy("order", "DESC");
    transactions_table_builder.EnableRefreshButton(false);
    transactions_table_builder.EnableBookmarksButton(false);
    transactions_table_builder.EnableExportButton(true);
    transactions_table_builder.SetLimit(0);
    transactions_table_builder.EnableTotal(true);
    transactions_table_builder.SetTotalDataTitle("TOTAL");

    transactions_table_builder.AddColumn({"transaction", "TRANSACTION", 1, search_filter});
    transactions_table_builder.AddColumn({"login", "LOGIN", 2, search_filter});
    transactions_table_builder.AddColumn({"name", "NAME", 3, search_filter});
    transactions_table_builder.AddColumn({"type", "TRANSACTION_TYPE", 4, search_filter});
    transactions_table_builder.AddColumn({"profit", "AMOUNT", 5, search_filter});
    transactions_table_builder.AddColumn({"open_time", "CREATION_TIME", 6, date_time_filter});
    transactions_table_builder.AddColumn({"currency", "CURRENCY", 7, search_filter});
    transactions_table_builder.AddColumn({"comment", "COMMENT", 8, search_filter});

    for (const auto& trx : transactions_vector) {
        double multiplier = 1; // For USD

        // Conversion disabled
        // if (group_record.currency != "USD") {
        //     try {
        //         server->CalculateConvertRateByCurrency(
        //             group_record.currency, "USD", static_cast<int>(trx.cmd), &multiplier);
        //     } catch (const std::exception& e) {
        //         std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
        //     }
        // }

        transactions_total_map[group_record.currency].profit += trx.profit * multiplier;

        transactions_table_builder.AddRow({utils::TruncateDouble(trx.order, 0),
                                           std::to_string(account_record.login),
                                           account_record.name,
                                           utils::ConvertCmdToString(static_cast<int>(trx.cmd)),
                                           utils::TruncateDouble(trx.profit * multiplier, 2),
                                           utils::FormatTimestampToString(trx.open_time),
                                           group_record.currency,
                                           trx.comment});
    }

    JSONArray transactions_total_array;
    transactions_total_array.emplace_back(JSONObject{
        {"profit", utils::TruncateDouble(transactions_total_map[group_record.currency].profit, 2)},
        {"currency", group_record.currency}});
    transactions_table_builder.SetTotalData(transactions_total_array);

    const JSONObject transactions_table_props = transactions_table_builder.CreateTableProps();
    const Node       transactions_table_node  = Table({}, transactions_table_props);

    // Total report
    const Node report = Column({h1({text("Account Summary Report")}),
                                main_info,
                                h2({text("Open Orders")}),
                                open_orders_table_node,
                                h2({text("Pending Orders")}),
                                pending_trades_table_node,
                                h2({text("Closed Orders")}),
                                closed_orders_table_node,
                                h2({text("Finance History")}),
                                transactions_table_node});
    utils::CreateUI(report, response, allocator);
}