/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#include <map>

#include "dynrpg_easyrpg.h"
#include "main_data.h"
#include "game_variables.h"
#include "game_party.h"
#include "game_enemyparty.h"
#include "game_actor.h"
#include "game_enemy.h"
#include "game_screen.h"
#include "game_system.h"
#include "utils.h"
#include "version.h"

static bool EasyOput(dyn_arg_list args) {
	auto func = "output";
	bool okay = false;
	std::string mode;
	std::tie(mode, std::ignore) = DynRpg::ParseArgs<std::string, std::string>(func, args, &okay);
	if (!okay)
		return true;
	mode = Utils::LowerCase(mode);

	auto msg = DynRpg::ParseVarArg(func, args, 1, okay);

	if (mode == "debug") {
		Output::DebugStr(msg);
	} else if (mode == "info") {
		Output::InfoStr(msg);
	} else if (mode == "warning") {
		Output::WarningStr(msg);
	} else if (mode == "error") {
		Output::ErrorStr(msg);
	}

	return true;
}

bool DynRpg::EasyRpgPlugin::EasyCall(dyn_arg_list args, bool& do_yield, Game_Interpreter* interpreter) {
	auto func_name = std::get<0>(DynRpg::ParseArgs<std::string>("call", args));

	if (func_name.empty()) {
		// empty function name
		Output::Warning("call: Empty RPGSS function name");

		return true;
	}

	for (auto& plugin: instance.plugins) {
		if (plugin->Invoke(func_name, args.subspan(1), do_yield, interpreter)) {
			return true;
		}
	}

	return false;
}

static bool EasyAdd(dyn_arg_list args) {
	auto func = "easyrpg_add";
	bool okay = false;

	int target_var;
	int val;
	std::tie(target_var, val) = DynRpg::ParseArgs<int, int>(func, args, &okay);
	if (!okay)
		return true;

	for (size_t i = 2; i < args.size(); ++i) {
		val += std::get<0>(DynRpg::ParseArgs<int>(func, args.subspan(i), &okay));
		if (!okay)
			return true;
	}

	Main_Data::game_variables->Set(target_var, val);

	return true;
}

bool DynRpg::EasyRpgPlugin::Invoke(std::string_view func, dyn_arg_list args, bool& do_yield, Game_Interpreter* interpreter) {
	if (func == "call") {
		return EasyCall(args, do_yield, interpreter);
	} else if (func == "easyrpg_output") {
		return EasyOput(args);
	} else if (func == "easyrpg_add") {
		return EasyAdd(args);
	}
    else if (func == "easyrpg_mode7_set_slant") {
		auto func_name = "easyrpg_mode7_set_slant";
		bool okay = false;
		int slant_value;
		std::tie(slant_value) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::SetMode7Slant(slant_value);
		return true;
	}

	else if (func == "easyrpg_mode7_set_yaw") {
		auto func_name = "easyrpg_mode7_set_yaw";
		bool okay = false;
		int yaw_value;
		std::tie(yaw_value) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::SetMode7Yaw(yaw_value);
		return true;
	}

	else if (func == "easyrpg_mode7_tilt") {
		auto func_name = "easyrpg_mode7_tilt";
		bool okay = false;
		int tilt_value;
		std::tie(tilt_value) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::TiltMode7(tilt_value);
		return true;
	}

	else if (func == "easyrpg_mode7_rotate") {
		auto func_name = "easyrpg_mode7_rotate";
		bool okay = false;
		int rotate_value;
		std::tie(rotate_value) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::RotateMode7(rotate_value);
		return true;
	}

	else if (func == "easyrpg_mode7_set_horizon") {
		auto func_name = "easyrpg_mode7_set_horizon";
		bool okay = false;
		int horizon_value;
		std::tie(horizon_value) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::SetMode7Horizon(horizon_value);
		return true;
	}

	else if (func == "easyrpg_mode7_set_zoom") { // This doesn't seem to do anything
		auto func_name = "easyrpg_mode7_set_zoom";
		bool okay = false;
		int zoom_value;
		std::tie(zoom_value) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::SetMode7Zoom(zoom_value);
		return true;
	}

    else if (func == "easyrpg_mode7_set_scale") {
		auto func_name = "easyrpg_mode7_set_scale";
		bool okay = false;
		int scale_value;
		std::tie(scale_value) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::SetMode7Scale(scale_value);
		return true;
	}

	// --- Timed Camera Movement ---

	else if (func == "easyrpg_mode7_tilt_towards") {
		auto func_name = "easyrpg_mode7_tilt_towards";
		bool okay = false;
		int tilt_value, duration;
		std::tie(tilt_value, duration) = DynRpg::ParseArgs<int, int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::TiltTowardsMode7(tilt_value, duration);
		return true;
	}

	else if (func == "easyrpg_mode7_rotate_towards") {
		auto func_name = "easyrpg_mode7_rotate_towards";
		bool okay = false;
		int rotate_value, duration;
		std::tie(rotate_value, duration) = DynRpg::ParseArgs<int, int>(func_name, args, &okay);
		if (!okay) return true;
		Game_Map::RotateTowardsMode7(rotate_value, duration);
		return true;
	}
    else if (func == "easyrpg_mode7_get_slant") {
		auto func_name = "easyrpg_mode7_get_slant";
		bool okay = false;
		int variable_id;
		std::tie(variable_id) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;

		float slant_value = Game_Map::GetMode7Slant();
		Main_Data::game_variables->Set(variable_id, static_cast<int>(slant_value * 100.0f));
		return true;
	}

	else if (func == "easyrpg_mode7_get_yaw") {
		auto func_name = "easyrpg_mode7_get_yaw";
		bool okay = false;
		int variable_id;
		std::tie(variable_id) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;

		float yaw_value = Game_Map::GetMode7Yaw();
		Main_Data::game_variables->Set(variable_id, static_cast<int>(yaw_value * 100.0f));
		return true;
	}

	else if (func == "easyrpg_mode7_get_horizon") {
		auto func_name = "easyrpg_mode7_get_horizon";
		bool okay = false;
		int variable_id;
		std::tie(variable_id) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;

		int horizon_value = Game_Map::GetMode7Horizon();
		Main_Data::game_variables->Set(variable_id, horizon_value);
		return true;
	}

	else if (func == "easyrpg_mode7_get_baseline") {
		auto func_name = "easyrpg_mode7_get_baseline";
		bool okay = false;
		int variable_id;
		std::tie(variable_id) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;

		int baseline_value = Game_Map::GetMode7Baseline();
		Main_Data::game_variables->Set(variable_id, baseline_value);
		return true;
	}

	else if (func == "easyrpg_mode7_get_scale") {
		auto func_name = "easyrpg_mode7_get_scale";
		bool okay = false;
		int variable_id;
		std::tie(variable_id) = DynRpg::ParseArgs<int>(func_name, args, &okay);
		if (!okay) return true;

		double scale_value = Game_Map::GetMode7Scale();
		Main_Data::game_variables->Set(variable_id, static_cast<int>(scale_value * 100.0));
		return true;
	}

else if (func == "easyrpg_mode7_set_fade") {
    bool okay = false;
    int pixels;
    std::tie(pixels) = DynRpg::ParseArgs<int>("easyrpg_mode7_set_fade", args, &okay);
    if (!okay) return true;

    Game_Map::SetMode7FadeWidth(pixels);
    return true;
}

else if (func == "easyrpg_mode7_overlay") {
    bool okay = false;
    int slot, y_off;
    float anchor, scroll;
    std::string name;
    // Format: ID, Filename, Anchor(0-100), Y-Offset, ScrollRatio(0-200)
    std::tie(slot, name, anchor, y_off, scroll) =
        DynRpg::ParseArgs<int, std::string, float, int, float>("easyrpg_mode7_overlay", args, &okay);

    if (okay) {
        // Convert integer-style percentages (0-100) to multipliers (0.0-1.0)
        Game_Map::SetMode7Overlay(slot, name, anchor / 100.0f, y_off, scroll / 100.0f);
    }
    return true;
}


	  // START: Mode7 Toggle Commands
    else if (func == "easyrpg_mode7_on") {
        Game_Map::SetIsMode7(true);
        return true;
    }

    else if (func == "easyrpg_mode7_off") {
        Game_Map::SetIsMode7(false);
        return true;
    }

    else if (func == "easyrpg_event_zoom") {
        auto func_name = "easyrpg_event_zoom";
        bool okay = false;
        int char_id, zoom_percent;

        std::tie(char_id, zoom_percent) = DynRpg::ParseArgs<int, int>(func_name, args, &okay);
        if (!okay) return true;

        // GetCharacter handles: 0 = This Event, -1 = Player, >0 = Event ID
        Game_Character* target = interpreter->GetCharacter(char_id, func_name);
        if (target) {
            target->SetCustomZoom(zoom_percent / 100.0f);
        }
        return true;
    }

    else if (func == "easyrpg_battler_zoom") {
        auto func_name = "easyrpg_battler_zoom";
        bool okay = false;
        int type, id, percent;

        std::tie(type, id, percent) = DynRpg::ParseArgs<int, int, int>(func_name, args, &okay);
        if (!okay) return true;

        Game_Battler* target = nullptr;
        if (type == 0) {
        // Party Slot 1-4. GetActor(0) is the first member.
        target = Main_Data::game_party->GetActor(id - 1);
        } else {
        // Enemy Index 1-8. GetEnemy(0) is the first enemy.
        target = Main_Data::game_enemyparty->GetEnemy(id - 1);
        }

        if (target) {
            target->SetCustomZoom(static_cast<float>(percent) / 100.0f);
        } else {
            Output::Warning("Battler Zoom: Target type {} ID {} not found", type, id);
        }
        return true;
    }
    else if (func == "easyrpg_battler_hide") {
        auto func_name = "easyrpg_battler_hide";
        bool okay = false;
        int type, id, hide_state;

    // Arguments: Type (0 for Actor, 1 for Enemy), ID (1-8), State (1 to Hide, 0 to Show)
        std::tie(type, id, hide_state) = DynRpg::ParseArgs<int, int, int>(func_name, args, &okay);
        if (!okay) return true;

        Game_Battler* target = nullptr;
        if (type == 0) {
            target = Main_Data::game_party->GetActor(id - 1);
        } else {
            target = Main_Data::game_enemyparty->GetEnemy(id - 1);
        }

        if (target) {
            target->SetProxyHidden(hide_state != 0);
        } else {
            Output::Warning("Battler Hide: Target type {} ID {} not found", type, id);
        }
        return true;
        }

    else if (func == "easyrpg_battler_opacity") {
        auto func_name = "easyrpg_battler_opacity";
        bool okay = false;
        int type, id, alpha;

    // Arguments: Type (0=Actor, 1=Enemy), ID (1-8), Opacity (0-255)
        std::tie(type, id, alpha) = DynRpg::ParseArgs<int, int, int>(func_name, args, &okay);
        if (!okay) return true;

        Game_Battler* target = nullptr;
        if (type == 0) {
            target = Main_Data::game_party->GetActor(id - 1);
        } else {
            target = Main_Data::game_enemyparty->GetEnemy(id - 1);
        }

        if (target) {
            target->SetCustomOpacity(std::clamp(alpha, 0, 255));
        }
        return true;
        }
    else if (func == "easyrpg_battler_angle") {
        auto func_name = "easyrpg_battler_angle";
        bool okay = false;
        int type, id, angle;

    // Arguments: Type (0=Actor, 1=Enemy), ID (1-8), Angle (0-360)
        std::tie(type, id, angle) = DynRpg::ParseArgs<int, int, int>(func_name, args, &okay);
        if (!okay) return true;

        Game_Battler* target = nullptr;
        if (type == 0) {
            target = Main_Data::game_party->GetActor(id - 1);
        } else {
            target = Main_Data::game_enemyparty->GetEnemy(id - 1);
        }

        if (target) {
            target->SetCustomAngle(static_cast<float>(angle));
        }
        return true;
        }
    else if (func == "easyrpg_set_anim_zoom") {
		bool okay = false;
		int zoom_percent;
		std::tie(zoom_percent) = DynRpg::ParseArgs<int>("easyrpg_set_anim_zoom", args, &okay);
		if (!okay) return true; // Error already printed by ParseArgs

		if (zoom_percent < 0) {
			Output::Warning("easyrpg_set_anim_zoom: Zoom percentage cannot be negative.");
			return true;
		}

		double zoom_factor = static_cast<double>(zoom_percent) / 100.0;
		Main_Data::game_system->SetNextBattleAnimationZoom(zoom_factor);
		return true;
	}


return false;
} //
    // ... (rest of your Mode7 setters/getters) ...


void DynRpg::EasyRpgPlugin::Load(const std::vector<uint8_t>& buffer) {
	if (buffer.size() < 4) {
		Output::Warning("EasyRpgPlugin: Bad savegame data");
	} else {
		uint32_t ver;
		memcpy(&ver, buffer.data(), 4);
		Utils::SwapByteOrder(ver);
		Output::Debug("DynRpg Savegame version {}", ver);
	}
}

std::vector<uint8_t> DynRpg::EasyRpgPlugin::Save() {
	std::vector<uint8_t> save_data;
	save_data.resize(4);

	uint32_t version = PLAYER_SAVEGAME_VERSION;
	Utils::SwapByteOrder(version);
	memcpy(&save_data[0], reinterpret_cast<char*>(&version), 4);

	return save_data;
}
