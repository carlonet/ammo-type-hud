# Ammo Type on HUD

[Nexus Mods page](https://www.nexusmods.com/fallout4/mods/110793) · F4SE plugin for **Fallout 4 1.11.240** that shows the ammo type of the equipped weapon next to the vanilla
HUD ammo counter, plus the equipped grenade/mine and how many you have, in the HUD's own font and colour. No ESP/ESL, nothing stored in saves.

![Ammo type next to the counter](rilascio/immagine-2-dettaglio.png)

## How it works

- A `TextField` is added inside `RightMeters_mc.AmmoCount_mc` of `Interface\HUDMenu.swf`, to the left of
  `ClipCount_tf` / `ReserveCount_tf`, copying their text format. It inherits the HUD colour and is hidden with
  the counter.
- A second field on the clip row shows the equipped grenade/mine and its count ("Molotov 2"), read from the
  player's inventory (thrown weapon with an equipped stack). Note: `TESObjectWEAP::IsThrownWeapon()` in
  CommonLibF4 treats the weapon type as a bitmask (guns match too), so the type is compared directly.
- Every 250 ms a UI task reads `Actor::GetCurrentAmmo` for the player and updates the text when it changes.
- `AmmoTypeHUD.ini` (size, positions, width, words stripped from names, grenade line on/off) is reloaded when it changes.

## Changelog

- 1.1.0: equipped grenade/mine name and count on the line above the ammo type.
- 1.0.0: first release.
- Log: `Documents\My Games\Fallout4\F4SE\AmmoTypeHUD.log`.

## Building

Requirements: Visual Studio 2022 (or Build Tools) with C++, and [xmake](https://xmake.io).

```bat
git clone --recurse-submodules <this repository>
cd ammo-type-hud
xmake f -y -m releasedbg
xmake build -y
```

Output: `build\windows\x64\releasedbg\AmmoTypeHUD.dll`. Ship it with `AmmoTypeHUD.ini` in `Data\F4SE\Plugins\`.

## License

GPL-3.0 with the exceptions in `EXCEPTIONS`, as CommonLibF4 and the
[libxse plugin template](https://github.com/libxse/commonlibf4-template) it is based on.

Written with the help of an AI assistant (Claude), built and tested in game by jnappo.
