#pragma once
#include <string>
#include <utility>
#include <functional>
#include <vector>
#include <regex>
#include <sstream>
#include <stack>
#include <wx/wx.h>
#include <wx/wrapsizer.h>
#include <wx/fs_inet.h>
#include <wx/mstream.h>
#include <wx/image.h> 
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include <curl/curl.h>

#include <iostream>

namespace AdaptiveCards
{
    class CurlInit {
    public:
        CurlInit() {
            curl_global_init(CURL_GLOBAL_ALL);
        }
        ~CurlInit() {
            curl_global_cleanup();
        }
    };

    class url_stream {
        std::stringstream ss_;
        static CurlInit curl_init_;

        static size_t write_data(void *ptr, size_t size, size_t nmemb, url_stream *pthis)
        {
            auto const total{size * nmemb};
            pthis->ss_.write(static_cast<const char *>(ptr), total);
            return total;
        }
    public:
        url_stream(std::string const &url) {
            auto curl_handle = curl_easy_init();
            curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &url_stream::write_data);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, this);
            curl_easy_perform(curl_handle);
            curl_easy_cleanup(curl_handle);
        }
        wxMemoryInputStream input_stream() {
            ss_.flush();
            return wxMemoryInputStream(ss_.str().data(), ss_.str().size());
        }
    };

    class Frame : public wxFrame
    {
    public:
        Frame(const wxString &title, const wxPoint &pos, const wxSize &size)
            : wxFrame(NULL, wxID_ANY, title, pos, size)
        {
            CreateStatusBar();
            SetStatusText("Welcome to AdaptiveCards-wxWidgets!");
        }

    private:
        void OnExit(wxCommandEvent &event)
        {
            Close(true);
        }
        wxDECLARE_EVENT_TABLE();
    };

    template <typename TCardProvider, const char * const initial_card>
    class App : public wxApp
    {
        TCardProvider cardprovider_;
        std::string current_card_;
    public:
        bool OnInit() override
        {
            wxInitAllImageHandlers();

            auto frame = new Frame("Hello World", wxPoint(50, 50), wxSize(450, 340));
            frame->Show(true);
            ShowCard(initial_card, "{}", frame);
            return true;
        }

        using TSinks = std::vector<std::pair<std::string,std::function<void(std::string)>>>;
        using TSetter = std::function<void(std::string const &)>;
        using TExpressionSet = std::function<void(TSetter, std::string const &value)>;
        using TResize = std::function<void(int)>;
        using TAddWidget = std::function<void(wxWindow *)>;
        using TWidgetFactory = std::function<TResize(rapidjson::Value &, wxWindow *parent, TExpressionSet, TAddWidget)>;

        auto CreateCardTemplate(std::string const &src, Frame *frame) {
            static const std::map<std::string,TWidgetFactory> widget_factories {
                {"TextBlock", [](rapidjson::Value &element, wxWindow *frame, TExpressionSet expr, TAddWidget add) {
                    auto const text {element.HasMember("text") ? std::string(element["text"].GetString()) : std::string()};
                    auto const label {new wxStaticText(frame, -1, text.c_str())};
                    if (element.HasMember("size")) {
                        expr([label](auto size_value) {
                            auto font {label->GetFont()};
                            if (std::string("Medium") == size_value) {
                                font.SetPointSize(font.GetPointSize() * 3 / 2);
                            }
                            label->SetFont(font);
                        }, element["size"].GetString());
                    }
                    if (element.HasMember("weight")) {
                        expr([label](auto weight_value){
                            auto font {label->GetFont()};
                            if (std::string("Bolder") == weight_value) {
                                font.SetWeight(wxFONTWEIGHT_BOLD);
                            }
                            label->SetFont(font);
                        }, element["weight"].GetString());
                    }
                    add(label);
                    expr([label](auto text_value) {
                        label->SetLabelText(text_value);
                    }, text);
                    return [label](int new_size) {
                        std::string t{label->GetLabelText()};
                        for(auto pos {t.find('\n')}; pos != std::string::npos; pos = t.find('\n')) {
                            t.replace(pos, 1, std::string(1, ' '));
                        }
                        label->SetLabelText(t.c_str());
                        label->Wrap(new_size);
                    };
                }},
                {"ColumnSet", [](rapidjson::Value &element, wxWindow *frame, TExpressionSet expr, TAddWidget add) {
                    auto container{new wxPanel{frame}};
                    auto sizer {new wxBoxSizer(wxHORIZONTAL)};
                    TResize resize {[](int ){}};
                    for (auto &col: element["columns"].GetArray()) {
                        auto const pos {widget_factories.find(col["type"].GetString())};
                        if (pos != widget_factories.end()) {
                            auto added_resize {pos->second(col, container, expr, [sizer](wxWindow *control){
                                sizer->Add(control);
                            })};
                            resize = [resize, added_resize](int new_size){
                                resize(new_size);
                                added_resize(new_size);
                            };
                        }
                    }
                    container->SetSizer(sizer);
                    add(container);
                    return resize;
                }},
                {"Column", [](rapidjson::Value &element, wxWindow *frame, TExpressionSet expr, TAddWidget add) {
                    auto container{new wxPanel{frame}};
                    auto sizer {new wxBoxSizer(wxVERTICAL)};
                    TResize resize {[](int ){}};
                    for (auto &col: element["items"].GetArray()) {
                        auto const pos {widget_factories.find(col["type"].GetString())};
                        if (pos != widget_factories.end()) {
                            auto added_resize {pos->second(col, container, expr, [sizer](wxWindow *control){
                                sizer->Add(control);
                            })};
                            resize = [resize, added_resize](int new_size){
                                resize(new_size);
                                added_resize(new_size);
                            };
                        }
                    }
                    container->SetSizer(sizer);
                    add(container);
                    return resize;
                }},
                {"Image", [](rapidjson::Value &element, wxWindow *frame, TExpressionSet expr, TAddWidget add) {
                    auto img_control {new wxStaticBitmap{frame, -1, wxBitmap{1,1}}};
                    expr([img_control](std::string const &value){
                        auto in {url_stream(value)};
                        auto input_stream {in.input_stream()};
                        wxImage image{input_stream};
                        if(image.Ok())
                        {
                            auto const width {img_control->GetSize().GetWidth()};
                            std::cerr << __LINE__ << " width: " << width << std::endl;
                            image.Rescale(width, width * image.GetHeight() / image.GetWidth());
                            wxBitmap bmp{image};
                            img_control->SetBitmap(bmp);
                        }
                    }, element["url"].GetString());
                    auto const size_expr {element.HasMember("size") ? element["size"].GetString() : "Medium"};
                    expr([img_control](std::string const &value) {
                        if (value == "Small") {
                            img_control->SetSize(wxDefaultCoord, wxDefaultCoord, 50, wxDefaultCoord, wxSIZE_AUTO_HEIGHT);
                        }
                        else if (value == "Medium") {
                            img_control->SetSize(wxDefaultCoord, wxDefaultCoord, 250, wxDefaultCoord, wxSIZE_AUTO_HEIGHT);
                        }
                        img_control->SetAutoLayout(false);
                    }, size_expr);
                    add(img_control);
                    return [](int){};
                }}
            };
            TSinks sinks;
            rapidjson::Document doc;
            doc.Parse(src.c_str());
            auto sizer {new wxBoxSizer(wxVERTICAL)};
            std::function<void(int)> on_size = [sizer](int){ sizer->Layout(); };
            for (auto &element: doc["body"].GetArray()) {
                auto const element_type {element["type"].GetString()};
                auto const pos {widget_factories.find(element_type)};
                if (pos != widget_factories.end()) {
                    auto resize = pos->second(element, frame, [&sinks](TSetter setter, std::string const &text) {
                        std::smatch match_result;
                        static std::regex const match_expr {"\\$\\{(.+)\\}"};
                        if (std::regex_match(text, match_result, match_expr)) {
                            sinks.push_back(std::make_pair(match_result[1], setter));
                        }
                        else {
                            setter(text);
                        }
                    },
                    [sizer](auto widget){
                        sizer->Add(widget, wxSizerFlags().Top().Expand().Border(wxALL, 3));
                    });
                    on_size = [resize,on_size](int new_size){
                        resize(new_size);
                        on_size(new_size);
                    };
                }
            }
            frame->SetSizer(sizer);
            frame->Bind( wxEVT_SIZE, [on_size,sizer](wxSizeEvent& event) {
                on_size(event.GetSize().GetWidth());
                event.Skip();
            } );
            return sinks;
        }

        static auto split(std::string const &src, char delimiter = '.') {
            std::stringstream ss(src);
            std::vector<std::string> result;
            std::string word;
            while (std::getline(ss, word, delimiter)) {
                result.push_back(word);
            }
            return result;
        }

        void ResolveSinks(TSinks &sinks, std::string const &data) {
            rapidjson::Document doc;
            doc.Parse(data.c_str());
            auto t{0};
            for (auto &sink: sinks) {
                std::reference_wrapper<const rapidjson::Value> last_value{doc};
                for(std::string const &member: split(sink.first)) {
                    auto o {last_value.get().GetObject()};
                    last_value = std::cref(o[member.c_str()]);
                }
                auto const result{last_value.get().GetString()};
                sink.second(result);
            }
        }

        void ShowCard(std::string const &locator, std::string const &data, Frame *frame) {
            auto const result {cardprovider_(locator, data)};
            auto sinks {CreateCardTemplate(result.first, frame)};
            ResolveSinks(sinks, result.second);
            current_card_ = locator;
        }
    };
}

wxBEGIN_EVENT_TABLE(AdaptiveCards::Frame, wxFrame)
EVT_MENU(wxID_EXIT, AdaptiveCards::Frame::OnExit)
wxEND_EVENT_TABLE()

AdaptiveCards::CurlInit AdaptiveCards::url_stream::curl_init_;
