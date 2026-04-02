#include "PluginInterface.h"

using namespace ast;

extern "C" void AboutReport(rapidjson::Value&                   request,
                            rapidjson::Value&                   response,
                            rapidjson::Document::AllocatorType& allocator,
                            CServerInterface*                   server) {
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
    response.AddMember("type", REPORT_RANGE_ACCOUNT_TYPE, allocator);
}

extern "C" void DestroyReport() {}

extern "C" void CreateReport(rapidjson::Value&                   request,
                             rapidjson::Value&                   response,
                             rapidjson::Document::AllocatorType& allocator,
                             CServerInterface*                   server) {
    int login = 0;
    int from  = 0;
    int to    = 0;

    if (request.HasMember("login") && request["login"].IsInt())
        login = request["login"].GetInt();

    if (request.HasMember("from") && request["from"].IsInt())
        from = request["from"].GetInt();

    if (request.HasMember("to") && request["to"].IsInt())
        to = request["to"].GetInt();

    AccountRecord            account_record{};
    GroupRecord              group_record{};
    MarginLevel              margin_level{};
    std::vector<TradeRecord> closed_trades_vector;
    std::vector<TradeRecord> open_trades_vector;
    std::vector<TradeRecord> pending_trades_vector;
    std::vector<TradeRecord> transactions_vector;

    try {
        server->GetAccountByLogin(login, &account_record);
        server->GetGroup(account_record.group, &group_record);
        server->GetAccountBalanceByLogin(login, &margin_level);
        server->GetCloseTradesByLogin(login, &closed_trades_vector);
        server->GetOpenTradesByLogin(login, &open_trades_vector);
        server->GetPendingTradesByLogin(login, &pending_trades_vector);
        server->GetTransactionsByLogin(login, from, to, &transactions_vector);

        // Фильтрация для closed_trades_vector
        std::erase_if(closed_trades_vector, [from, to](const TradeRecord& closed_trade) {
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
        {p({span({text("Balance: ")}),
            span({text(std::to_string(utils::TruncateDouble(margin_level.balance, 2)))})}),
         p({span({text("Equity: ")}),
            span({text(std::to_string(utils::TruncateDouble(margin_level.equity, 2)))})}),
         p({span({text("Margin: ")}),
            span({text(std::to_string(utils::TruncateDouble(margin_level.margin, 2)))})}),
         p({span({text("Free Margin: ")}),
            span({text(std::to_string(utils::TruncateDouble(margin_level.margin_free, 2)))})}),
         p({span({text("Margin Level: ")}),
            span({text(std::to_string(utils::TruncateDouble(margin_level.margin_level, 2)))})})},
        {{"style", "font-size: 14px"}});

    auto main_info = div(
        {left_column, right_column},
        {{"style", "display:flex; gap:40px; align-items:flex-start; justify-content: center;"}});

    // Common table filters
    FilterConfig search_filter;
    search_filter.type = FilterType::Search;
    FilterConfig date_time_filter;
    date_time_filter.type = FilterType::DateTime;

    // Close orders table
    TableBuilder closed_orders_table_builder("AccountClosedOrdersTable");
    closed_orders_table_builder.SetIdColumn("order");
    closed_orders_table_builder.SetOrderBy("order", "DESC");
    closed_orders_table_builder.EnableAutoSave(false);
    closed_orders_table_builder.EnableRefreshButton(false);
    closed_orders_table_builder.EnableBookmarksButton(false);
    closed_orders_table_builder.EnableExportButton(true);

    closed_orders_table_builder.AddColumn({"order", "ORDER", 1, search_filter});
    closed_orders_table_builder.AddColumn({"open_time", "OPEN_TIME", 4, date_time_filter});
    closed_orders_table_builder.AddColumn({"close_time", "CLOSE_TIME", 5, date_time_filter});
    closed_orders_table_builder.AddColumn({"type", "TYPE", 6, search_filter});
    closed_orders_table_builder.AddColumn({"symbol", "SYMBOL", 7, search_filter});
    closed_orders_table_builder.AddColumn({"volume", "VOLUME", 8, search_filter});
    closed_orders_table_builder.AddColumn({"open_price", "OPEN_PRICE", 9, search_filter});
    closed_orders_table_builder.AddColumn({"close_price", "CLOSE_PRICE", 10, search_filter});
    closed_orders_table_builder.AddColumn({"sl", "S / L", 11, search_filter});
    closed_orders_table_builder.AddColumn({"tp", "T / P", 12, search_filter});
    closed_orders_table_builder.AddColumn({"commission", "COMMISSION", 13, search_filter});
    closed_orders_table_builder.AddColumn({"storage", "SWAP", 14, search_filter});
    closed_orders_table_builder.AddColumn({"profit", "AMOUNT", 15, search_filter});
    closed_orders_table_builder.AddColumn({"currency", "CURRENCY", 16, search_filter});
    closed_orders_table_builder.AddColumn({"comment", "COMMENT", 17, search_filter});

    for (const auto& closed_trade : closed_trades_vector) {
        double multiplier = 1;

        if (group_record.currency != "USD") {
            try {
                server->CalculateConvertRateByCurrency(
                    group_record.currency, "USD", closed_trade.cmd, &multiplier);
            } catch (const std::exception& e) {
                std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
            }
        }

        closed_orders_table_builder.AddRow(
            {utils::TruncateDouble(closed_trade.order, 0),
             utils::FormatTimestampToString(closed_trade.open_time),
             utils::FormatTimestampToString(closed_trade.close_time),
             utils::ConvertCmdToString(closed_trade.cmd),
             closed_trade.symbol,
             utils::TruncateDouble(closed_trade.volume / 100.0, 2),
             utils::TruncateDouble(closed_trade.open_price * multiplier, 2),
             utils::TruncateDouble(closed_trade.close_price * multiplier, 2),
             utils::TruncateDouble(closed_trade.sl, 2),
             utils::TruncateDouble(closed_trade.tp, 2),
             utils::TruncateDouble(closed_trade.commission * multiplier, 2),
             utils::TruncateDouble(closed_trade.storage * multiplier, 2),
             utils::TruncateDouble(closed_trade.profit * multiplier, 2),
             "USD",
             closed_trade.comment});
    }

    const JSONObject closed_orders_table_props = closed_orders_table_builder.CreateTableProps();
    const Node       closed_orders_table_node  = Table({}, closed_orders_table_props);

    // Open orders table
    TableBuilder open_orders_table_builder("AccountOpenOrdersTable");
    open_orders_table_builder.SetIdColumn("order");
    open_orders_table_builder.SetOrderBy("order", "DESC");
    open_orders_table_builder.EnableAutoSave(false);
    open_orders_table_builder.EnableRefreshButton(false);
    open_orders_table_builder.EnableBookmarksButton(false);
    open_orders_table_builder.EnableExportButton(true);

    open_orders_table_builder.AddColumn({"order", "ORDER", 1, search_filter});
    open_orders_table_builder.AddColumn({"open_time", "OPEN_TIME", 4, date_time_filter});
    open_orders_table_builder.AddColumn({"type", "TYPE", 5});
    open_orders_table_builder.AddColumn({"symbol", "SYMBOL", 6, search_filter});
    open_orders_table_builder.AddColumn({"volume", "VOLUME", 7, search_filter});
    open_orders_table_builder.AddColumn({"open_price", "OPEN_PRICE", 8, search_filter});
    open_orders_table_builder.AddColumn({"sl", "S / L", 9, search_filter});
    open_orders_table_builder.AddColumn({"tp", "T / P", 10, search_filter});
    open_orders_table_builder.AddColumn({"storage", "SWAP", 11, search_filter});
    open_orders_table_builder.AddColumn({"profit", "AMOUNT", 12, search_filter});
    open_orders_table_builder.AddColumn({"comment", "COMMENT", 13, search_filter});
    open_orders_table_builder.AddColumn({"currency", "CURRENCY", 14, search_filter});

    for (const auto& open_trade : open_trades_vector) {
        double multiplier = 1; // for USD

        if (group_record.currency != "USD") {
            try {
                server->CalculateConvertRateByCurrency(
                    group_record.currency, "USD", open_trade.cmd, &multiplier);
            } catch (const std::exception& e) {
                std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
            }
        }

        open_orders_table_builder.AddRow(
            {utils::TruncateDouble(open_trade.order, 0),
             utils::FormatTimestampToString(open_trade.open_time),
             utils::ConvertCmdToString(open_trade.cmd),
             open_trade.symbol,
             utils::TruncateDouble(open_trade.volume / 100.0, 2),
             utils::TruncateDouble(open_trade.open_price * multiplier, 2),
             utils::TruncateDouble(open_trade.sl * multiplier, 2),
             utils::TruncateDouble(open_trade.tp * multiplier, 2),
             utils::TruncateDouble(open_trade.storage * multiplier, 2),
             utils::TruncateDouble(open_trade.profit * multiplier, 2),
             open_trade.comment,
             "USD"});
    }

    const JSONObject open_orders_table_props = open_orders_table_builder.CreateTableProps();
    const Node       open_orders_table_node  = Table({}, open_orders_table_props);

    // Pending orders table
    TableBuilder pending_trades_table_builder("AccountPendingOrdersTable");

    pending_trades_table_builder.SetIdColumn("order");
    pending_trades_table_builder.SetOrderBy("order", "DESC");
    pending_trades_table_builder.EnableAutoSave(false);
    pending_trades_table_builder.EnableRefreshButton(false);
    pending_trades_table_builder.EnableBookmarksButton(false);
    pending_trades_table_builder.EnableExportButton(true);

    pending_trades_table_builder.AddColumn({"order", "ORDER", 1, search_filter});
    pending_trades_table_builder.AddColumn({"open_time", "OPEN_TIME", 4, date_time_filter});
    pending_trades_table_builder.AddColumn({"type", "TYPE", 5, search_filter});
    pending_trades_table_builder.AddColumn({"symbol", "SYMBOL", 6, search_filter});
    pending_trades_table_builder.AddColumn({"volume", "VOLUME", 7, search_filter});
    pending_trades_table_builder.AddColumn({"open_price", "OPEN_PRICE", 8, search_filter});
    pending_trades_table_builder.AddColumn({"sl", "S / L", 9, search_filter});
    pending_trades_table_builder.AddColumn({"tp", "T / P", 10, search_filter});
    pending_trades_table_builder.AddColumn({"storage", "SWAP", 11, search_filter});
    pending_trades_table_builder.AddColumn({"profit", "AMOUNT", 12, search_filter});
    pending_trades_table_builder.AddColumn({"comment", "COMMENT", 13, search_filter});
    pending_trades_table_builder.AddColumn({"currency", "CURRENCY", 14, search_filter});

    for (const auto& pending_trade : pending_trades_vector) {
        double multiplier = 1; // For USD

        if (group_record.currency != "USD") {
            try {
                server->CalculateConvertRateByCurrency(
                    group_record.currency, "USD", pending_trade.cmd, &multiplier);
            } catch (const std::exception& e) {
                std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
            }
        }

        pending_trades_table_builder.AddRow(
            {utils::TruncateDouble(pending_trade.order, 0),
             utils::FormatTimestampToString(pending_trade.open_time),
             utils::ConvertCmdToString(pending_trade.cmd),
             pending_trade.symbol,
             utils::TruncateDouble(pending_trade.volume / 100.0, 2),
             utils::TruncateDouble(pending_trade.open_price * multiplier, 2),
             utils::TruncateDouble(pending_trade.sl * multiplier, 2),
             utils::TruncateDouble(pending_trade.tp * multiplier, 2),
             utils::TruncateDouble(pending_trade.storage * multiplier, 2),
             utils::TruncateDouble(pending_trade.profit * multiplier, 2),
             pending_trade.comment,
             "USD"});
    }

    const JSONObject pending_trades_table_props = pending_trades_table_builder.CreateTableProps();
    const Node       pending_trades_table_node  = Table({}, pending_trades_table_props);

    // Transactions table
    TableBuilder transactions_table_builder("CreditFacilityReport");

    transactions_table_builder.SetIdColumn("order");
    transactions_table_builder.SetOrderBy("order", "DESC");
    transactions_table_builder.EnableRefreshButton(false);
    transactions_table_builder.EnableBookmarksButton(false);
    transactions_table_builder.EnableExportButton(true);

    transactions_table_builder.AddColumn({"order", "ORDER", 1, search_filter});
    transactions_table_builder.AddColumn({"order", "TYPE", 2, search_filter});
    transactions_table_builder.AddColumn({"open_time", "OPEN_TIME", 3, date_time_filter});
    transactions_table_builder.AddColumn({"comment", "COMMENT", 4, search_filter});
    transactions_table_builder.AddColumn({"profit", "AMOUNT", 5, search_filter});
    transactions_table_builder.AddColumn({"currency", "CURRENCY", 6, search_filter});

    for (const auto& trx : transactions_vector) {
        double multiplier = 1; // For USD

        if (group_record.currency != "USD") {
            try {
                server->CalculateConvertRateByCurrency(
                    group_record.currency, "USD", trx.cmd, &multiplier);
            } catch (const std::exception& e) {
                std::cerr << "[AccountSummaryReportInterface]: " << e.what() << std::endl;
            }
        }

        transactions_table_builder.AddRow({utils::TruncateDouble(trx.order, 0),
                                           utils::ConvertCmdToString(trx.cmd),
                                           utils::FormatTimestampToString(trx.open_time),
                                           trx.comment,
                                           utils::TruncateDouble(trx.profit * multiplier, 2),
                                           "USD"});
    }

    const JSONObject transactions_table_props = transactions_table_builder.CreateTableProps();
    const Node       transactions_table_node  = Table({}, transactions_table_props);

    // Total report
    const Node report = Column({h1({text("Account Summary Report")}),
                                main_info,
                                h2({text("Closed Orders")}),
                                closed_orders_table_node,
                                h2({text("Open Orders")}),
                                open_orders_table_node,
                                h2({text("Pending Orders")}),
                                pending_trades_table_node,
                                h2({text("Finance History")}),
                                transactions_table_node});
    utils::CreateUI(report, response, allocator);
}