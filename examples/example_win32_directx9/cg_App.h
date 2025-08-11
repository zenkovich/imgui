#pragma once

#include <string>

#include "cg_Config.h"
#include "cg_SourceParser.h"
#include "cg_Physics.h"
#include "cg_GraphRenderer.h"

namespace cg
{
    class App
    {
    public:
        App();

        bool Initialize(const std::string& configJsonText);
        void Frame(float dtMs, const ImVec2& canvasPos, const ImVec2& canvasSize);

    private:
        Config _config;
        SourceParser::Result _parseResult;
        Physics* _physics = nullptr;
        GraphRenderer* _renderer = nullptr;
        bool _paused = false;
        bool _pauseIntegrate = false;
        bool _pauseConstraints = false;
        bool _pauseRepulsion = false;
    };
}


