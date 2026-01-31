# Installation & Build Guide

## Prerequisites

Before you begin, ensure you have the following installed on your system:

*   **[Git](https://git-scm.com/downloads)**: For cloning the repository.
*   **[CMake](https://cmake.org/download/)** (v3.21+): Cross-platform build system.
*   **C++ Compiler**:
    *   **Windows**: [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (Community Edition is free). Ensure the "Desktop development with C++" workload is selected during installation.

## 1. Clone the Repository

This project uses [vcpkg](https://github.com/microsoft/vcpkg) as a submodule for dependency management. You must clone recursively to include it.

```bash
git clone --recursive https://github.com/KevinMorrison-629/LeagueOfGains.git
cd LeagueOfGains
```

> **Note**: If you already cloned without `--recursive`, run `git submodule update --init --recursive` to fetch the vcpkg submodule.

## 2. Configuration

The bot requires a configuration file to store your API keys and settings.

1.  **Copy the Example Config**:
    Copy the provided `LeagueOfGains_example.cfg` to `LeagueOfGains.cfg` in the root directory.
    ```powershell
    # Windows PowerShell
    Copy-Item LeagueOfGains_example.cfg LeagueOfGains.cfg
    ```

2.  **Edit `LeagueOfGains.cfg`**:
    Open the file in any text editor and fill in your credentials:
    *   `bot_token`: Your Discord Bot Token (from the [Discord Developer Portal](https://discord.com/developers/applications)).
    *   `riot_api_key`: Your Riot API Key (from the [Riot Developer Portal](https://developer.riotgames.com/)).
    *   `application_id`: Your Discord Application ID (found in the General Information tab of your Discord App).

## 3. Build Instructions

### Option A: Using Visual Studio 2022 (Recommended)
1.  Open Visual Studio.
2.  Select **Open a local folder** and choose the `LeagueOfGains` directory.
3.  Visual Studio should detect `CMakePresets.json` automatically and configure the project.
    *   *Note: This may take a few minutes the first time as it downloads and builds dependencies via vcpkg.*
4.  Once configuration implies "Ready", select `default` from the configuration dropdown (if not already selected).
5.  Press **Build** > **Build All** (or `Ctrl+Shift+B`).

### Option B: Command Line (CLI)

1.  **Bootstrap vcpkg**:
    Initialize the package manager (only needed once).
    ```powershell
    .\external\vcpkg\bootstrap-vcpkg.bat
    ```

2.  **Configure**:
    Run CMake with the `default` preset.
    ```powershell
    cmake --preset default
    ```

3.  **Build**:
    Compile the application (this will also build all dependencies).
    ```powershell
    cmake --build --preset default
    ```

## 4. Running the Application

After a successful build, the executable is generated in the build folder.

```powershell
# Navigate to the output directory (path may vary slightly based on config)
cd build\server\Debug

# Run the bot
.\server.exe
```

> **Important**: The build process automatically copies `LeagueOfGains.cfg` and necessary DLLs (like `dpp.dll`, `libssl`, etc.) to the output directory. If you modify your config file in the root directory, you must **rebuild** (or manually copy the config) for changes to take effect in the executable folder.
