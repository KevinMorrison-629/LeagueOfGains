import requests
import json
import os
import sys

# Configuration
CONFIG_FILE = "LeagueOfGains.cfg"
API_VERSION = "10"

def load_config():
    """Attempts to load the configuration from the local config file."""
    if not os.path.exists(CONFIG_FILE):
        print(f"Error: Could not find {CONFIG_FILE} in the current directory.")
        return {}
    
    try:
        with open(CONFIG_FILE, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error parsing {CONFIG_FILE}: {e}")
        return {}

def get_application_id(token, config_id=None):
    """
    Determines Application ID from config, token extraction, or user input.
    """
    if config_id and config_id != "YOUR_APPLICATION_ID_HERE":
        print(f"Loaded Application ID from config.")
        return config_id

    try:
        import base64
        # The ID is the part before the first period
        id_part = token.split('.')[0]
        # Add padding if necessary for base64 decoding
        id_part += '=' * (-len(id_part) % 4)
        decoded_id = base64.b64decode(id_part).decode('utf-8')
        if decoded_id.isdigit():
            print(f"Detected Application ID from token: {decoded_id}")
            return decoded_id
    except:
        pass
    
    return input("Please enter your Discord Application ID (Client ID): ").strip()

def clear_commands(token, app_id, guild_id=None):
    """
    Sends a PUT request with an empty list to clear commands.
    If guild_id is provided, clears commands for that specific guild.
    """
    headers = {
        "Authorization": f"Bot {token}",
        "Content-Type": "application/json"
    }
    
    if guild_id:
        url = f"https://discord.com/api/v{API_VERSION}/applications/{app_id}/guilds/{guild_id}/commands"
        target_desc = f"Guild {guild_id}"
    else:
        url = f"https://discord.com/api/v{API_VERSION}/applications/{app_id}/commands"
        target_desc = "GLOBAL"

    print(f"Sending request to clear {target_desc} commands...")
    
    try:
        response = requests.put(url, headers=headers, json=[])
        
        if response.status_code in [200, 201, 204]:
            print(f"SUCCESS! {target_desc} commands have been cleared.")
            if not guild_id:
                print("   (Global updates may take up to 1 hour to propagate)")
        else:
            print(f"Failed to clear {target_desc} commands. Status: {response.status_code}")
            print(f"   Response: {response.text}")
            
    except Exception as e:
        print(f"Exception occurred: {e}")

def main():
    print("--- LeagueOfGains Command Cleaner ---")

    # 1. Get Config & Token
    config = load_config()
    token = config.get("bot_token")
    
    if not token or token == "YOUR_DISCORD_BOT_TOKEN_HERE":
        print("Config file missing or has placeholder token.")
        token = input("Enter your Bot Token: ").strip()
    else:
        print("Loaded Bot Token from config.")

    # 2. Get App ID
    app_id = get_application_id(token, config.get("application_id"))

    if not app_id or not token:
        print("Missing credentials.")
        return

    # 3. Clear Global Commands
    print("\n--- Phase 1: Global Commands ---")
    clear_commands(token, app_id, guild_id=None)

    # 4. Clear Guild Commands (Loop)
    print("\n--- Phase 2: Guild/Server Commands ---")
    print("Test servers often use Guild Commands because they update instantly.")
    print("To fix 'stuck' commands in a specific server, enter its ID below.")
    
    while True:
        user_input = input("\nEnter Guild ID to clear (or press Enter to finish): ").strip()
        
        if not user_input:
            break
            
        if user_input.isdigit():
            clear_commands(token, app_id, guild_id=user_input)
        else:
            print("Invalid ID. Please enter numeric digits only.")

    print("\n Done.")

if __name__ == "__main__":
    # check for requests library
    try:
        import requests
    except ImportError:
        print("This script requires the 'requests' library.")
        print("   Run: pip install requests")
        sys.exit(1)

    main()