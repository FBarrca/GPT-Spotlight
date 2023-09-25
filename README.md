# GPT-Spotlight

I developed this handy tool last year to facilitate easy access to ChatGpt without disrupting my workflow, but forgot to open-source it. It allows you to create personalized prompts for any content you have in your clipboard, eliminating the need to open a new tab for every small task.

These prompts are stored in a JSON file called `commands.json`, and you have the freedom to add new prompts by modifying the file.

To launch the application, simply press `Ctrl+Shift+X`, and to exit, press `Esc` (of course, you need to run the `.exe` file first). You can conveniently click on any of the available prompts or use the search bar for a quick fuzzy search.

You can also change the model in the `openai.json` file and set your own API key.

![Demo](./demo.gif)

The application is built using ImGui, libuiohook, and clip, so it should be 100% cross-platform, although I have only compiled it for Windows.

## Installation

Just download the latest release; it's a portable application, it doesn't get installed. To run it, just double-click on the `.exe` file, after you have set up the `openai.json` file. The app doesn't run on startup, but you can add it to your startup folder if you want.
