//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2019 casey langen
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

#include <f8n/debug/debug.h>
#include <f8n/environment/Environment.h>

#include <iostream>
#include <vector>
#include <string>
#include <cassert>

#include "pstream.h"

static const std::string APP_NAME = "xdimmer";
static const int MAX_SIZE = 1000;
static const int DEFAULT_WIDTH = 100;
static const int MIN_WIDTH = 24;
static const int DEFAULT_HEIGHT = 26;
static const int MIN_HEIGHT = 3;

using namespace cursespp;

struct Monitor {
    const std::string name;
    const float brightness;
};

namespace str {
    std::string trim(const std::string &s) {
        /* so lazy https://stackoverflow.com/a/17976541 */
        auto front = std::find_if_not(s.begin(), s.end(), isspace);
        auto back = std::find_if_not(s.rbegin(), s.rend(), isspace).base();
        return (back <= front ? std::string() : std::string(front, back));
    }

    std::vector<std::string> split(const std::string& str, const std::string& delimiters) {
        using ContainerT = std::vector<std::string>;
        ContainerT tokens;
        std::string::size_type pos, lastPos = 0, length = str.length();
        using value_type = typename ContainerT::value_type;
        using size_type = typename ContainerT::size_type;
        while (lastPos < length + 1) {
            pos = str.find_first_of(delimiters, lastPos);
            if (pos == std::string::npos) {
                pos = length;
            }
            if (pos != lastPos) {
                std::string token = trim(value_type(
                    str.data() + lastPos, (size_type) pos - lastPos));

                if (token.size()) {
                    tokens.push_back(token);
                }
            }
            lastPos = pos + 1;
        }
        return tokens;
    }

    template<typename... Args>
    static std::string fmt(const std::string& format, Args ... args) {
        size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; /* extra space for '\0' */
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        return std::string(buf.get(), buf.get() + size - 1); /* omit the '\0' */
    }
}

namespace cmd {
    std::vector<std::string> queryNames() {
        std::vector<std::string> names;
        redi::ipstream in("xrandr -q | grep \" connected \"");
        for (std::string line; std::getline(in, line);) {
            auto parts = str::split(line, " ");
            if (parts.size()) {
                //std::cout << "name: " << parts[0] << "\n";
                names.push_back(parts[0]);
            }
        }
        return names;
    }

    std::vector<float> queryValues() {
        std::vector<float> values;
        redi::ipstream in("xrandr --verbose | grep -i brightness");
        for (std::string line; std::getline(in, line);) {
            auto parts = str::split(line, " ");
            if (parts.size() > 1) {
                //std::cout << "value: " << parts[1] << "\n";
                values.push_back(std::stof(parts[1]));
            }
        }

        return values;
    }

    std::vector<Monitor> query() {
        auto names = queryNames();
        auto values = queryValues();
        assert(names.size() == values.size());
        std::vector<Monitor> monitors;
        for (size_t i = 0; i < names.size(); i++) {
            monitors.push_back(Monitor{names[i], values[i]});
        }
        return monitors;
    }

    void update(const Monitor& monitor, float brightness) {
        if (brightness < 0.05) { brightness = 0.05; }
        std::string command = str::fmt(
            "xrandr --output %s --brightness %f\n",
            monitor.name.c_str(),
            brightness);
        //std::cout << command;
        redi::opstream out(command);
    }
}

namespace ui {
    static std::string formatRow(size_t width, const std::vector<Monitor>& monitors, size_t index) {
        auto& m = monitors[index];

        size_t maxLeft = 0;
        for (auto& m: monitors) {
            if (m.name.size() > maxLeft) {
                maxLeft = m.name.size();
            }
        }

        size_t maxRight = 5; /* ' 100%' */

        std::string leftText = text::Align(
            m.name, text::AlignRight, (int) maxLeft);

        std::string rightText = text::Align(
            std::to_string((int)(round(m.brightness * 100.0))) + "%",
            text::AlignRight,
            (int) maxRight);

        int trackWidth = (int) width - ((int) maxRight + (int) maxLeft + 3);
        int thumbOffset = std::max(0, (int)(m.brightness * (float) trackWidth) - 1);
        std::string trackText = " ";
        for (int i = 0; i < trackWidth; i++) {
            trackText += (i == thumbOffset) ? "■" : "─";
        }

        return " " + leftText + trackText + rightText;
    }

    class MonitorAdapter: public ScrollAdapterBase {
        public:
            MonitorAdapter() {
                this->Refresh();
            }

            virtual ~MonitorAdapter() {
            }

            virtual size_t GetEntryCount() override {
                return this->monitors.size();
            }

            virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override {
                std::string formatted = formatRow(window->GetContentWidth(), this->monitors, index);
                auto entry = std::make_shared<SingleLineEntry>(formatted);
                entry->SetAttrs(Color::Default);
                if (index == window->GetScrollPosition().logicalIndex) {
                    entry->SetAttrs(Color::ListItemHighlighted);
                }
                return entry;
            }

            void Update(size_t index, float delta) {
                auto& m = this->monitors[index];
                float newValue = m.brightness + delta;
                if (newValue > 1.0) { newValue = 1.0; }
                if (newValue < 0.0) { newValue = 0.0; }
                cmd::update(m, newValue);
                this->Refresh();
            }

        private:
            void Refresh() {
                this->monitors = cmd::query();
            }

            std::vector<Monitor> monitors;
    };

    class MainLayout: public LayoutBase {
        public:
            MainLayout() : LayoutBase() {
                this->adapter = std::make_shared<MonitorAdapter>();
                this->listWindow = std::make_shared<ListWindow>(this->adapter);
                this->AddWindow(this->listWindow);
                this->listWindow->SetFocusOrder(0);
                this->listWindow->SetFrameVisible(true);
                this->listWindow->SetFrameTitle("xdimmer");
            }

            virtual void OnLayout() override {
                this->listWindow->MoveAndResize(
                    0, 0, this->GetContentWidth(), this->GetContentHeight());
            }

            virtual bool KeyPress(const std::string& key) override {
                if (key == "KEY_LEFT") {
                    this->UpdateSelected(-0.05);
                    return true;
                }
                else if (key == "KEY_RIGHT") {
                    this->UpdateSelected(0.05);
                    return true;
                }
                else if (key == "kLFT5") {
                    this->UpdateSelected(-0.25);
                    return true;
                }
                else if (key == "kRIT5") {
                    this->UpdateSelected(0.25);
                    return true;
                }
                return false;
            }

        private:
            void UpdateSelected(float delta) {
                auto index = this->listWindow->GetSelectedIndex();
                this->adapter->Update(index, delta);
                this->listWindow->OnAdapterChanged();
            }

            std::shared_ptr<ListWindow> listWindow;
            std::shared_ptr<MonitorAdapter> adapter;
    };
}

int main(int argc, char* argv[]) {
    f8n::env::Initialize(APP_NAME, 1);
    f8n::debug::Start({ new f8n::debug::SimpleFileBackend() });

    App app(APP_NAME);
    app.SetMinimumSize(MIN_WIDTH, MIN_HEIGHT);
    app.Run(std::make_shared<ui::MainLayout>());

    f8n::debug::Stop();

    return 0;
}
