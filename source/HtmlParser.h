//
// Created by dandy on 02/10/2022.
//

#ifndef BANK_APP_HTMLPARSER_H
#define BANK_APP_HTMLPARSER_H

#include <string>
#include <lexbor/html/html.h>
#include <lexbor/html/node.h>
#include <lexbor/dom/dom.h>
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>
#include <vector>
#include <memory>
#include <optional>
#include "exceptions/lexbor_exception.h"

typedef std::basic_string<lxb_char_t> lxb_string;
constexpr size_t LXB_CHARSIZE = sizeof(lxb_char_t);

namespace bank_app{
    std::string lexborStatusString(lxb_status_t status){
        std::string status_message;
        switch (status) {
            case LXB_STATUS_OK:
                status_message = "OK";
                break;
            case LXB_STATUS_ERROR:
                status_message = "html parser error";
                break;
            case LXB_STATUS_ERROR_MEMORY_ALLOCATION:
                status_message = "memory allocation error";
                break;
            case LXB_STATUS_ERROR_OBJECT_IS_NULL:
                status_message = "object is null error";
                break;
            case LXB_STATUS_ERROR_SMALL_BUFFER:
                status_message = "small buffer error";
                break;
            case LXB_STATUS_ERROR_INCOMPLETE_OBJECT:
                status_message = "incomplete object error";
                break;
            case LXB_STATUS_ERROR_NO_FREE_SLOT:
                status_message = "no free slot error";
                break;
            case LXB_STATUS_ERROR_TOO_SMALL_SIZE:
                status_message = "too small size error";
                break;
            case LXB_STATUS_ERROR_NOT_EXISTS:
                status_message = "not exists error";
                break;
            case LXB_STATUS_ERROR_WRONG_ARGS:
                status_message = "wrong args error";
                break;
            case LXB_STATUS_ERROR_WRONG_STAGE:
                status_message = "wrong stage error";
                break;
            case LXB_STATUS_ERROR_UNEXPECTED_RESULT:
                status_message = "unexpected result error";
                break;
            case LXB_STATUS_ERROR_UNEXPECTED_DATA:
                status_message = "unexpected data error";
                break;
            case LXB_STATUS_ERROR_OVERFLOW:
                status_message = "overflow error";
                break;
            case LXB_STATUS_CONTINUE:
                status_message = "CONTINUE";
                break;
            case LXB_STATUS_SMALL_BUFFER:
                status_message = "SMALL BUFFER";
                break;
            case LXB_STATUS_ABORTED:
                status_message = "ABBORTED";
                break;
            case LXB_STATUS_STOPPED:
                status_message = "STOPPED";
                break;
            case LXB_STATUS_NEXT:
                status_message = "NEXT";
                break;
            case LXB_STATUS_STOP:
                status_message = "STOP";
                break;
        }

        return status_message;
    }

    lxb_string lxbFromString(std::string text){
        return lxb_string(reinterpret_cast<lxb_char_t*>(text.data()), text.size());
    }

    std::string lxbToString(lxb_string text){
        return std::string(reinterpret_cast<char*>(text.data()), text.size());
    }

    std::string lxbCharToString(lxb_char_t* text, size_t len){
        return std::string(reinterpret_cast<char*>(text), len);
    }

    std::string lxbGetNodeAttr(lxb_dom_node_t* data, lxb_string attrName){
        auto element = lxb_dom_interface_element(data);
        auto attr = lxb_dom_element_attr_by_name(element, attrName.data(), attrName.size());

        return lxbToString(lxb_string(attr->value->data, attr->value->length));
    }

    std::string lxbGetInnerHtml(lxb_dom_node_t* node){
        auto temp = std::make_unique<lexbor_str_t>();
        lxb_html_serialize_deep_str(node, temp.get());

        return lxbToString(lxb_string(temp->data, temp->length));
    }

    class HtmlParser{
        lxb_string html_src;
        lxb_dom_node_t *body;
        lxb_selectors_t *selectors;
        lxb_css_selectors_t *css_selectors;
        lxb_html_document_t *document;
        lxb_css_parser_t *parser;
        std::vector<lxb_dom_node_t*> results;
    public:
        HtmlParser(lxb_string html_src) : html_src(html_src){
            document = lxb_html_document_create();
            errorCheck(lxb_html_document_parse(document, html_src.c_str(), (html_src.size() / LXB_CHARSIZE) - 1),
                       "HtmlParser:lxb_html_document_parse");
            body = lxb_dom_interface_node(lxb_html_document_body_element(document));

            parser = lxb_css_parser_create();
            errorCheck(lxb_css_parser_init(parser, NULL, NULL), "HtmlParser:lxb_css_parser_init");

            css_selectors = lxb_css_selectors_create();
            errorCheck(lxb_css_selectors_init(css_selectors, 128), "HtmlParser:lxb_css_selectors_init");

            selectors = lxb_selectors_create();
            errorCheck(lxb_selectors_init(selectors), "HtmlParser:lxb_selectors_init");
        }

        void errorCheck(lxb_status_t status, std::string source){
            auto message = lexborStatusString(status);
            if(status != LXB_STATUS_OK){
                auto message = lexborStatusString(status);
                throw lexbor_exception(message, source, status);
            }
        }

        HtmlParser* css(lxb_string needle, const std::optional<lxb_dom_node_t*>& target = std::nullopt){
            auto needleList = lxb_css_selectors_parse(parser, needle.c_str(),
                                                      needle.size()  / LXB_CHARSIZE);

            if(needleList != NULL){
                auto qualifiedTarget = target ? target.value() : body;

                errorCheck(lxb_selectors_find(selectors, qualifiedTarget, needleList,
                                              [](lxb_dom_node_t *node, lxb_css_selector_specificity_t *spec,
                                                 void *ctx) -> lxb_status_t {
                                                  auto results = reinterpret_cast<std::vector<lxb_dom_node_t*>*>(ctx);
                                                  results->push_back(node);
                                                  return LXB_STATUS_OK;
                                              }, &results), "HtmlParser:css:lxb_selectors_find");
            }

            return this;
        }

        auto toArrayString(){
            auto list = std::make_shared<std::vector<lxb_string>>();

            if(!results.empty()) {
                for (auto result: results) {
                    auto temp = std::make_unique<lexbor_str_t>();
                    lxb_html_serialize_deep_str(result, temp.get());
                    list->push_back(lxb_string(temp->data, temp->length));
                }
            }

            return list;
        }

        auto toArrayStdString(){
            auto list = std::make_shared<std::vector<std::string>>();

            if(!results.empty()) {
                for (auto result: results) {
                    auto temp = std::make_unique<lexbor_str_t>();
                    lxb_html_serialize_deep_str(result, temp.get());
                    list->push_back(std::string(reinterpret_cast<char*>(temp->data), temp->length));
                }
            }

            return list;
        }

        std::vector<lxb_dom_node_t*>& toArray(){
            if(results.empty()){
                throw lexbor_exception("empty result list", "HtmlParser:toArray", LXB_STATUS_ERROR);
            }

            return results;
        }

        ~HtmlParser(){
            /* Destroy Selectors object. */
            (void) lxb_selectors_destroy(selectors, true);

            /* Destroy resources for CSS Parser. */
            (void) lxb_css_parser_destroy(parser, true);

            /* Destroy CSS Selectors List memory. */
            (void) lxb_css_selectors_destroy(css_selectors, true, true);

            /* Destroy HTML Document. */
            (void) lxb_html_document_destroy(document);
        }
    };
}

#endif //BANK_APP_HTMLPARSER_H
