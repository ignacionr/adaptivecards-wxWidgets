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
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

#include <iostream>

namespace AdaptiveCards
{
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
            auto frame = new Frame("Hello World", wxPoint(50, 50), wxSize(450, 340));
            frame->Show(true);
            ShowCard(initial_card, "{}", frame);
            return true;
        }

        using TSinks = std::vector<std::pair<std::string,std::function<void(std::string)>>>;

        auto CreateCardTemplate(std::string const &src, Frame *frame) {
            TSinks sinks;
            rapidjson::Document doc;
            std::regex const match_expr {"\\$\\{(.+)\\}"};
            doc.Parse(src.c_str());
            auto sizer {new wxBoxSizer(wxVERTICAL)};
            std::function<void(int)> on_size = +[](int){};
            for (auto &element: doc["body"].GetArray()) {
                if (std::string("TextBlock") == element["type"].GetString()) {
                    auto text{element.HasMember("text") ? std::string(element["text"].GetString()) : std::string()};
                    auto label {new wxStaticText(frame, -1, 
                        text.c_str())};
                    label->SetAutoLayout(true);
                    // if (element.HasMember("wrap") && element["wrap"].GetBool()) {
                    //     label->Wrap(frame->m_width);
                    //     std::cerr << __LINE__ << " width: " << frame->m_width << "\n";
                    // }
                    auto font {label->GetFont()};
                    if (element.HasMember("size")) {
                        auto size_value {element["size"].GetString()};
                        if (std::string("Medium") == size_value) {
                            font.SetPointSize(font.GetPointSize() * 3 / 2);
                        }
                    }
                    label->SetFont(font);
                    sizer->Add(label, wxSizerFlags().Top().Expand().Border(wxALL, 3));
                    std::smatch match_result;
                    if (std::regex_match(text, match_result, match_expr)) {
                        sinks.push_back(std::make_pair(match_result[1], 
                        [label,sizer,w=frame->m_width](auto v){ 
                            label->SetLabelText(v);
                            label->Wrap(w);
                            sizer->Layout();
                        }));
                        on_size = [on_size,label](int new_size){
                            on_size(new_size);
                            std::string t{label->GetLabelText()};
                            for(auto pos {t.find('\n')}; pos != std::string::npos; pos = t.find('\n')) {
                                t.replace(pos, 1, std::string(1, ' '));
                            }
                            label->SetLabelText(t.c_str());
                            label->Wrap(new_size);
                        };
                    }
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
