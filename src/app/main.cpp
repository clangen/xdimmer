//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include <cursespp/LayoutBase.h>

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
static const int MIN_WIDTH = 54;
static const int DEFAULT_HEIGHT = 26;
static const int MIN_HEIGHT = 12;

using namespace cursespp;

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

struct Monitor {
    const std::string name;
    const float brightness;
};

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
        std::string command = str::fmt(
            "xrandr --output %s --brightness %f\n",
            monitor.name.c_str(),
            brightness);
        //std::cout << command;
        redi::opstream out(command);
    }
}

class MainLayout: public LayoutBase {
    public:
        MainLayout() : LayoutBase() {
            this->label = std::make_shared<TextLabel>();
            this->label->SetText("xdimmer", text::AlignCenter);
            this->SetFrameVisible(true);
            this->SetFrameTitle(APP_NAME);
            this->AddWindow(label);
        }

        virtual void OnLayout() override {
            this->label->MoveAndResize(0, this->GetContentHeight() / 2, this->GetContentWidth(), 1);
        }

    private:
        std::shared_ptr<TextLabel> label;
};

#ifdef WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    PDC_set_resize_limits(MIN_HEIGHT, MAX_SIZE, MIN_WIDTH, MAX_SIZE);
    resize_term(DEFAULT_HEIGHT, DEFAULT_WIDTH); /* must be before app init */

    if (App::Running(APP_NAME)) {
        return 0;
    }
#else
int main(int argc, char* argv[]) {
#endif
    f8n::env::Initialize(APP_NAME, 1);

    f8n::debug::Start({
        new f8n::debug::FileBackend(f8n::env::GetDataDirectory() + "log.txt")
    });

    f8n::debug::info("main.cpp", "hello, world!");

    App app(APP_NAME); /* must be before layout creation */

    app.SetMinimumSize(MIN_WIDTH, MIN_HEIGHT);

    app.SetKeyHandler([&](const std::string& kn) -> bool {
        return false;
    });

    //app.Run(std::make_shared<MainLayout>());
    auto monitors = cmd::query();
    for (auto& m : monitors) {
        cmd::update(m, m.brightness == 1.0 ? 0.75 : 1.0);
    }

    f8n::debug::Stop();

    return 0;
}
