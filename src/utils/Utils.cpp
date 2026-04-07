#include "Utils.h"

namespace utils {
    void CreateUI(const ast::Node&                    node,
                  rapidjson::Value&                   response,
                  rapidjson::Document::AllocatorType& allocator) {
        // Content
        Value node_object(kObjectType);
        to_json(node, node_object, allocator);

        Value content_array(kArrayType);
        content_array.PushBack(node_object, allocator);

        // Header
        Value header_array(kArrayType);

        {
            Value space_object(kObjectType);
            space_object.AddMember("type", "Space", allocator);

            Value children(kArrayType);

            Value text_object(kObjectType);
            text_object.AddMember("type", "#text", allocator);

            Value props(kObjectType);
            props.AddMember("value", "Account Equity report", allocator);

            text_object.AddMember("props", props, allocator);
            children.PushBack(text_object, allocator);

            space_object.AddMember("children", children, allocator);

            header_array.PushBack(space_object, allocator);
        }

        // Footer
        Value footer_array(kArrayType);

        {
            Value space_object(kObjectType);
            space_object.AddMember("type", "Space", allocator);

            Value props_space(kObjectType);
            props_space.AddMember("justifyContent", "space-between", allocator);
            space_object.AddMember("props", props_space, allocator);

            Value children(kArrayType);

            // Export PDF Button
            Value export_btn_object(kObjectType);
            export_btn_object.AddMember("type", "Button", allocator);

            Value export_btn_props_object(kObjectType);
            export_btn_props_object.AddMember("className", "form_action_button", allocator);
            export_btn_props_object.AddMember("buttonType", "primary", allocator);
            export_btn_props_object.AddMember("loading", "{{isExportLoading}}", allocator);
            export_btn_props_object.AddMember(
                "onClick",
                "({self}) => {\n self?.exportPdf('account-summary-report');\n} ",
                allocator);

            export_btn_object.AddMember("props", export_btn_props_object, allocator);

            Value export_btn_children(kArrayType);

            Value export_text_object(kObjectType);
            export_text_object.AddMember("type", "#text", allocator);

            Value export_text_props_object(kObjectType);
            export_text_props_object.AddMember("value", "Export pdf", allocator);

            export_text_object.AddMember("props", export_text_props_object, allocator);
            export_btn_children.PushBack(export_text_object, allocator);

            export_btn_object.AddMember("children", export_btn_children, allocator);

            children.PushBack(export_btn_object, allocator);

            // Close Button
            Value btn_object(kObjectType);
            btn_object.AddMember("type", "Button", allocator);

            Value btn_props_object(kObjectType);
            btn_props_object.AddMember("className", "form_action_button", allocator);
            btn_props_object.AddMember("borderType", "danger", allocator);
            btn_props_object.AddMember("buttonType", "outlined", allocator);

            btn_props_object.AddMember("onClick", "{\"action\":\"CloseModal\"}", allocator);

            btn_object.AddMember("props", btn_props_object, allocator);

            Value btn_children(kArrayType);

            Value text_object(kObjectType);
            text_object.AddMember("type", "#text", allocator);

            Value text_props_object(kObjectType);
            text_props_object.AddMember("value", "Close", allocator);

            text_object.AddMember("props", text_props_object, allocator);
            btn_children.PushBack(text_object, allocator);

            btn_object.AddMember("children", btn_children, allocator);

            children.PushBack(btn_object, allocator);

            space_object.AddMember("children", children, allocator);

            footer_array.PushBack(space_object, allocator);
        }

        // Modal
        Value model_object(kObjectType);
        model_object.AddMember("size", "xxxl", allocator);
        model_object.AddMember("headerContent", header_array, allocator);
        model_object.AddMember("footerContent", footer_array, allocator);
        model_object.AddMember("content", content_array, allocator);

        // UI
        Value ui_object(kObjectType);
        ui_object.AddMember("modal", model_object, allocator);

        response.SetObject();
        response.AddMember("ui", ui_object, allocator);
    }

    std::string FormatTimestampToString(const time_t& timestamp, const std::string& format) {
        std::tm tm{};
        localtime_r(&timestamp, &tm);

        std::ostringstream oss;
        oss << std::put_time(&tm, format.c_str());
        return oss.str();
    }

    double TruncateDouble(const double& value, const int& digits) {
        const double factor = std::pow(10.0, digits);
        return std::trunc(value * factor) / factor;
    }

    std::string ConvertCmdToString(int cmd) {
        switch (cmd) {
            case -1:
                return "Nothing";
            case 0:
                return "Buy";
            case 1:
                return "Sell";
            case 2:
                return "Buy Limit";
            case 3:
                return "Sell Limit";
            case 4:
                return "Buy Stop";
            case 5:
                return "Sell Stop";
            case 6:
                return "Deposit";
            case 7:
                return "Credit In";
            case 8:
                return "Withdrawal";
            case 9:
                return "Credit Out";
            case 10:
                return "Buy Stop Limit";
            case 11:
                return "Sell Stop Limit";
            default:
                return "Unknown";
        }
    }

    double GetMarketPriceByCmd(int cmd, const SymbolRecord& symbol_record) {
        switch (cmd) {
            case 0: // OP_BUY
            case 2: // OP_BUYLIMIT
            case 4: // OP_BUYSTOP
                return symbol_record.ask;

            case 1: // OP_SELL
            case 3: // OP_SELLLIMIT
            case 5: // OP_SELLSTOP
                return symbol_record.bid;

            default:
                return symbol_record.bid;
        }
    }

} // namespace utils
