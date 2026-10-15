# Tests: Windows demo build

The demo package built by `tools/windows/build_demo.ps1` ([../tech/windows.md](../tech/windows.md), "Demo build"):
client with Windsward, Everfall and the arena, three native servers (one per map) and nw-core in Docker Desktop. Run
on the Windows machine. For each item, send the script output or the server log (`deploy\build\logs\<map>.log`) if
something is wrong.

## Build

1. **First run.** `.\tools\windows\build_demo.ps1 -HostAddress <IP of the PC> -Admin <account>`: ends with "Done"; `deploy\build` has `Windows\NWPort.exe`, `WindowsServer\NWPortServer.exe`, `Servidores.bat`, `Jogar.bat` and `NWExiles-demo-client.zip`.
2. **Checks.** With the editor open, or Docker Desktop stopped, or `NW_UE_SOURCE` missing from `local.env`: the script stops at the start with a clear message.
3. **Second run.** Run it again with `-SkipCook`: no secret changes in `deploy\.env`, nw-core keeps the accounts, the launchers are rewritten.
4. **Secrets.** Without `deploy\.env`: the script creates it and prints no secret.

## Servers and entry

5. **Servers.** `deploy\build\Servidores.bat`: three windows (Windsward :7777, Everfall :7778, Arena :7779); each log shows `server: backend http://127.0.0.1:8090, pass required`.
6. **Entry without -nwconnect.** `deploy\build\Jogar.bat`: login screen; Play with a new character enters Windsward.
7. **Saved map.** Leave the game while in Everfall and play again: you enter Everfall, at the place you left.

## Travel (F10 > World)

8. **F10 for admins.** With an account flagged by `-Admin`: F10 opens. With another account: F10 does not open.
9. **Everfall.** F10 > Go to Everfall: the screen loads Everfall at the map's start; inventory, coins and levels are the same.
10. **Arena and back.** F10 > Go to the arena, then Go to South Windsward: each trip lands on the right server (window of that map logs `join:`, the old one logs `leave:`).
11. **Others stay.** Two players in Windsward; one goes to the arena: the other stays in Windsward, without loading.
12. **Server down.** Close the Arena window and press Go to the arena: the client goes back to the login screen with a connection error (the character is already saved in the arena). `Servidores.bat arena` and Play: enters the arena.

## Friends

13. **Zip.** On another PC on the network, unzip `NWExiles-demo-client.zip` and run `Windows\Jogar.bat`: logs in with an invite and enters Windsward.
