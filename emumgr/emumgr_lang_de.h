/*
 * emumgr - emulation manager for GWENESIS on the Pico Held
 *
 * Copyright (C) 2023 Daniel Kammer (daniel.kammer@web.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#ifndef EMUMGR_LANG_DE_H
#define EMUMGR_LANG_DE_H

// Escape Code für Umlaute
// ä e4, Ä c4
// Ü dc, ü fc
// Ö d6, ö f6
// ß df

#define STR_MAIN_TITLE "Pico Held Snes9x"
#define STR_OPTIONS_MENU "Einstellungen"

#define STR_HIGH "hoch"
#define STR_LOW "niedrig"
#define STR_ERROR "Fehler"
#define STR_FPS "Zeige FPS"
#define STR_ENABLED "aktiviert"
#define STR_DISABLED "deaktiviert"
#define STR_ROM_TOO_BIG "ROM ist zu gro\xdf"
#define STR_READING_ROM "beim Lesen der SD-Karte"
#define STR_LOADING "Laden..."
#define STR_GO_BACK "\x3c""  Zur""\xfc""ck"
#define STR_BACK "Zur""\xfc""ck"
#define STR_GAMESTATE_SAVED "Spielstand gesichert"
#define STR_ERROR "Fehler"
#define STR_ERR_OPEN_FILE "Datei kann nicht""\0""ge""\xf6""ffnet werden"
#define STR_ERR_READ_FILE "Datei kann nicht""\0""gelesen werden"
#define STR_ERR_WRITE_FILE "Datei kann nicht""\0""geschrieben werden"
#define STR_ERR_PSRAM "Nicht gen""\xfc""egend PSRAM"
#define STR_ERR_ROM "Beim Laden des ROMS"
#define STR_ERR_OPEN_DIR "Beim Oeffnen des Ordners"
#define STR_LOAD_GAME_FIRST "Bitte zuerst ein""\0"" Spiel w""\xe4""hlen"
#define STR_ERR_ABORTED "Abgebrochen"
#define STR_ERR_SD_MOUNT "Beim Lesen der SD-Karte"

#define STR_OFF "Aus"
#define STR_YES "JA"
#define STR_NO "NEIN"
#define STR_CONFIRM_OVERWRITE "\xdc""berschreiben best""\xe4""tigen"
#define STR_CONFIRM_BL "Bootloader starten best\xe4tigen"

#define STR_SLOT_1 "Speicherplatz 1"
#define STR_SLOT_2 "Speicherplatz 2"
#define STR_SLOT_3 "Speicherplatz 3"
#define STR_SLOT_4 "Speicherplatz 4"
#define STR_SLOT_5 "Speicherplatz 5"
#define STR_SLOT_6 "Speicherplatz 6"
#define STR_SAVE "Spielstand sichern"
#define STR_SAVE_STATE "Spielstand sichern"
#define STR_SAVING_GAME_STATE "Sichere Spielstand"
#define STR_PLEASE_WAIT "Bitte warten..."
#define STR_LOAD "Spielstand laden"
#define STR_LOAD_STATE "Spielstand laden"
#define STR_SEL_ROM "Spiel w\xe4hlen"
#define STR_SOUND "Lautst\xe4rke"
#define STR_BRIGHTNESS "Helligkeit"
#define STR_OPTIONS "Einstellungen"
#define STR_LAUNCH "Spiel starten"
#define STR_SHUTDOWN "Ausschalten"

#define STR_100 "100 %"
#define STR_90 "90 %"
#define STR_80 "80 %"
#define STR_70 "70 %"
#define STR_60 "60 %"
#define STR_50 "50 %"
#define STR_40 "40 %"
#define STR_30 "30 %"
#define STR_20 "20 %"
#define STR_10 "10 %"
#define STR_0 "0 %"

#define STR_BTN_MENU "Knopfreihenfolge"
#define STR_BTN_ABC "A-B-C"
#define STR_BTN_ACB "A-C-B"
#define STR_BTN_BAC "B-A-C"
#define STR_BTN_BCA "B-C-A"
#define STR_BTN_CAB "C-A-B"
#define STR_BTN_CBA "C-B-A"

#define STR_BUTTONS "Kn\xf6pfe"
#define STR_TOGGLE_SND_ACC "Genauigkeit umschalten"
#define STR_TOGGLE_FPS "FPS-Anzeige umschalten"
#define STR_FPS "Zeige FPS"
#define STR_TOGGLE_Z80 "Z80 erwzingen (Experte)"
#define STR_Z80 "Z80 erzwingen"

#define STR_LAUNCH_BL "Bootloader starten"
#endif // EMUMGR_LANG_DE_H