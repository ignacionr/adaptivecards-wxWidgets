#include <utility>
#include <string>
#include <fstream>
#include <iterator>

#include "adaptivecards-wx.h"

struct CardsProvider {
    std::pair<std::string, std::string> operator()(std::string const &card_locator, std::string const &posted_data) {
        std::stringstream card_template;
        {
            std::ifstream template1{"card_template1.json"};
            card_template << template1.rdbuf();
        }
        std::stringstream card_contents;
        {
            std::ifstream card1{"card1.json"};
            card_contents << card1.rdbuf();
        }
        return std::make_pair(card_template.str(), card_contents.str());
    }
};

constexpr char initial_card[] {"/"};

using MyApp = AdaptiveCards::App<CardsProvider, initial_card>;

wxIMPLEMENT_APP(MyApp);
