# League of Gains

*"We'll either end this season Masters... or be jacked as shit." -RonDog* 

---

League of Gains is a Discord bot that turns your League of Legends deaths into physical exercise. It tracks your games automatically and assigns "penance" based on your performance (specifically, how much you feed).

---

## Features

* **Automatic Tracking**: Polls the Riot API to detect new matches.
* **Multi-Account Support**: Link as many Riot accounts as you want to a single Discord user.
* **Customizable Difficulty**: Set global or muscle-group specific multipliers ("Wimp Mode") to scale the workout to your fitness level.
* **Exercise Rerolls**: Don't like the assigned exercise? Reroll it for a different one (based on the original death count).
* **Stat Tracking**: Keeps track of total deaths, reps completed, and your most "inted" champions.

## Commands

### Setup & Account Management
`/link`: Connects a Riot Account to your Discord ID.
* **Params**:
    * `name` (string): Your Riot Game Name.
    * `tag` (string): Your Riot Tag Line.
    * `region` (string): Region code (e.g., na1, euw1, kr, jp1).
* **Logic**: Verifies the account exists via Riot API and links the PUUID to your Discord ID in the database.

`/wimp`: Adjusts the difficulty of assigned exercises. Default multiplier is 1.0.
* **Params**:
    * `multiplier` (number): The scaling factor (e.g., 0.5 for half reps, 2.0 for double).
    * `type` (string, optional): Specific muscle group to apply this to (upper, lower, core). If omitted, applies to all groups.
### Workout Management
`/penance`:
View your current backlog of exercises.
* **Params**: None.
* **Output**: Returns a list of pending punishments, including the Game ID, Exercise Name, Rep Count, and original Death Count.

`/complete`: Mark a specific punishment as done.
* **Params**:
    * `gameid` (string): The Match ID associated with the task (found via /penance).
* **Logic**: Removes the task from the queue and archives it in your history stats.

`/reroll`: Swap an assigned exercise for a random new one from the server configuration.
* **Params**:
    * gameid (string): The Match ID to reroll.
* **Logic**: Picks a new random exercise. Recalculates reps: Original Deaths * New Exercise Base Count * Your Multiplier.

### Information
`/stats`: Displays your fitness and gaming statistics.
* **Params**: None.
* **Output**: Total deaths, games tracked, lowest KDA, most deaths in a single game, top 3 "death" champions, and total reps completed per exercise.

`/punishments`: Lists all potential exercises defined in the server configuration.
* **Params**: None.
* **Output**: Lists Exercise Name, Muscle Group, and Base Rep Count per death.

`/forcefetch`: Manually triggers the match update scanner.
* **Params**: None.
* **Restriction**: 60-second global cooldown per server.

## Configuration (`LeagueOfGains.cfg`)

The bot requires a JSON configuration file to run.
```
{
  "bot_token": "YOUR_DISCORD_BOT_TOKEN",
  "riot_api_key": "YOUR_RIOT_API_KEY",
  "thread_pool_size": 4,
  "exercises": [
    { "name": "Pushups", "count": 10, "type": "upper" },
    { "name": "Squats", "count": 20, "type": "lower" },
    { "name": "Situps", "count": 15, "type": "core" },
    { "name": "Burpees", "count": 5, "type": "lower" }
  ]
}
```

## Logic Flow
1. Initialization: On startup, the bot loads exercises from the config into the database.
2. Monitoring: Every 5 minutes, the bot checks the match history for every linked user.
3. Detection: If a new match ID is found:
    * It analyzes the match stats.
    * If deaths > 0, it selects a random exercise.
    * Calculation: Reps = Deaths * Base_Rep_Count * User_Multiplier.
    * The task is added to the user's /penance queue.
    * The user receives a DM with the details.
4. Completion: The user performs the exercise and runs /complete <gameid> to clear it.