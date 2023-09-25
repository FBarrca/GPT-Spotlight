#include <iostream>
#include <thread>
#include <Windows.h>
#include <uiohook.h>
#include <fstream>
#include "clip.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "immapp/immapp.h"
#include "imgui_md_wrapper.h"
#include "imgui-command-palette/imcmd_command_palette.h"

#include "liboai.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
std::atomic<bool> commandPaletteAtom(false);
std::mutex mtx;
std::condition_variable cv;
int finished_threads = 0;

// KEYCODES constants
#define KEYCODE_LEFT_SHIFT 42
#define KEYCODE_LEFT_CONTROL 29
#define KEYCODE_X 45
#define KEYCODE_ESC 1

// Create a struct to hold the prompt and output
struct PromptOutput
{
    std::string prompt;
    std::string output;
    PromptOutput(std::string prompt, std::string output) : prompt(prompt), output(output) {}
};
// Create a vector to hold the prompt and output empty initially
std::vector<std::shared_ptr<PromptOutput>> Outputs = {};

bool call_api(const std::string &prompt_action, std::string &output)
{
    liboai::OpenAI oai;
    liboai::Conversation convo;

    std::string clipboard_content;
    clip::get_text(clipboard_content);
    std::string prompt = prompt_action + " \n" + clipboard_content;
    convo.AddUserData(prompt);

    std::ifstream file("openai.json");
    if (!file.is_open())
    {
        std::cerr << "Error: Unable to open openai.json" << std::endl;
        return false;
    }

    json openaiJson;
    file >> openaiJson;
    std::string api_key = openaiJson["apikey"];
    std::string model = openaiJson["model"];

    try
    {
        if (oai.auth.SetKey(api_key))
        {
            liboai::Response response = oai.ChatCompletion->create(model, convo);
            std::cout << "Sent prompt: " << prompt << std::endl;
            convo.Update(response);
            std::string response_text = convo.GetLastResponse();
            output = response_text;
            return true;
        }
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }
    return false;
}

void call_api_async(const std::string &prompt_action, std::function<void(bool, const std::string &)> callback)
{
    std::thread([prompt_action, callback]()
                {
        std::string output;
        bool success = call_api(prompt_action, output);
        
        // Ensure thread-safety when modifying the Outputs vector
        {
            std::lock_guard<std::mutex> lock(mtx);
            Outputs.push_back(std::make_shared<PromptOutput>(prompt_action, output)); // Add output to Outputs vector
        }
        
        callback(success, output); })
        .detach();
}

void LoadCommands(ImCmd::Context *context)
{
    std::ifstream file("commands.json");
    if (!file.is_open())
    {
        std::cerr << "Error: Unable to open commands.json" << std::endl;
        return;
    }

    json commandsJson;
    file >> commandsJson;

    for (const auto &commandElement : commandsJson)
    {
        std::string commandName = commandElement["name"];
        std::string commandType = commandElement["type"];
        std::string commandAction = commandElement["action"];

        ImCmd::Command command;
        command.Name = commandName; // Assigning without std::move and .c_str()
        command.InitialCallback = [commandAction, &special_keys_pressed = commandPaletteAtom]()
        {
            std::string clipboard_content;
            clip::get_text(clipboard_content);
            std::cout << "InitialCallback: " << commandAction << " " << clipboard_content << std::endl;
            special_keys_pressed.store(false);
            std::string output;
            std::string prompt = commandAction + " \n" + clipboard_content;
            call_api_async(prompt, [](bool success, const std::string &output)
                           {
                if (success)
                {
                    std::cout << "Output: " << output << std::endl;
                }
                else
                {
                    std::cerr << "API call failed" << std::endl;
                } });
        };
        ImCmd::AddCommand(std::move(command));
    }
}

void DemoCommandPalette()
{

    static bool wasInited = false;
    static bool showCommandPalette = false;
    static ImCmd::Context *commandPaletteContext = nullptr;
    static int counter = 0;
    auto initCommandPalette = []()
    {
        commandPaletteContext = ImCmd::CreateContext();
        ImVec4 highlight_font_color(1.0f, 0.0f, 0.0f, 1.0f);
        ImCmd::SetStyleColor(ImCmdTextType_Highlight, ImGui::ColorConvertFloat4ToU32(highlight_font_color));

        // Add theme command: a two steps command, with initial callback + SubsequentCallback
        {
            ImCmd::Command select_theme_cmd;
            select_theme_cmd.Name = "Select theme";
            select_theme_cmd.InitialCallback = [&]()
            {
                ImCmd::Prompt(std::vector<std::string>{
                    "Classic",
                    "Dark",
                    "Light",
                });
            };
            select_theme_cmd.SubsequentCallback = [&](int selected_option)
            {
                switch (selected_option)
                {
                case 0:
                    ImGui::StyleColorsClassic();
                    break;
                case 1:
                    ImGui::StyleColorsDark();
                    break;
                case 2:
                    ImGui::StyleColorsLight();
                    break;
                default:
                    break;
                }
            };
            ImCmd::AddCommand(std::move(select_theme_cmd));
        }

        // Simple command that increments a counter
        {
            ImCmd::Command inc_cmd;
            inc_cmd.Name = "increment counter";
            inc_cmd.InitialCallback = []
            { counter += 1; };
            ImCmd::AddCommand(inc_cmd);
        }

        LoadCommands(commandPaletteContext);
    };

    if (!wasInited)
    {
        initCommandPalette();
        wasInited = true;
    }

    showCommandPalette = commandPaletteAtom.load();
    if (showCommandPalette)
    {
        ImCmd::CommandPaletteWindow("CommandPalette", &showCommandPalette);
    }
}

void drawGUI()
{
    std::vector<std::shared_ptr<PromptOutput>> localOutputs;

    // Lock the mutex, make a copy of Outputs, then release the lock
    {
        std::lock_guard<std::mutex> lock(mtx);
        localOutputs = Outputs;
    }

    // Set viewport to main
    // Create a new floating window
    for (std::size_t index = 0; index < localOutputs.size(); ++index)
    {
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        // do not dock to the viewport
        std::string windowName = "Output " + std::to_string(index);
        if (ImGui::Begin(windowName.c_str(), nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse))
        {
            // Close button
            if (ImGui::Button("Close"))
            {
                // Remove the output from the Outputs vector
                // lock mutex
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    Outputs.erase(std::remove_if(Outputs.begin(), Outputs.end(), [&](std::shared_ptr<PromptOutput> &p)
                                                 { return p->prompt == localOutputs[index]->prompt; }),
                                  Outputs.end());
                }
            }
            // Copy button
            ImGui::SameLine();
            if (ImGui::Button("Copy anwser"))
            {
                clip::set_text(localOutputs[index]->output);
            }
            std::string title = "**" + localOutputs[index]->prompt + "**";
            ImGuiMd::RenderUnindented(title.c_str());
            std::string outputContainer = windowName + " output";
            if (ImGui::BeginChild(outputContainer.c_str(), ImVec2(0, 0), true))
            {
                ImGuiMd::Render(localOutputs[index]->output.c_str());
                ImGui::EndChild();
            }
            ImGui::End();
        }
    }

    DemoCommandPalette();
}

void setupViewports()
{
    for (int n = 0; n < ImGui::GetPlatformIO().Viewports.Size; n++)
    {
        ImGuiViewport *viewport = ImGui::GetPlatformIO().Viewports[n];
        // Assuming all viewports should be shown
        viewport->Flags &= ~ImGuiViewportFlags_NoRendererClear;
    }
}

void runGUI()
{
    HelloImGui::RunnerParams runnerParams;
    runnerParams.appWindowParams.windowTitle = "GPT Spotlight";
    runnerParams.appWindowParams.borderless = true;
    runnerParams.appWindowParams.hidden = true;

    // runnerParams.appWindowParams.windowGeometry.sizeAuto = true;
    // Set full width but not full height
    runnerParams.appWindowParams.windowGeometry.windowSizeMeasureMode = HelloImGui::WindowSizeMeasureMode::RelativeTo96Ppi;
    runnerParams.appWindowParams.windowGeometry.size = HelloImGui::ScreenSize{2080, 1};
    runnerParams.appWindowParams.windowGeometry.positionMode = HelloImGui::WindowPositionMode::MonitorCenter;
    // runnerParams.appWindowParams.windowGeometry.fullScreenMode = HelloImGui::FullScreenMode::FullMonitorWorkArea;
    runnerParams.imGuiWindowParams.backgroundColor = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);
    runnerParams.imGuiWindowParams.enableViewports = true;

    runnerParams.callbacks.ShowGui = drawGUI;

    ImmApp::AddOnsParams addOnsParams;
    addOnsParams.withMarkdown = true;
    addOnsParams.withImplot = false;

    ImmApp::Run(runnerParams, addOnsParams);
}

// This function will be called whenever a key is pressed or released.
void dispatch_proc(uiohook_event *const event)
{
    static bool shift_pressed = false;
    static bool ctrl_pressed = false;

    switch (event->type)
    {
    case EVENT_KEY_PRESSED:
        std::cout << "Key pressed: " << event->data.keyboard.keycode << std::endl;
        if (event->data.keyboard.keycode == KEYCODE_LEFT_SHIFT)
        {
            shift_pressed = true;
        }
        if (event->data.keyboard.keycode == KEYCODE_LEFT_CONTROL)
        {
            ctrl_pressed = true;
        }
        if (shift_pressed && ctrl_pressed && event->data.keyboard.keycode == KEYCODE_X)
        {
            commandPaletteAtom.store(true); // Notify the gui thread
        }
        // Escape key while the special keys are pressed is true
        if (commandPaletteAtom.load() && event->data.keyboard.keycode == KEYCODE_ESC)
        {
            commandPaletteAtom.store(false); // Notify the gui thread
        }
        break;
    case EVENT_KEY_RELEASED:
        if (event->data.keyboard.keycode == KEYCODE_LEFT_SHIFT)
        {
            shift_pressed = false;
        }
        if (event->data.keyboard.keycode == KEYCODE_LEFT_CONTROL)
        {
            ctrl_pressed = false;
        }
        break;
    }
}

void CreateConsole()
{
    AllocConsole();
    FILE *fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    SetConsoleTitle("Debug Console");
    std::cout << "Debug Console Initialized" << std::endl;
}

int main()
{
    // CreateConsole(); // Create a console window for debugging

    hook_set_dispatch_proc(dispatch_proc);

    std::thread hookThread(hook_run); // Run hook_run in a separate thread.
    std::thread guiThread(runGUI);

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []
            { return finished_threads >= 2; });
    hookThread.join();
    guiThread.join();
    hook_stop();

    // FreeConsole(); // Free the console window
    return 0;
}